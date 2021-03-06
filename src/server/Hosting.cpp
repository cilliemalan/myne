#include "pch.hpp"
#include "HttpParser.hpp"
#include "Hosting.hpp"


static const std::string _mime_bin("application/octet-stream");
static const std::unordered_map<std::string, std::string> _mime_mappings{
	{".png", "image/png"},
	{".gif", "image/gif"},
	{".bmp", "image/bmp"},
	{".svg", "image/svg+xml"},

	{".htm", "text/html"},
	{".html", "text/html"},
	{".txt", "text/plain"},
	{".css", "text/css"},

	{".js", "application/javascript"},
	{".json", "application/json"},
	{".xml", "application/xml"},
	{".pdf", "application/pdf"},
	{".woff", "application/font-woff"},
	{".woff2", "application/font-woff2"},

	{".zip", "application/octet-stream"},
};


static const std::string &content_type_for(const std::string &filename)
{
	std::string ext = pathextension(filename);
	const auto iter = _mime_mappings.find(ext);
	if (iter != _mime_mappings.cend())
	{
		return iter->second;
	}
	else
	{
		return _mime_bin;
	}
}

void reset_request(request_info &request)
{
	request.method = Method::Unknown;
	request.path.clear();
	request.host.clear();
	request.connection = Connection::None;
	request.accept.clear();
	request.accept_charset.clear();
	request.accept_encoding.clear();
	request.cookie.clear();
	request.referer.clear();
	request.user_agent.clear();
	request.dnt = false;
	request.ranges.clear();
	request.upgrade_insecure = false;

	request.stream_id = 0;
	reset_response(request.response);
}

void reset_response(response_info &response)
{
	response.status_code = 0;
	response.status.clear();
	response.lastModified = 0;
	response.etag.clear();
	response.contentType.clear();
	response.contentEncoding.clear();
	response.setCookie.clear();
	response.tk = '\0';
	response.contentLength = 0;
	response.send_zero_content_length = 0;
	response.location.clear();
	response.connection = Connection::None;
	response.acceptRanges = false;
	response.content_range.end = 0;
	response.content_range.size = 0;
	response.content_range.start = 0;
	response.response_data = 0;
	response.data_sent = 0;
	response._strings.clear();
}

void reset_response(response_info &response, int statusCode, const std::string &status)
{
	reset_response(response);
	response.status_code = statusCode;
	response.status.reserve(status.size() + 1);
	response.status.assign(status);
}

static std::string _sok("200 OK");
static std::string _sbad_request("400 Bad Request");
static std::string _sinternal_server_error("500 Internal Server Error");
static std::string _snot_found("404 Not Found");
static std::string _smethod_not_allowed("405 Method Not Allowed");

void response_error(response_info &response, int code, const std::string &status)
{
	reset_response(response, 400, status);
	response.contentLength = status.size();
	response.response_data = &status[0];
}

void response_bad_request(response_info &response)
{
	response_error(response, 400, _sbad_request);
}

void response_internal_server_error(response_info &response)
{
	response_error(response, 500, _sinternal_server_error);
}

void response_not_found(response_info &response)
{
	response_error(response, 404, _snot_found);
}

void response_method_not_allowed(response_info &response)
{
	response_error(response, 405, _smethod_not_allowed);
}

void response_ok(response_info &response, size_t content_length, std::string content_type, const void* data)
{
	reset_response(response, 200, _sok);

	if (content_length != static_cast<size_t>(-1)) response.contentLength = content_length;
	if (content_type.size()) response.contentType = content_type;
	if (data) response.response_data = data;
}



StaticHosting::StaticHosting(const std::string &root)
{
	if (root.size() == 0)
	{
		throw std::runtime_error("root was empty");
	}
	else if (root == "/")
	{
		throw std::runtime_error("root cannot be the root dir");
	}
	else
	{
		_root = resolvepath(root);
		if (_root.size() == 0 || _root[_root.size() - 1] != '/')
		{
			throw std::runtime_error("root is not a directory");
		}
	}
}

StaticHosting::~StaticHosting()
{
}

bool StaticHosting::request(request_info &request)
{
	UrlParser parsed_url(request.path);

	if (!parsed_url)
	{
		return false;
	}

	const std::string &path = parsed_url.s_path();
	auto full_path = combinepath(_root, path);

	if (full_path.size() == 0 || !startswith(full_path, _root))
	{
		return false;
	}

	if (full_path[full_path.size() - 1] == '/')
	{
		full_path = combinepath(full_path, "/index.html");

		if (full_path.size() == 0 || full_path[full_path.size() - 1] == '/')
		{
			return false;
		}
	}

	try
	{
		auto &f = file(full_path);
		auto &response = request.response;

		if (request.method == Method::GET)
		{
			response_ok(response, f.size(), content_type_for(f.name()), &f[0]);
		}
		else if (request.method == Method::HEAD)
		{
			response_ok(response, f.size(), content_type_for(f.name()), nullptr);
		}
		else
		{
			response_method_not_allowed(response);
		}

		return true;
	}
	catch (...)
	{
		return false;
	}
}

const MappedFile &StaticHosting::file(const std::string &filename)
{
	auto file = _files.find(filename);
	if (file != _files.cend())
	{
		return file->second;
	}
	else
	{
		auto r = _files.emplace(filename, filename);
		return r.first->second;
	}
}