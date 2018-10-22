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
	epoll_event ev{ 0, 0 };
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


// LinuxSocket

LinuxSocket::LinuxSocket(int fd)
	:_fd(fd)
{
}

ssize_t LinuxSocket::read(void* b, size_t max)
{
	if (!b || !max) return 0;

	ssize_t r = 0;
	while (r == 0) r = ::read(_fd, b, max);

	if (r <= 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return -1;
		}
		else
		{
			// socket broken
			perror("read");
			close();
			return 0;
		}
	}

	return r;
}

ssize_t LinuxSocket::write(const void* b, size_t amt)
{
	if (!b || !amt) return 0;

	ssize_t r = 0;
	while (r == 0) r = ::write(_fd, b, amt);

	if (r <= 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return -1;
		}
		else
		{
			// socket broken
			perror("write");
			close();
			return 0;
		}
	}

	return r;
}

void LinuxSocket::close()
{
	if (_fd)
	{
		::close(_fd);
		_fd = 0;
	}
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

void Acceptor::accept(std::shared_ptr<LinuxSocket> socket, socket_handler acceptHandler)
{
	auto fd = socket->fd();
	auto events = std::make_shared<Acceptor::LocalSocketEventProducer>();
	auto pass_socket = std::make_shared< Acceptor::LocalSocket>(socket, *this);
	acceptHandler(pass_socket, events);
	_sockets.insert_or_assign(fd, std::make_pair(events, socket));
	epoll_add(_efd, fd, nullptr, EPOLLIN | EPOLLET | EPOLLOUT | EPOLLHUP | EPOLLRDHUP);
}

void Acceptor::worker()
{
	constexpr int n = 1024;
	epoll_event events[n];

	while (_running)
	{
		int numEvents = epoll_wait(_efd, &events[0], n, -1);
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

			auto eventReceiver = handleriterator->second.first;
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
				try
				{
					handleriterator->second.second->close();
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

Listener::Listener(const char* address, int port, socket_handler acceptHandler)
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
	if (n <= 0) n = 1;
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
	constexpr int n = 1024;
	epoll_event events[n];

	while (_running)
	{
		auto numEvents = epoll_wait(_efd, &events[0], n, -1);
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
							break;
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
					acceptor.accept(socket, _acceptHandler);
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
