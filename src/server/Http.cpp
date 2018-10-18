#include "pch.hpp"
#include "Listener.hpp"
#include "Http.hpp"

void process_header(request_info *request, const char *name, size_t namelen, std::string value)
{
	if (s_eq(name, namelen, ":method"))
	{
		if (s_eq(value, "GET")) request->method = Method::GET;
		else if (s_eq(value, "HEAD")) request->method = Method::HEAD;
		else if (s_eq(value, "OPTIONS")) request->method = Method::OPTIONS;
		else if (s_eq(value, "PATCH")) request->method = Method::PATCH;
		else if (s_eq(value, "POST")) request->method = Method::POST;
		else if (s_eq(value, "PUT")) request->method = Method::PUT;
		else request->method = Method::Unknown;
	}
	else if (s_eq(name, namelen, ":path"))
	{
		request->path = value;
	}
	else if (s_eq(name, namelen, "Host") || s_eq(name, namelen, ":authority"))
	{
		request->host = value;
	}
	else if (s_eq(name, namelen, "Connection"))
	{
		if (s_eq(value, "Close"))
		{
			request->connection = Connection::Close;
		}
		else if (s_eq(value, "Keep-Alive"))
		{
			request->connection = Connection::KeepAlive;
		}
		else
		{
			request->connection = Connection::Unknown;
		}
	}
	else if (s_eq(name, namelen, "User-Agent")) request->user_agent = value;
	else if (s_eq(name, namelen, "Accept")) request->accept = value;
	else if (s_eq(name, namelen, "Accept-Charset")) request->accept_charset = value;
	else if (s_eq(name, namelen, "Accept-Encoding")) request->accept_encoding = value;
	else if (s_eq(name, namelen, "Cookie")) request->cookie = value;
	else if (s_eq(name, namelen, "DNT")) request->dnt = value == "1";
	else if (s_eq(name, namelen, "Upgrade")) request->upgrade_insecure = value == "1";
}

void process_header(request_info *request, const char *name, size_t namelen, const char *value, size_t valuelen)
{
	process_header(request, name, namelen, std::string(value, valuelen));
}

void process_header(request_info *request, std::string name, std::string value)
{
	process_header(request, &name[0], name.size(), value);
}

std::string serialize_day_of_week(int dow)
{
	switch (dow)
	{
	case 0: return "Sun";
	case 1: return "Mon";
	case 2: return "Tue";
	case 3: return "Wed";
	case 4: return "Thu";
	case 5: return "Fri";
	case 6: return "Sat";
	default: return "";
	}
}

std::string serialize_moth(int m)
{
	switch (m)
	{
	case 0: return "Jan";
	case 1: return "Feb";
	case 2: return "Mar";
	case 3: return "Apr";
	case 4: return "May";
	case 5: return "Jun";
	case 6: return "Jul";
	case 7: return "Aug";
	case 8: return "Sep";
	case 9: return "Oct";
	case 10: return "Nov";
	case 11: return "Dec";
	default: return "";
	}
}

std::string zeropad(int i, unsigned char width)
{
	std::string result;
	result += std::to_string(i);
	while (result.size() < width) result.insert(0, "0");
	return result;
}

std::string serialize_date(time_t date)
{
	auto &ti = *gmtime(&date);

	std::string result;
	result.reserve(30);
	result += serialize_day_of_week(ti.tm_wday);
	result += ", ";
	result += zeropad(ti.tm_mday, 2);
	result += " ";
	result += serialize_moth(ti.tm_mon);
	result += " ";
	result += std::to_string(ti.tm_year + 1900);
	result += " ";
	result += zeropad(ti.tm_hour, 2);
	result += ":";
	result += zeropad(ti.tm_min, 2);
	result += ":";
	result += zeropad(ti.tm_sec % 60, 2);
	result += " GMT";
	return result;
}

std::vector<char> serialize_headers_http1(const response_info& r)
{
	std::string output;
	output += "HTTP/1.1 ";
	output += std::to_string(r.status_code);
	output += " ";
	output += r.status;
	output += "\r\n";

	if (r.acceptRanges) output += "Accept-Ranges: bytes\r\n";
	if (r.connection == Connection::Close) output += "Connection: close\r\n";
	else if (r.connection == Connection::KeepAlive) output += "Connection: keep-alive\r\n";
	if (r.contentEncoding.size() > 0) output += "Content-Encoding: " + r.contentEncoding + "\r\n";
	if (r.contentLength >= 0)
	{
		output += "Content-Length: ";
		output += std::to_string(r.contentLength);
		output += "\r\n";
	}
	if (r.contentType.size() > 0) output += "Content-Type: " + r.contentType + "\r\n";
	if (r.content_range.end != 0)
	{
		output += "Content-Range: bytes ";
		output += std::to_string(r.content_range.start);
		output += "-";
		output += std::to_string(r.content_range.end);
		if (r.content_range.size > 0)
		{
			output += "/";
			output += std::to_string(r.content_range.size);
		}
		output += "\r\n";
	}
	if (r.etag.size() > 0) output += "ETag: \"" + r.etag + "\"\r\n";
	if (r.lastModified != 0) output += "Last-Modified: " + serialize_date(r.lastModified) + "\r\n";
	if (r.location.size() > 0) output += "Location: " + r.location + "\r\n";
	if (r.setCookie.size() > 0) output += "Set-Cookie: " + r.setCookie + "\r\n";
	if (r.tk != 0)
	{
		output += "Tk: ";
		output += r.tk;
		output += "\r\n";
	}
	output += "\r\n";
	return std::vector<char>(output.begin(), output.end());
}

void emplace_http2_header(std::vector<nghttp2_nv> &v, const char* name, size_t namelen, const char* value, size_t valuelen)
{
	v.emplace_back(nghttp2_nv
	{
		const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name)),
		const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value)),
		namelen,
		valuelen,
		NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE
	});
}

void emplace_http2_header(std::vector<nghttp2_nv> &v, const std::string &name, const char* value, size_t valuelen)
{
	emplace_http2_header(v, &name[0], name.size(), value, valuelen);
}

void emplace_http2_header(std::vector<nghttp2_nv> &v, const std::string &name, const std::string &value)
{
	emplace_http2_header(v, &name[0], name.size(), &value[0], value.size());
}

std::vector<nghttp2_nv> serialize_headers_http2(std::unique_ptr<response_info> &_r)
{
	static std::string h_status(":status");
	static std::string h_accept_ranges("accept-ranges");
	static std::string h_connection("connection");
	static std::string h_content_encoding("content-encoding");
	static std::string h_content_length("content-length");
	static std::string h_content_type("content-type");
	static std::string h_content_range("content-range");
	static std::string h_etag("etag");
	static std::string h_last_modified("last-modified");
	static std::string h_location("location");
	static std::string h_set_cookie("set-cookie");
	static std::string h_tk("tk");

	static std::string s_bytes("bytes");
	static std::string s_close("close");
	static std::string s_keep_alive("keep-alive");

	std::vector<nghttp2_nv> headers;
	auto &r = *_r;

	emplace_http2_header(headers, h_status, &r.status[0], 3);


	if (r.acceptRanges) emplace_http2_header(headers, h_accept_ranges, s_bytes);
	if (r.connection == Connection::Close) emplace_http2_header(headers, h_connection, s_close);
	else if (r.connection == Connection::KeepAlive) emplace_http2_header(headers, h_connection, s_keep_alive);
	if (r.contentEncoding.size() > 0) emplace_http2_header(headers, h_content_encoding, r.contentEncoding);
	if (r.contentLength >= 0) emplace_http2_header(headers, h_content_length, r._strings.emplace_back(std::to_string(r.contentLength)));
	if (r.contentType.size() > 0) emplace_http2_header(headers, h_content_type, r.contentType);
	if (r.content_range.end != 0)
	{
		std::string content_range("bytes ");
		content_range += std::to_string(r.content_range.start);
		content_range += "-";
		content_range += std::to_string(r.content_range.end);
		if (r.content_range.size > 0)
		{
			content_range += "/";
			content_range += std::to_string(r.content_range.size);
		}
		emplace_http2_header(headers, h_content_range, r._strings.emplace_back(content_range));
	}
	if (r.etag.size() > 0) emplace_http2_header(headers, h_etag, r.etag);
	if (r.lastModified != 0) emplace_http2_header(headers, h_last_modified, r._strings.emplace_back(serialize_date(r.lastModified)));
	if (r.location.size() > 0) emplace_http2_header(headers, h_location, r.location);
	if (r.setCookie.size() > 0) emplace_http2_header(headers, h_set_cookie, r.setCookie);
	if (r.tk != 0) emplace_http2_header(headers, h_tk, &r.tk, 1);

	return headers;
}


HttpHandler::HttpHandler(HttpServer &http, std::shared_ptr<Socket> socket)
	: _done(false), _http(http), _socket(socket), _parser(parser_callbacks(), HttpParserType::Request)
{
}

void HttpHandler::read_avail()
{
	process();
}

void HttpHandler::write_avail()
{
	if (!_socket) return;

	while (!_done && pending_responses.size() > 0)
	{
		auto &r = pending_responses[pending_responses.size() - 1];
		auto headers_left = r.headers.size() - r.headers_written;
		auto body_left = r.body ? r.body_size - r.body_written : 0;

		if (headers_left > 0)
		{
			auto written = _socket->write(&r.headers[r.headers_written], headers_left);
			if (written == 0) { _done = true; break; }
			else if (written < 0) break;

			r.headers_written += written;
			continue;
		}
		else if (body_left > 0)
		{
			auto written = _socket->write(r.body + r.body_written, body_left);
			if (written == 0) { _done = true; break; }
			else if (written < 0) break;

			r.body_written += written;
			continue;
		}
		else
		{
			pending_responses.pop_back();
		}
	}

	if (_done && _socket)
	{
		_socket->close();
		_socket.reset();
	}
}

void HttpHandler::closed()
{
	_socket.reset();
}

// process incoming data
void HttpHandler::process()
{
	if (!_socket) return;

	char buffer[2048];
	auto amt = _socket->read(buffer, sizeof(buffer));

	bool eof = false;
	if (_done || amt == 0)
	{
		eof = true;
	}
	else if (amt > 0)
	{
		auto r = _parser.feed(buffer, amt);
		if (static_cast<ssize_t>(r) != amt)
		{
			eof = true;
		}
	}

	if (eof)
	{
		_done = true;
		if (_socket)
		{
			_socket->close();
			_socket.reset();
		}
	}
}

void HttpHandler::on_url(std::string method, std::string url)
{
	if (s_eq(method, "GET")) request.method = Method::GET;
	else if (s_eq(method, "HEAD")) request.method = Method::HEAD;
	else if (s_eq(method, "OPTIONS")) request.method = Method::OPTIONS;
	else if (s_eq(method, "PATCH")) request.method = Method::PATCH;
	else if (s_eq(method, "POST")) request.method = Method::POST;
	else if (s_eq(method, "PUT")) request.method = Method::PUT;
	else request.method = Method::Unknown;

	request.path = url;
}

void HttpHandler::on_status(int, std::string)
{
	// only for requests. Never called
}

void HttpHandler::on_header(std::string name, std::string value)
{
	process_header(&request, name, value);
}

void HttpHandler::on_headers_complete()
{

}

void HttpHandler::on_body(const char* b, size_t l)
{

}

void HttpHandler::on_message_complete()
{
	bool handled = false;
	response_info rinfo;
	for (auto h : _http)
	{
		if (h->request(request, rinfo))
		{
			handled = true;
			break;
		}
	}

	if (!handled)
	{
		response_not_found(rinfo);
	}

	pending_responses.emplace_back(
		serialize_headers_http1(rinfo),
		static_cast<const char*>(rinfo.response_data),
		rinfo.contentLength);
}

HttpParserCallbacks HttpHandler::parser_callbacks()
{
	return HttpParserCallbacks
	{
		[this](std::string m, std::string u) { on_url(m, u); },
		[this](int i, std::string s) { on_status(i, s); },
		[this](std::string a, std::string b) { on_header(a, b); },
		std::bind(&HttpHandler::on_headers_complete, this),
		[this](const char* b, size_t l) { on_body(b,l); },
		std::bind(&HttpHandler::on_message_complete, this)
	};
}





ssize_t http2_recv(nghttp2_session *session, uint8_t *buf, size_t length, int flags, void *user_data)
{
	return reinterpret_cast<Http2Handler*>(user_data)->_recv(buf, length, flags);
}

ssize_t http2_send(nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_data)
{
	return reinterpret_cast<Http2Handler*>(user_data)->_send(data, length, flags);
}

int http2_on_frame_recv(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
	return reinterpret_cast<Http2Handler*>(user_data)->_on_frame_recv(frame);
}

int http2_on_begin_headers(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
	return reinterpret_cast<Http2Handler*>(user_data)->_on_begin_headers(frame);
}

int http2_on_header(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags, void *user_data)
{
	return reinterpret_cast<Http2Handler*>(user_data)->_on_header(frame, name, namelen, value, valuelen, flags);
}

int http2_on_stream_close(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data)
{
	return reinterpret_cast<Http2Handler*>(user_data)->_on_stream_close(stream_id, error_code);
}

ssize_t http2_read(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source, void *user_data)
{
	return reinterpret_cast<Http2Handler*>(user_data)->_read(stream_id, buf, length, data_flags, source);
}

int http2_send_data(nghttp2_session *session, nghttp2_frame *frame, const uint8_t *framehd, size_t length, nghttp2_data_source *source, void *user_data)
{
	return reinterpret_cast<Http2Handler*>(user_data)->_send_data(frame, framehd, length, source);
}

Http2Handler::Http2Handler(HttpServer &http, std::shared_ptr<Socket> socket)
	:_http(http), _socket(socket), _session(nullptr)
{
	nghttp2_session_callbacks *callbacks = nullptr;
	nghttp2_session_callbacks_new(&callbacks);
	if (!callbacks) throw std::runtime_error("could not create nghttp2 session");

	try
	{
		nghttp2_session_callbacks_set_send_callback(callbacks, http2_send);
		nghttp2_session_callbacks_set_send_data_callback(callbacks, http2_send_data);
		nghttp2_session_callbacks_set_recv_callback(callbacks, http2_recv);
		nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, http2_on_frame_recv);
		nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, http2_on_begin_headers);
		nghttp2_session_callbacks_set_on_header_callback(callbacks, http2_on_header);
		nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, http2_on_stream_close);

		nghttp2_session_server_new(&_session, callbacks, this);
		if (!_session) throw std::runtime_error("could not create nghttp2 session");

		nghttp2_settings_entry iv[]
		{
			{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}
		};

		if (nghttp2_submit_settings(_session, NGHTTP2_FLAG_NONE, iv, 1))
		{
			throw std::runtime_error("failed to send settings header");
		}
	}
	catch (...)
	{
		if (callbacks) nghttp2_session_callbacks_del(callbacks);
		throw;
	}
}

Http2Handler::~Http2Handler()
{
	if (_session) { nghttp2_session_del(_session); _session = nullptr; }
	if (_socket) { _socket->close(); _socket.reset(); }
}

void Http2Handler::read_avail()
{
	if (_session)
	{
		nghttp2_session_recv(_session);
	}
}

void Http2Handler::write_avail()
{
	if (_session)
	{
		nghttp2_session_send(_session);
	}
}

void Http2Handler::closed()
{

}

ssize_t Http2Handler::_recv(uint8_t *buf, size_t length, int flags)
{
	if (!_socket) return NGHTTP2_ERR_EOF;

	ssize_t amt = _socket->read(buf, length);
	if (amt == 0)
	{
		return NGHTTP2_ERR_EOF;
	}
	else if (amt < 0)
	{
		return NGHTTP2_ERR_WOULDBLOCK;
	}

	return amt;
}

ssize_t Http2Handler::_send(const uint8_t *data, size_t length, int flags)
{
	if (!_socket) return NGHTTP2_ERR_EOF;

	ssize_t amt = _socket->write(data, length);

	if (amt == 0)
	{
		return NGHTTP2_ERR_EOF;
	}
	else if (amt < 0)
	{
		return NGHTTP2_ERR_WOULDBLOCK;
	}

	return amt;
}

int Http2Handler::_on_frame_recv(const nghttp2_frame *frame)
{
	switch (frame->hd.type)
	{
	case NGHTTP2_DATA:
	case NGHTTP2_HEADERS:
		if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)
		{
			auto sttr = _streams.find(frame->hd.stream_id);
			if (sttr != _streams.end())
			{
				bool handled = false;
				auto &stream = sttr->second;
				stream.response.reset(new response_info());
				for (auto h : _http)
				{
					if (h->request(stream, *stream.response))
					{
						handled = true;
						break;
					}
				}

				if (!handled)
				{
					response_not_found(*stream.response);
				}

				std::vector<nghttp2_nv> response_headers = serialize_headers_http2(stream.response);

				nghttp2_data_provider response_data;
				response_data.source.ptr = this;
				response_data.read_callback = http2_read;
				return nghttp2_submit_response(_session, stream.stream_id, &response_headers[0], response_headers.size(), &response_data);
			}
		}
	}

	return 0;
}

int Http2Handler::_on_begin_headers(const nghttp2_frame *frame)
{
	auto stream_id = frame->hd.stream_id;
	auto strr = _streams.emplace(stream_id, stream_id);
	if (!strr.second) return -1;

	return 0;
}

int Http2Handler::_on_header(const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags)
{
	auto stream_id = frame->hd.stream_id;
	auto strr = _streams.find(stream_id);
	if (strr == _streams.end()) return -1;
	auto &stream = strr->second;

	if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST)
	{
		process_header(&stream, reinterpret_cast<const char*>(name), namelen, reinterpret_cast<const char*>(value), valuelen);
	}
	return 0;
}

int Http2Handler::_on_stream_close(int32_t stream_id, uint32_t error_code)
{
	_streams.erase(stream_id);
	return 0;
}

ssize_t Http2Handler::_read(int32_t stream_id, uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source)
{
	auto strr = _streams.find(stream_id);
	if (strr == _streams.end()) return -1;
	auto &stream = strr->second;
	auto &response = *stream.response;
	auto data_avail = response.contentLength;
	const char* data = reinterpret_cast<const char*>(response.response_data);

	size_t amt = length < data_avail ? length : data_avail;
	if (amt == 0)
	{
		*data_flags = NGHTTP2_DATA_FLAG_EOF;
	}
	else
	{
		//*data_flags = NGHTTP2_DATA_FLAG_NO_COPY;
		memcpy(buf, response.response_data, amt);
		response.response_data = data + amt;
		response.contentLength -= amt;
	}

	return amt;
}

int Http2Handler::_send_data(nghttp2_frame *frame, const uint8_t *framehd, size_t length, nghttp2_data_source *source)
{
	//auto strr = _streams.find(frame->hd.stream_id);
	//if (strr == _streams.end()) return -1;
	//auto &stream = strr->second;
	//auto &response = *stream.response;
	//auto data_avail = response.contentLength;

	return 0;
}