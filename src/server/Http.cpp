#include "pch.hpp"
#include "Listener.hpp"
#include "Http.hpp"

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
	if (s_eq(name, "Host"))
	{
		request.host = value;
	}
	else if (s_eq(name, "Connection"))
	{
		if (s_eq(value, "Close"))
		{
			request.connection = Connection::Close;
		}
		else if (s_eq(value, "Keep-Alive"))
		{
			request.connection = Connection::KeepAlive;
		}
		else
		{
			request.connection = Connection::Unknown;
		}
	}
	else if (s_eq(name, "User-Agent")) request.user_agent = value;
	else if (s_eq(name, "Accept")) request.accept = value;
	else if (s_eq(name, "Accept-Charset")) request.accept_charset = value;
	else if (s_eq(name, "Accept-Encoding")) request.accept_encoding = value;
	else if (s_eq(name, "Cookie")) request.cookie = value;
	else if (s_eq(name, "DNT")) request.dnt = value == "1";
	else if (s_eq(name, "Upgrade")) request.upgrade_insecure = value == "1";
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
		serialize_headers(rinfo),
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


Http2Handler::Http2Handler(HttpServer &http, std::shared_ptr<Socket> socket)
	:_http(http), _socket(socket)
{

}

void Http2Handler::read_avail()
{
	if (_socket) { _socket->close(); _socket.reset(); }
}

void Http2Handler::write_avail()
{

}

void Http2Handler::closed()
{

}
