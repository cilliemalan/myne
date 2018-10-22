#include "pch.hpp"
#include "Listener.hpp"
#include "Tls.hpp"
#include "Http.hpp"

// telnet localhost 9080
// openssl s_client -connect localhost:9443 -servername localtest.me

std::vector<Listener*> listeners;

void sig_handler(int signum)
{
	info("got SIGINT\n");
	if (listeners.size())
	{
		for (auto l : listeners) l->stop();
	}
}

void maximize_fds()
{
	rlimit l;
	if (prlimit(getpid(), RLIMIT_NOFILE, nullptr, &l) == 0)
	{
		if (l.rlim_cur < l.rlim_max)
		{
			l.rlim_cur = l.rlim_max;
			if (prlimit(getpid(), RLIMIT_NOFILE, &l, nullptr) < 0)
			{
				warning("warning: could not increase file limit.\n");
				logpwarning("prlimit");
			}
		}
	}
	else
	{
		warning("warning: could not get file limit.\n");
		logpwarning("prlimit");
	}
}

int main(int argc, char *argv[])
{
	maximize_fds();

	std::shared_ptr<Hosting> static_hosting = std::make_shared<StaticHosting>("./rabbiteer.io");
	Tls tls;
	HttpServer http{ static_hosting };

	tls.add_certificate("localhost.cer", "localhost.key");
	tls.add_certificate("localtest.cer", "localtest.key");

	tls.add_handler([&http](std::shared_ptr<TlsSocket> sock) { return std::make_shared<HttpHandler>(http, sock); });
	tls.add_handler("http/1.1", [&http](std::shared_ptr<TlsSocket> sock) { return std::make_shared<HttpHandler>(http, sock); });
	tls.add_handler("h2", [&http](std::shared_ptr<TlsSocket> sock) { return std::make_shared<Http2Handler>(http, sock); });

	Listener l1(nullptr, 443, [&tls,&http](std::shared_ptr<Socket> socket, std::shared_ptr<SocketEventProducer> events)
	{
		auto tls_socket = std::make_shared<TlsSocket>(socket, tls);
		events->connect(tls_socket);
		tls_socket->set_shared_ptr(tls_socket);
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

