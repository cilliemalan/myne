#include "pch.hpp"
#include "HttpParser.hpp"
#include "../../thirdparty/http-parser/http_parser.h"


#define _parser (reinterpret_cast<http_parser*>(_pvt))

// link up callbacks
int _on_message_begin(void* parser) { return reinterpret_cast<HttpParser*>(reinterpret_cast<http_parser*>(parser)->data)->on_message_begin(); }
int _on_url(void* parser, const char *at, size_t length) { return reinterpret_cast<HttpParser*>(reinterpret_cast<http_parser*>(parser)->data)->on_url(at, length); }
int _on_status(void* parser, const char *at, size_t length) { return reinterpret_cast<HttpParser*>(reinterpret_cast<http_parser*>(parser)->data)->on_status(at, length); }
int _on_header_field(void* parser, const char *at, size_t length) { return reinterpret_cast<HttpParser*>(reinterpret_cast<http_parser*>(parser)->data)->on_header_field(at, length); }
int _on_header_value(void* parser, const char *at, size_t length) { return reinterpret_cast<HttpParser*>(reinterpret_cast<http_parser*>(parser)->data)->on_header_value(at, length); }
int _on_headers_complete(void* parser) { return reinterpret_cast<HttpParser*>(reinterpret_cast<http_parser*>(parser)->data)->on_headers_complete(); }
int _on_body(void* parser, const char *at, size_t length) { return reinterpret_cast<HttpParser*>(reinterpret_cast<http_parser*>(parser)->data)->on_body(at, length); }
int _on_message_complete(void* parser) { return reinterpret_cast<HttpParser*>(reinterpret_cast<http_parser*>(parser)->data)->on_message_complete(); }

static http_parser_settings settings
{
  reinterpret_cast<http_cb>(_on_message_begin),
  reinterpret_cast<http_data_cb>(_on_url),
  reinterpret_cast<http_data_cb>(_on_status),
  reinterpret_cast<http_data_cb>(_on_header_field),
  reinterpret_cast<http_data_cb>(_on_header_value),
  reinterpret_cast<http_cb>(_on_headers_complete),
  reinterpret_cast<http_data_cb>(_on_body),
  reinterpret_cast<http_cb>(_on_message_complete),
  nullptr, //on_chunk_header
  nullptr, //on_chunk_complete
};
const auto _settings = &settings;


void append_to(std::string &s, const char* b, size_t a)
{
	s.reserve(s.size() + a);
	s.append(b, b + a);
}


HttpParser::HttpParser(HttpParserCallbacks callbacks, HttpParserType type)
	:_callbacks(callbacks), _pvt(new http_parser)
{
	http_parser_init(_parser, static_cast<http_parser_type>(type));
	_parser->data = this;
}

HttpParser::~HttpParser()
{
	if (_pvt) { delete reinterpret_cast<http_parser*>(_pvt); _pvt = 0; }
}

size_t HttpParser::feed(const void* buffer, size_t amt)
{
	return http_parser_execute(_parser, _settings, reinterpret_cast<const char*>(buffer), amt);
}

void HttpParser::flush_on_transition()
{
	if (_state == State::Url)
	{
		if (_callbacks.url) _callbacks.url(http_method_str(static_cast<http_method>(_parser->method)), _last_value);
		_last_header.erase();
		_last_value.erase();
	}
	else if (_state == State::Status)
	{
		if (_callbacks.status) _callbacks.status(_parser->status_code, _last_value);
		_last_header.erase();
		_last_value.erase();
	}
	else if (_state == State::Header)
	{
		if (_callbacks.header) _callbacks.header(_last_header, _last_value);
		_last_header.erase();
		_last_value.erase();
	}
}

int HttpParser::on_message_begin()
{
	_last_value.clear();
	_last_header.clear();

	return 0;
}

int HttpParser::on_url(const char *at, size_t length)
{
	append_to(_last_value, at, length);
	_state = State::Url;
	return 0;
}

int HttpParser::on_status(const char *at, size_t length)
{
	append_to(_last_value, at, length);
	_state = State::Status;
	return 0;
}

int HttpParser::on_header_field(const char *at, size_t length)
{
	flush_on_transition();
	append_to(_last_header, at, length);
	_state = State::Header;
	return 0;
}

int HttpParser::on_header_value(const char *at, size_t length)
{
	append_to(_last_value, at, length);
	return 0;
}

int HttpParser::on_headers_complete()
{
	flush_on_transition();
	_last_header.clear();
	_last_value.clear();
	_last_header.shrink_to_fit();
	_last_value.shrink_to_fit();
	if (_callbacks.headers_complete) _callbacks.headers_complete();
	return 0;
}

int HttpParser::on_body(const char *at, size_t length)
{
	if (_callbacks.body) _callbacks.body(at, length);
	return 0;
}

int HttpParser::on_message_complete()
{
	if (_callbacks.message_complete) _callbacks.message_complete();
	return 0;
}


UrlParser::UrlParser(const char* s, size_t len)
{
	if (len == 0 || len > PATH_MAX || s[0] != '/')
	{
		memset(this, 0, sizeof(UrlParser));
	}
	else
	{
		struct http_parser_url u;
		http_parser_url_init(&u);
		_valid = !http_parser_parse_url(s, len, false, &u);
		if (_valid)
		{
			_url = s;
			_port = u.port;

			if ((u.field_set & (1 << UF_SCHEMA)) != 0)
			{
				_schema = u.field_data[UF_SCHEMA].off;
				_schema_len = u.field_data[UF_SCHEMA].len;
			}
			else
			{
				_schema = 0;
				_schema_len = 0;
			}

			if ((u.field_set & (1 << UF_HOST)) != 0)
			{
				_host = u.field_data[UF_HOST].off;
				_host_len = u.field_data[UF_HOST].len;
			}
			else
			{
				_host = 0;
				_host_len = 0;
			}

			if ((u.field_set & (1 << UF_PATH)) != 0)
			{
				_path = u.field_data[UF_PATH].off;
				_path_len = u.field_data[UF_PATH].len;
			}
			else
			{
				_path = 0;
				_path_len = 0;
			}

			if ((u.field_set & (1 << UF_QUERY)) != 0)
			{
				_query = u.field_data[UF_QUERY].off;
				_query_len = u.field_data[UF_QUERY].len;
			}
			else
			{
				_query = 0;
				_query_len = 0;
			}

			if ((u.field_set & (1 << UF_FRAGMENT)) != 0)
			{
				_fragment = u.field_data[UF_FRAGMENT].off;
				_fragment_len = u.field_data[UF_FRAGMENT].len;
			}
			else
			{
				_fragment = 0;
				_fragment_len = 0;
			}
		}
		else
		{
			memset(this, 0, sizeof(UrlParser));
		}
	}
}