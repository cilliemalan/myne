#pragma once

struct HttpParserCallbacks
{
	std::function<void(std::string)> url;
	std::function<void(int, std::string)> status;
	std::function<void(std::string name, std::string value)> header;
	std::function<void()> headers_complete;
	std::function<void(const char* b, size_t l)> body;
	std::function<void()> message_complete;
};

enum class HttpParserType
{
	Request, Response, Both
};

class HttpParser
{
public:
	HttpParser(HttpParserCallbacks callbacks, HttpParserType type);
	~HttpParser();

	size_t feed(const void* buffer, size_t amt);

private:
	/*
http_cb      on_message_begin;
http_data_cb on_url;
http_data_cb on_status;
http_data_cb on_header_field;
http_data_cb on_header_value;
http_cb      on_headers_complete;
http_data_cb on_body;
http_cb      on_message_complete;

http_cb      on_chunk_header;
http_cb      on_chunk_complete;
	*/

	void flush_on_transition();

	int on_message_begin();
	int on_url(const char *at, size_t length);
	int on_status(const char *at, size_t length);
	int on_header_field(const char *at, size_t length);
	int on_header_value(const char *at, size_t length);
	int on_headers_complete();
	int on_body(const char *at, size_t length);
	int on_message_complete();

	friend int _on_message_begin(void*);
	friend int _on_url(void*, const char *, size_t);
	friend int _on_status(void*, const char *, size_t);
	friend int _on_header_field(void*, const char *, size_t);
	friend int _on_header_value(void*, const char *, size_t);
	friend int _on_headers_complete(void*);
	friend int _on_body(void*, const char *, size_t);
	friend int _on_message_complete(void*);

	enum class State
	{
		None, Status, Url, HeaderField, HeaderValue, Body
	};
	
	State _state;
	std::string _last_value;
	std::string _last_header;
	HttpParserCallbacks _callbacks;
	void* _pvt;
};