////https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/epoll-example.c
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netdb.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <sys/epoll.h>
//#include <errno.h>
//
//#include <array>
//
//#define MAXEVENTS 64
//#define HTTP_PORT 80
//#define HTTPS_PORT 443
//
//static int make_socket_non_blocking(int sfd)
//{
//	int flags, s;
//
//	flags = fcntl(sfd, F_GETFL, 0);
//	if (flags == -1)
//	{
//		perror("fcntl");
//		return -1;
//	}
//
//	flags |= O_NONBLOCK;
//	s = fcntl(sfd, F_SETFL, flags);
//	if (s == -1)
//	{
//		perror("fcntl");
//		return -1;
//	}
//
//	return 0;
//}
//
//static int create_and_bind(const char* address, int port)
//{
//	// convert the port
//	char sPort[20];
//	int sPortLen = sprintf(sPort, "%d", port);
//	if (sPortLen <= 0)
//	{
//		fprintf(stderr, "invalid port: %d\n", port);
//		return -1;
//	}
//
//	// get addess information
//	addrinfo hints
//	{
//		AI_PASSIVE,		// flags - All interfaces
//		AF_UNSPEC,		// family - Return IPv4 and IPv6 choices
//		SOCK_STREAM,	// socktype - We want a TCP socket
//		IPPROTO_TCP,	// protocol - We want TCP
//		0,				// addrlen
//		nullptr,		// addr
//		nullptr,		// canonname
//		nullptr,		// next
//	};
//
//	addrinfo *result;
//	int s = getaddrinfo(address, sPort, &hints, &result);
//	if (s)
//	{
//		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
//		return -1;
//	}
//
//	// try to bind a socket to a viable address
//	addrinfo *rp;
//	int sfd;
//	for (rp = result; rp; rp = rp->ai_next)
//	{
//		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
//		if (sfd == -1)
//		{
//			continue;
//		}
//
//		s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
//		if (!s)
//		{
//			/* We managed to bind successfully! */
//			break;
//		}
//
//		close(sfd);
//	}
//
//	if (rp == NULL)
//	{
//		fprintf(stderr, "Could not bind\n");
//		return -1;
//	}
//
//	freeaddrinfo(result);
//
//	return sfd;
//}
//
//int main(int argc, char *argv[])
//{
//	// bind to sockets
//	int http_sfd = create_and_bind(nullptr, HTTP_PORT);
//	if (http_sfd == -1) abort();
//
//	int https_sfd = create_and_bind(nullptr, HTTPS_PORT);
//	if (https_sfd == -1) abort();
//
//	if (make_socket_non_blocking(http_sfd) == -1) abort();
//	if (make_socket_non_blocking(https_sfd) == -1) abort();
//	printf("Listening on port 80 and port 443\n");
//
//	// listen
//	if (listen(http_sfd, SOMAXCONN) == -1 ||
//		listen(https_sfd, SOMAXCONN) == -1)
//	{
//		perror("listen");
//		abort();
//	}
//
//	// initialize epoll
//	int efd = epoll_create1(0);
//	if (efd == -1)
//	{
//		perror("epoll_create");
//		abort();
//	}
//
//	epoll_event ev;
//	ev.events = EPOLLIN | EPOLLET;
//	ev.data.fd = http_sfd;
//	if (epoll_ctl(efd, EPOLL_CTL_ADD, http_sfd, &ev) == -1)
//	{
//		perror("epoll_ctl");
//		abort();
//	}
//	ev.data.fd = https_sfd;
//	if (epoll_ctl(efd, EPOLL_CTL_ADD, https_sfd, &ev) == -1)
//	{
//		perror("epoll_ctl");
//		abort();
//	}
//
//	// Buffer where events are returned
//	std::array<epoll_event, MAXEVENTS> events;
//	while (1)
//	{
//		auto numEvents = epoll_wait(efd, &events[0], events.size(), -1);
//		for (int i = 0; i < numEvents; i++)
//		{
//			const auto &event = events[i];
//			const auto sfd = event.data.fd;
//			if ((event.events & EPOLLERR) ||
//				(event.events & EPOLLHUP) ||
//				(!(event.events & EPOLLIN)))
//			{
//				/* An error has occured on this fd, or the socket is not
//				   ready for reading (why were we notified then?) */
//				fprintf(stderr, "epoll error\n");
//				close(sfd);
//				continue;
//			}
//
//			else if (sfd == http_sfd || sfd == https_sfd)
//			{
//				// this is an accept
//				while (1)
//				{
//					sockaddr in_addr;
//					socklen_t in_len = sizeof(sockaddr);
//					auto infd = accept(sfd, &in_addr, &in_len);
//					if (infd == -1)
//					{
//						if ((errno == EAGAIN) ||
//							(errno == EWOULDBLOCK))
//						{
//							// We have processed all incoming connections.
//							break;
//						}
//						else
//						{
//							// couldn't accept for some reason
//							perror("accept");
//							continue;
//						}
//					}
//
//					// get info about the connection
//					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
//					if (!getnameinfo(&in_addr, in_len,
//						hbuf, sizeof hbuf,
//						sbuf, sizeof sbuf,
//						NI_NUMERICHOST | NI_NUMERICSERV))
//					{
//						printf("Accepted connection on descriptor %d "
//							"(host=%s, port=%s)\n", infd, hbuf, sbuf);
//					}
//
//					// Make the incoming socket non-blocking and add it to the list of fds to monitor.
//					if (make_socket_non_blocking(infd) == -1) abort();
//
//					epoll_event nev;
//					nev.data.fd = infd;
//					nev.events = EPOLLIN | EPOLLET;
//					if(epoll_ctl(efd, EPOLL_CTL_ADD, infd, &nev) == -1)
//					{
//						perror("epoll_ctl");
//						abort();
//					}
//				}
//			}
//			else
//			{
//				// this is incoming data
//				/* We have data on the fd waiting to be read. Read and
//				   display it. We must read whatever data is available
//				   completely, as we are running in edge-triggered mode
//				   and won't get a notification again for the same
//				   data. */
//				int done = 0;
//
//				while (1)
//				{
//					ssize_t count;
//					char buf[4096];
//
//					count = read(sfd, buf, sizeof buf);
//					if (count == -1)
//					{
//						/* If errno == EAGAIN, that means we have read all
//						   data. So go back to the main loop. */
//						if (errno != EAGAIN)
//						{
//							perror("read");
//							done = 1;
//						}
//						break;
//					}
//					else if (count == 0)
//					{
//						/* End of file. The remote has closed the
//						   connection. */
//						done = 1;
//						break;
//					}
//
//					/* Write the buffer to standard output */
//					if (write(STDOUT_FILENO, buf, count) == -1)
//					{
//						perror("write");
//						abort();
//					}
//				}
//
//				if (done)
//				{
//					printf("Closed connection on descriptor %d\n", sfd);
//
//					/* Closing the descriptor will make epoll remove it
//					   from the set of descriptors which are monitored. */
//					close(sfd);
//				}
//			}
//		}
//	}
//
//	close(http_sfd);
//	close(https_sfd);
//
//	return EXIT_SUCCESS;
//}



#include "pch.hpp"
#include "Listener.hpp"

int main(int argc, char *argv[])
{
	std::function<SocketStream*(int)> streamFactory = [](int fd) { return new FunctionalSocketStream(fd, 
		[](int fd, void* data, int length, std::function<int(void* data, int length)> write)
	{
		write(data, length);
	}); };

	printf("starting listener\n");
	Listener l(nullptr, 80, streamFactory);

	printf("press a to exit\n");
	while (getchar() != 'a');
	printf("exiting\n");
	return 0;
}