#pragma once
#include "HttpParser.hpp"
#include "Hosting.hpp"


class HttpServer
{
	using iterator = std::vector<std::shared_ptr<Hosting>>::const_iterator;
public:
	HttpServer(std::initializer_list<std::shared_ptr<Hosting>> hostings) :_hostings(hostings) {}

	inline iterator begin() const { return _hostings.cbegin(); }
	inline iterator end() const { return _hostings.cend(); }
private:
	std::vector<std::shared_ptr<Hosting>> _hostings;
};

class HttpHandler : public SocketEventReceiver
{
public:
	HttpHandler(HttpServer &http, std::shared_ptr<Socket> socket);

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

	request_info request;

	struct _response
	{
		_response(std::vector<char> headers, const char* body, size_t body_size)
			:headers(headers), body(body), body_size(body_size),
			headers_written(0), body_written(0)
		{}

		std::vector<char> headers;
		const char* body;
		size_t body_size;
		size_t headers_written;
		size_t body_written;
	};

	std::vector<_response> pending_responses;

	bool _done;
	HttpServer &_http;
	std::shared_ptr<Socket> _socket;
	HttpParser _parser;
};

class Http2Handler : public SocketEventReceiver
{
public:
	Http2Handler(HttpServer &http, std::shared_ptr<Socket> socket);

	virtual ~Http2Handler() {}

	virtual void read_avail();
	virtual void write_avail();
	virtual void closed();
private:
	HttpServer &_http;
	std::shared_ptr<Socket> _socket;
};