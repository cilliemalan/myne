#include "pch.hpp"
#include "Listener.hpp"
#include "Http.hpp"


HttpHandler::HttpHandler(HttpServer &http, std::shared_ptr<ComboSocket> socket)
	: _http(http), _socket(socket), _parser(parser_callbacks(), HttpParserType::Request)
{
}


void HttpHandler::read_avail()
{
	process();
}

void HttpHandler::write_avail()
{
	if (_done)
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
		_socket->close();
		_socket.reset();
	}
}

void HttpHandler::on_url(std::string method, std::string url)
{
	_method = method;
	_url = url;
}

void HttpHandler::on_status(int, std::string)
{
	// only for requests. Never called
}

void HttpHandler::on_header(std::string name, std::string value)
{
	if (name == "Host")
	{
		_host = value;
	}
}

void HttpHandler::on_headers_complete()
{

}

void HttpHandler::on_body(const char* b, size_t l)
{

}

void HttpHandler::on_message_complete()
{
	std::string output("HTTP/1.1 200 OK\r\n\r\nGreat!");
	std::vector<char> v(output.begin(), output.end());
	_socket->buffered_write(v);
	_done = true;
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
