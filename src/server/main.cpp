#include "pch.hpp"
#include "Listener.hpp"
#include "Tls.hpp"
#include "Http.hpp"

// telnet localhost 9080
// openssl s_client -connect localhost:9443 -servername localtest.me

std::vector<Listener*> listeners;

void sig_handler(int signum)
{
	printf("got SIGINT\n");
	if (listeners.size())
	{
		printf("stopping the thing\n");
		for (auto l : listeners) l->stop();
	}
}

int main(int argc, char *argv[])
{
	rlimit l;
	if (prlimit(getpid(), RLIMIT_NOFILE, nullptr, &l) == 0)
	{
		if (l.rlim_cur < l.rlim_max)
		{
			l.rlim_cur = l.rlim_max;
			if (prlimit(getpid(), RLIMIT_NOFILE, &l, nullptr) < 0)
			{
				printf("warning: could not increase file limit.\n");
				perror("prlimit");
			}
		}
	}
	else
	{
		printf("warning: could not get file limit.\n");
		perror("prlimit");
	}


	std::shared_ptr<Hosting> static_hosting = std::make_shared<StaticHosting>("./rabbiteer.io");
	Tls tls;
	HttpServer http{ static_hosting };

	tls.add_certificate("localhost.cer", "localhost.key");
	tls.add_certificate("localtest.cer", "localtest.key");

	printf("starting listener\n");
	Listener l1(nullptr, 443, [&tls,&http](std::shared_ptr<Socket> socket, std::shared_ptr<SocketEventProducer> events)
	{
		auto tls_socket = std::make_shared<TlsSocket>(socket, tls);
		events->connect(tls_socket);

		tls_socket->connect(std::make_shared<HttpHandler>(http, tls_socket));
	});

	Listener l2(nullptr, 80, [&tls, &http](std::shared_ptr<Socket> socket, std::shared_ptr<SocketEventProducer> events)
	{
		auto handler = std::make_shared<HttpHandler>(http, socket);
		events->connect(handler);
	});

	listeners.push_back(&l1);
	listeners.push_back(&l2);
	signal(SIGINT, sig_handler);
	l1.wait();
	l2.wait();
	listeners.clear();

	return 0;
}
