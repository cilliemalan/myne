#pragma once
#include "HttpParser.hpp"

class HttpServer
{
public:
	HttpServer() {}
};






class HttpHandler : public SocketEventReceiver
{
public:
	HttpHandler(HttpServer &http, std::shared_ptr<ComboSocket> socket);

	virtual ~HttpHandler() {}

	virtual void read_avail();
	virtual void write_avail();
	virtual void closed();

protected:
	virtual void process();

private:
	HttpParserCallbacks parser_callbacks();

	void on_url(std::string, std::string);
	void on_status(int, std::string);
	void on_header(std::string name, std::string value);
	void on_headers_complete();
	void on_body(const char* b, size_t l);
	void on_message_complete();


	std::string _method;
	std::string _url;

	// common headers
	std::string _host;

	bool _done;
	HttpServer &_http;
	std::shared_ptr<ComboSocket> _socket;
	HttpParser _parser;
};