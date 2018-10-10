#include "pch.hpp"
#include "Listener.hpp"

// helper funcs

static void make_non_blocking(int sfd)
{
	int flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1) throw system_err();

	flags |= O_NONBLOCK;
	if (fcntl(sfd, F_SETFL, flags) == -1) throw system_err();
}

static int create_and_bind(const char* address, int port)
{
	// convert the port
	char sPort[20];
	int sPortLen = sprintf(sPort, "%d", port);
	if (sPortLen <= 0) throw std::runtime_error("invalid port");

	// get addess information
	addrinfo hints
	{
		AI_PASSIVE,		// flags - All interfaces
		AF_INET,		// family - Return IPv4 choices
		SOCK_STREAM,	// socktype - We want a TCP socket
		IPPROTO_TCP,	// protocol - We want TCP
		0,				// addrlen
		nullptr,		// addr
		nullptr,		// canonname
		nullptr,		// next
	};

	addrinfo *result;
	int s = getaddrinfo(address, sPort, &hints, &result);
	if (s) throw std::runtime_error(std::string("error getting address information. ") + gai_strerror(s));

	// try to bind a socket to a viable address
	addrinfo *rp;
	int sfd;
	for (rp = result; rp; rp = rp->ai_next)
	{
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
		{
			continue;
		}

		// reuse address if needed
		int yes = 1;
		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
		{
			perror("setsockopt");
			close(sfd);
			continue;
		}
		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) < 0)
		{
			perror("setsockopt");
			close(sfd);
			continue;
		}
		if (setsockopt(sfd, 6, TCP_NODELAY, &yes, sizeof(yes)) < 0)
		{
			perror("setsockopt");
			close(sfd);
			continue;
		}

		s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
		if (s)
		{
			perror("bind");
			close(sfd);
			continue;
		}
		else
		{
			break;
		}
	}

	if (!rp)
	{
		throw system_err();
	}

	freeaddrinfo(result);

	return sfd;
}

void epoll_add(int efd, int sfd, void* data = nullptr, uint32_t events = EPOLLIN | EPOLLET)
{
	epoll_event ev;
	ev.events = events;
	if (data)
	{
		ev.data.ptr = data;
	}
	else
	{
		ev.data.fd = sfd;
	}
	if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &ev) == -1) throw system_err();
}


// ComboSocket

ssize_t ComboSocket::read(void* b, size_t max)
{
	if (!_socket || !b || !max) return 0;

	ssize_t result = _socket->read(b, max);

	if (result == 0)
	{
		close();
		return 0;
	}

	return result;
}

ssize_t ComboSocket::write(const void* b, size_t amt)
{
	if (!_socket || !b || !amt) return 0;

	ssize_t result = _socket->write(b, amt);

	if (result == 0)
	{
		close();
		return 0;
	}

	return result;
}

void ComboSocket::close()
{
	if (_socket)
	{
		_socket->close();
		_socket.reset();
	}
}


void ComboSocket::read_avail()
{
	signal_read_avail();
}

void ComboSocket::write_avail()
{
	flush_pending_writes();

	if (_pending_writes.size() == 0)
	{
		signal_write_avail();
	}
}

void ComboSocket::closed()
{
	signal_closed();
}

ssize_t ComboSocket::buffered_write(const void* data, size_t length)
{
	if (!_socket) return -1;
	if (!length) return 0;

	// make sure there are no pending writes
	flush_pending_writes();

	const char* p0 = static_cast<const char*>(data);
	const char* p = p0;
	auto bytes_left = length;

	while (bytes_left > 0)
	{
		auto written = write(p, bytes_left);
		if (written > 0)
		{
			p += written;
			bytes_left -= written;
		}
		else if (written < 0)
		{
			// write would block, append to pending buffer
			auto pending_buffer_size = _pending_writes.size();
			_pending_writes.resize(pending_buffer_size + bytes_left);
			char* dst = &_pending_writes[pending_buffer_size];
			memcpy(dst, p, bytes_left);

			break;
		}
		else
		{
			// close the socket
			close();
			throw std::runtime_error("socket error");
		}
	}

	return static_cast<ssize_t>(p - p0);
}

ssize_t ComboSocket::flush_pending_writes()
{
	if (!_socket) return -1;

	char* p0 = &_pending_writes[0];
	char* p = p0;
	auto bytes_left = _pending_writes.size();

	while (bytes_left > 0)
	{
		auto written = write(p, bytes_left);
		if (written > 0)
		{
			p += written;
		}
		else if (written < 0)
		{
			// write would block, we're done here
			break;
		}
		else
		{
			// socket broken or closed
			close();
			signal_closed();
			throw std::runtime_error("socket error");
		}
	}

	if (p0 != p)
	{
		if (bytes_left > 0)
		{
			// realling the buffer
			memcpy(p0, p, bytes_left);
		}

		_pending_writes.resize(bytes_left);
		_pending_writes.shrink_to_fit();
	}

	return static_cast<ssize_t>(p - p0);
}

std::vector<char> ComboSocket::read_all()
{
	std::vector<char> data;

	bool eof;
	ssize_t count;
	do
	{
		char buf[4096];

		count = read(buf, sizeof(buf));
		if (count < 0)
		{
			// we have read all data.
		}
		else if (count == 0)
		{
			//End of file. (remote closed)
			eof = true;
		}
		else
		{
			data.insert(data.end(), buf, buf + count);
		}
	} while (count > 0);

	if (eof)
	{
		close();
	}

	return std::move(data);
}


//Acceptor

Acceptor::Acceptor(int nr)
	: _nr(nr),
	_efd(epoll_create1(0)),
	_running(true),
	_thread([this]() { worker(); })
{
	if (_efd == -1) throw system_err();

	// pipe for signaling
	if (pipe2(_pfd, O_NONBLOCK) == -1) throw system_err();
	epoll_add(_efd, _pfd[0]);
}

Acceptor::~Acceptor()
{
	stop();
}

std::shared_ptr<ComboSocket> Acceptor::accept(std::shared_ptr<LinuxSocket> socket, std::function<void(std::shared_ptr<ComboSocket>)> acceptHandler)
{
	auto fd = socket->fd();
	auto combo = std::make_shared<Acceptor::LocalComboSocket>(socket, *this, fd);
	acceptHandler(combo);
	_sockets.insert_or_assign(fd, combo);
	epoll_add(_efd, fd, nullptr, EPOLLIN | EPOLLET | EPOLLOUT | EPOLLHUP | EPOLLRDHUP);
	return combo;
}

void Acceptor::worker()
{
	std::array<epoll_event, 16> events;
	while (_running)
	{
		int numEvents = epoll_wait(_efd, &events[0], events.size(), -1);
		for (int i = 0; i < numEvents && _running; i++)
		{
			const auto &event = events[i];
			auto sfd = event.data.fd;

			// if data from _pfd then quit
			if (sfd == _pfd[0]) break;

			auto handleriterator = _sockets.find(sfd);
			if (handleriterator == _sockets.end())
			{
				// we got notified for a socket we don't have for some reason.
				continue;
			}

			auto eventReceiver = handleriterator->second;
			bool eof = false;
			auto eventFlags = event.events;


			if (eventFlags & (EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLRDHUP))
			{
				eof = true;
			}
			else
			{
				try
				{
					if (eventFlags & EPOLLIN)
					{
						eventReceiver->signal_read_avail();
					}

					if (eventFlags & EPOLLOUT)
					{
						eventReceiver->signal_write_avail();
					}
				}
				catch (std::runtime_error &e)
				{
					printf("Exception in handler: %s\n", e.what());
					eof = true;
				}
				catch (...)
				{
					printf("Unknown exception in handler\n");
					eof = true;
				}
			}

			if (eof)
			{
				printf("Closed connection on descriptor %d\n", sfd);

				try
				{
					eventReceiver->signal_closed();
				}
				catch (...) {}
				_sockets.erase(handleriterator);
			}
		}
	}
}

void Acceptor::stop()
{
	_running = false;

	if (_pfd[1]) { char a = 0; write(_pfd[1], &a, 1); }

	if (_thread.joinable()) _thread.join();

	if (_efd) { close(_efd); _efd = 0; }
	if (_pfd[0]) { close(_pfd[0]); _pfd[0] = 0; }
	if (_pfd[1]) { close(_pfd[1]); _pfd[1] = 0; }
}


// Listener

Listener::Listener(const char* address, int port, std::function<void(std::shared_ptr<ComboSocket>)> acceptHandler)
	: _efd(initialize_epoll()),
	_sfd(initialize_socket(address, port)),
	_thread([this]() {worker(); }),
	_acceptors(initialize_acceptors()),
	_acceptHandler(acceptHandler)
{
	if (_efd == -1) throw system_err();

	// pipe for signaling
	if (pipe2(_pfd, O_NONBLOCK) == -1) throw system_err();
	epoll_add(_efd, _pfd[0]);

	printf("created listener for %s:%d\n", address ? address : "<null>", port);
}

Listener::~Listener()
{
	stop();
	wait();
}

int Listener::initialize_epoll()
{
	int efd = epoll_create1(0);
	if (efd == -1) throw system_err();
	return efd;
}

int Listener::initialize_socket(const char* address, int port)
{
	int sfd = create_and_bind(address, port);
	make_non_blocking(sfd);
	if (listen(sfd, SOMAXCONN) == -1) throw system_err();
	epoll_add(_efd, sfd);
	return sfd;
}

std::vector<Acceptor> Listener::initialize_acceptors()
{
	int n = std::thread::hardware_concurrency();
	std::vector<Acceptor> result;
	result.reserve(n);
	for (int i = 0; i < n; i++)
	{
		result.emplace_back(i);
	}

	return result;
}

void Listener::worker()
{
	std::array<epoll_event, 16> events;
	while (_running)
	{
		auto numEvents = epoll_wait(_efd, &events[0], events.size(), -1);
		for (int i = 0; i < numEvents && _running; i++)
		{
			const auto &event = events[i];
			const auto sfd = event.data.fd;
			auto eventFlags = event.events;

			// if data from _pfd then quit
			if (sfd == _pfd[0]) break;

			if (eventFlags & EPOLLIN)
			{
				while (1)
				{
					sockaddr in_addr;
					socklen_t in_len = sizeof(sockaddr);
					auto infd = accept(sfd, &in_addr, &in_len);
					if (infd == -1)
					{
						if ((errno == EAGAIN) ||
							(errno == EWOULDBLOCK))
						{
							// We have processed all incoming connections.
							break;
						}
						else
						{
							// couldn't accept for some reason
							perror("accept");
							continue;
						}
					}

					// get info about the connection
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
					if (!getnameinfo(&in_addr, in_len,
						hbuf, sizeof hbuf,
						sbuf, sizeof sbuf,
						NI_NUMERICHOST | NI_NUMERICSERV))
					{
						//printf("Accepted connection on descriptor %d " "(host=%s, port=%s)\n", infd, hbuf, sbuf);
					}

					// Make the incoming socket non-blocking and forward to an acceptor
					make_non_blocking(infd);

					auto socket = std::make_shared<LinuxSocket>(infd);
					auto& acceptor = _acceptors[_counter++ % _acceptors.size()];
					auto combo = acceptor.accept(socket, _acceptHandler);
				}
			}
			else
			{
				_running = false;
				break;
			}
		}
	}

	if (_sfd > 0) { close(_sfd); _sfd = 0; }
	if (_efd > 0) { close(_efd); _efd = 0; }
	if (_pfd[0]) { close(_pfd[0]); _pfd[0] = 0; }
	if (_pfd[1]) { close(_pfd[1]); _pfd[1] = 0; }
}

void Listener::stop()
{
	_running = false;
	if (_pfd[1]) { char a = 0; write(_pfd[1], &a, 1); }
}

void Listener::wait()
{
	if (_thread.joinable()) _thread.join();
}