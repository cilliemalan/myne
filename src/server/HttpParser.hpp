#pragma once

struct HttpParserCallbacks
{
	std::function<void(std::string method, std::string url)> url;
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
		None, Status, Url, Header, Body
	};

	State _state;
	std::string _last_value;
	std::string _last_header;
	HttpParserCallbacks _callbacks;
	void* _pvt;
};

class UrlParser
{
public:
	UrlParser(const char* url, size_t len);
	UrlParser(const std::string &s) : UrlParser(&s[0], s.size()) {}

	inline operator bool() const { return _valid; }

	inline std::uint16_t port() const { return _port; }

	inline std::pair<const char*, std::uint16_t> schema() const { return std::make_pair(_url + _schema, _schema_len); }
	inline std::pair<const char*, std::uint16_t> host() const { return std::make_pair(_url + _host, _host_len); }
	inline std::pair<const char*, std::uint16_t> path() const { return std::make_pair(_url + _path, _path_len); }
	inline std::pair<const char*, std::uint16_t> query() const { return std::make_pair(_url + _query, _query_len); }
	inline std::pair<const char*, std::uint16_t> fragment() const { return std::make_pair(_url + _fragment, _fragment_len); }

	inline std::string s_schema() const { return std::string(_url + _schema, _schema_len); }
	inline std::string s_host() const { return std::string(_url + _host, _host_len); }
	inline std::string s_path() const { return std::string(_url + _path, _path_len); }
	inline std::string s_query() const { return std::string(_url + _query, _query_len); }
	inline std::string s_fragment() const { return std::string(_url + _fragment, _fragment_len); }
private:
	const char *_url;
	bool _valid;

	std::uint16_t _port;

	std::uint16_t _schema;
	std::uint16_t _schema_len;
	std::uint16_t _host;
	std::uint16_t _host_len;
	std::uint16_t _path;
	std::uint16_t _path_len;
	std::uint16_t _query;
	std::uint16_t _query_len;
	std::uint16_t _fragment;
	std::uint16_t _fragment_len;
};