#include "pch.hpp"
#include "Listener.hpp"

static void make_socket_non_blocking(int sfd)
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
		AF_UNSPEC,		// family - Return IPv4 and IPv6 choices
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

		s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
		if (!s)
		{
			/* We managed to bind successfully! */
			break;
		}

		close(sfd);
	}

	if (!rp)
	{
		throw std::runtime_error("Could not bind");
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




SocketStream::SocketStream(int fd)
	: _fd(fd)
{
	printf("Started socket stream %d\n", fd);
}

SocketStream::~SocketStream()
{
	printf("Destroying socket stream for %d\n", _fd);
	close();
}

void SocketStream::receive(void* data, int length)
{
}

void SocketStream::ready()
{
	printf("Received write ready signal\n", _fd);
	flush_pending_writes();
}

int SocketStream::send(void* data, int length)
{
	if (!valid())
	{
		throw std::runtime_error("cannot send because the socket has been closed.");
	}

	// make sure there are no pending writes
	flush_pending_writes();

	printf("writing %d bytes\n", length);
	auto r = write(_fd, data, length);
	if (r == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			printf("nothing written, would block\n");
		}
		else
		{
			printf("socket write error\n");
			// TODO: what now?
			close();
			throw std::runtime_error("socket error");
		}
	}
	else
	{
		printf("wrote %d bytes\n", length);
	}

	return static_cast<int>(r);
}

void SocketStream::close()
{
	if (_fd > 0)
	{
		printf("Closing socket stream %d\n", _fd);
		::close(_fd);
		_fd = 0;
	}
}
ssize_t SocketStream::buffered_send(void* data, size_t length)
{
	if (!valid())
	{
		throw std::runtime_error("cannot send because the socket has been closed.");
	}

	// make sure there are no pending writes
	flush_pending_writes();

	char* p0 = static_cast<char*>(data);
	char* p = p0;
	auto bytes_left = length;

	printf("going to buffered send %d bytes\n", length);
	while (bytes_left > 0)
	{
		printf("writing %d bytes\n", bytes_left);
		auto written = write(_fd, p, bytes_left);
		if (written > 0)
		{
			printf("wrote %d bytes\n", written);
			p += written;
			bytes_left -= written;
		}
		else if (written < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				printf("nothing written, would block\n");
				// write would block, append to pending buffer
				auto pending_buffer_size = _pending_writes.size();
				_pending_writes.resize(pending_buffer_size + bytes_left);
				char* dst = &_pending_writes[pending_buffer_size];
				memcpy(dst, p, bytes_left);

				printf("%d bytes will be written when possible\n", bytes_left);
				break;
			}
			else
			{
				printf("socket write error\n");
				// TODO: what now?
				close();
				throw std::runtime_error("socket error");
			}
		}
	}

	return static_cast<ssize_t>(p - p0);
}

ssize_t SocketStream::flush_pending_writes()
{
	if (!valid()) return -1;

	char* p0 = &_pending_writes[0];
	char* p = p0;
	auto bytes_left = _pending_writes.size();

	while (bytes_left > 0)
	{
		printf("flushing write buffer\n");
		printf("(flush) writing %d bytes\n", bytes_left);
		auto written = write(_fd, p, bytes_left);
		if (written > 0)
		{
			printf("(flush) wrote %d bytes\n", written);
			p += written;
		}
		else if (written < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				printf("(flush) nothing written, would block\n");
				// write would block, we're done here
				break;
			}
			else
			{
				printf("(flush) socket write error\n");
				// TODO: what now?
				close();
				throw std::runtime_error("socket error");
			}
		}
	}

	if (bytes_left > 0)
	{
		// realling the buffer
		memcpy(p0, p, bytes_left);
		_pending_writes.resize(bytes_left);

		printf("(flush) %d bytes will be written when possible\n", bytes_left);
	}

	return static_cast<ssize_t>(p - p0);
}




Acceptor::Acceptor(int a)
	: _efd(epoll_create1(0)),
	_running(true),
	_thread([this]() { worker(); })
{
	if (_efd == -1) throw system_err();
	printf("Created acceptor\n");
}

Acceptor::~Acceptor()
{
	printf("Destroying acceptor\n");
	stop();
}

void Acceptor::accept(SocketStream* stream)
{
	auto sfd = stream->sfd();
	_sockets[sfd] = stream;
	epoll_add(_efd, sfd, stream, EPOLLIN | EPOLLET | EPOLLOUT);
}

void Acceptor::worker()
{
	printf("started acceptor worker\n");
	std::array<epoll_event, 16> events;
	while (_running)
	{
		int numEvents = epoll_wait(_efd, &events[0], events.size(), 1000);
		for (int i = 0; i < numEvents && _running; i++)
		{
			const auto &event = events[i];
			auto stream = reinterpret_cast<SocketStream*>(event.data.ptr);
			auto sfd = stream->sfd();
			bool eof = false;
			auto eventFlags = event.events;

			if (eventFlags & EPOLLIN)
			{
				// this is incoming data
				while (1)
				{
					ssize_t count;
					char buf[4096];

					count = read(sfd, buf, sizeof(buf));
					if (count == -1)
					{
						/* If errno == EAGAIN, that means we have read all
							data. So go back to the main loop. */
						if (errno == EAGAIN)
						{
							break;
						}
						else
						{
							perror("read");
							eof = true;
						}
					}
					else if (count == 0)
					{
						/* End of file. The remote has closed the
							connection. */
						stream->receive(nullptr, 0);
						eof = true;
						break;
					}
					else
					{
						// send the data on
						stream->receive(buf, static_cast<int>(count));
					}
				}
			}
			else if (eventFlags & EPOLLOUT)
			{
				stream->ready();
			}
			else
			{
				/* An error has occured on this fd, or the socket is not
					ready for reading (why were we notified then?) */
				fprintf(stderr, "epoll error\n");
				eof = true;
			}


			if (eof)
			{
				printf("Closed connection on descriptor %d\n", sfd);

				stream->close();
				delete stream;
				_sockets.erase(sfd);
			}
		}
	}

	printf("exit acceptor worker\n");
}

void Acceptor::stop()
{
	printf("stopping acceptor\n");

	_running = false;

	if (_thread.joinable()) _thread.join();

	if (_efd) { close(_efd); _efd = 0; }

	printf("stopped acceptor\n");
}




Listener::Listener(const char* address, int port, std::function<SocketStream*(int)> streamFactory)
	: _efd(initialize_epoll()),
	_sfd(initialize_socket(address, port)),
	_thread([this]() {worker(); }),
	_acceptors(initialize_acceptors()),
	_streamFactory(streamFactory)
{
	printf("created listener for %s:%d\n", address ? address : "<null>", port);
}

Listener::~Listener()
{
	printf("destroying listener\n");
	stop();
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
	make_socket_non_blocking(sfd);
	if (listen(sfd, SOMAXCONN) == -1) throw system_err();
	epoll_add(_efd, sfd);
	return sfd;
}

std::vector<Acceptor> Listener::initialize_acceptors()
{
	int n = 1; // std::thread::hardware_concurrency();
	std::vector<Acceptor> result;
	result.reserve(n);
	for (int i = 0; i < n; i++)
	{
		result.emplace_back(1);
	}

	return result;
}

void Listener::worker()
{
	printf("starting listener worker\n");

	std::array<epoll_event, 16> events;
	while (_running)
	{
		auto numEvents = epoll_wait(_efd, &events[0], events.size(), 1000);
		for (int i = 0; i < numEvents && _running; i++)
		{
			const auto &event = events[i];
			const auto sfd = event.data.fd;
			auto eventFlags = event.events;

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
						printf("Accepted connection on descriptor %d "
							"(host=%s, port=%s)\n", infd, hbuf, sbuf);
					}

					// Make the incoming socket non-blocking and add it to the list of fds to monitor.
					make_socket_non_blocking(infd);
					Acceptor& acceptor = _acceptors[_counter++ % _acceptors.size()];
					auto stream = _streamFactory(infd);
					acceptor.accept(stream);
				}
			}
			else
			{
				printf("error with listener worker - stopping\n");
				stop();
				return;
			}
		}
	}

	printf("listener worker exit\n");
}

void Listener::stop()
{
	printf("stopping listener\n");
	_running = false;
	if (_sfd > 0) { close(_sfd); _sfd = 0; }

	if (_thread.joinable()) _thread.join();

	if (_efd > 0) { close(_efd); _efd = 0; }
	printf("stopped listener\n");
}
