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

	virtual ~Http2Handler();

	virtual void read_avail();
	virtual void write_avail();
	virtual void closed();
private:
	HttpServer &_http;
	std::shared_ptr<Socket> _socket;

	std::unordered_map<int, request_info> _streams;
	nghttp2_session *_session;

	ssize_t _recv(uint8_t *buf, size_t length, int flags);
	ssize_t _send(const uint8_t *data, size_t length, int flags);
	int _on_header(const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags);
	int _on_begin_headers(const nghttp2_frame *frame);
	int _on_frame_recv(const nghttp2_frame *frame);
	int _on_stream_close(int32_t stream_id, uint32_t error_code);
	ssize_t _read(int32_t stream_id, uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source);
	int _send_data(nghttp2_frame *frame, const uint8_t *framehd, size_t length, nghttp2_data_source *source);

	friend ssize_t http2_recv(nghttp2_session *session, uint8_t *buf, size_t length, int flags, void *user_data);
	friend ssize_t http2_send(nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_data);
	friend int http2_on_header(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags, void *user_data);
	friend int http2_on_begin_headers(nghttp2_session *session, const nghttp2_frame *frame, void *user_data);
	friend int http2_on_frame_recv(nghttp2_session *session, const nghttp2_frame *frame, void *user_data);
	friend int http2_on_stream_close(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data);
	friend ssize_t http2_read(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source, void *user_data);
	friend int http2_send_data(nghttp2_session *session, nghttp2_frame *frame, const uint8_t *framehd, size_t length, nghttp2_data_source *source, void *user_data);
};