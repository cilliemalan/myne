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
	std::shared_ptr<Hosting> static_hosting = std::make_shared<StaticHosting>("./rabbiteer.io");
	Tls tls;
	HttpServer http{ static_hosting };

	tls.add_certificate("localhost.cer", "localhost.key");
	tls.add_certificate("localtest.cer", "localtest.key");

	printf("starting listener\n");
	Listener l1(nullptr, 443, [&tls,&http](std::shared_ptr<ComboSocket> producer)
	{
		auto tls_socket = std::make_shared<TlsComboSocket>(producer, tls);
		producer->connect(tls_socket);

		tls_socket->connect(std::make_shared<HttpHandler>(http, tls_socket));
	});

	Listener l2(nullptr, 80, [&tls, &http](std::shared_ptr<ComboSocket> producer)
	{
		producer->connect(std::make_shared<HttpHandler>(http, producer));
	});

	listeners.push_back(&l1);
	listeners.push_back(&l2);
	signal(SIGINT, sig_handler);
	l1.wait();
	l2.wait();
	listeners.clear();

	return 0;
}
