#include "pch.hpp"
#include "HttpParser.hpp"
#include "Hosting.hpp"

int case_insensitive_compare(const char *a, size_t asize, const char *b, size_t bsize) noexcept
{
	if (!a && !b) return 0;
	if (b && !a) return -1;
	if (a && !b) return 1;
	if (asize == 0 && bsize == 0) return 0;
	else if (asize < bsize) return -1;
	else if (asize > bsize) return 1;
	else return strncasecmp(&a[0], &b[0], asize);
}

std::string resolvepath(const std::string &path) noexcept
{
	char buf[PATH_MAX + 1];
	if (!realpath(path.c_str(), buf) || !buf[0])
	{
		return std::string();
	}
	else
	{
		struct ::stat di;
		if (stat(buf, &di) == -1)
		{
			return std::string();
		}
		else if (S_ISREG(di.st_mode))
		{
			return std::string(buf);
		}
		else if (S_ISDIR(di.st_mode))
		{
			auto buflen = strlen(buf);
			if (buf[buflen - 1] == '/')
			{
				return std::string(buf, buflen);
			}
			else
			{
				if (buflen >= PATH_MAX)
				{
					return std::string();
				}
				else
				{
					buf[buflen] = '/';
					buf[++buflen] = 0;
					return std::string(buf, buflen);
				}
			}
		}

		return std::string(buf);
	}
}

std::string combinepath(const std::string &root, const std::string &suffix)
{
	if (root.size() == 0) throw std::runtime_error("root must be an absolute path");
	else if (root[0] != '/') throw std::runtime_error("root must be an absolute path");
	else if (root.size() == 1) throw std::runtime_error("root cannot be the system root");

	if (suffix.size() == 0) throw std::runtime_error("suffix cannot be empty");
	if (suffix[0] != '/') throw std::runtime_error("suffix must start with slash");

	if (root.size() + suffix.size() > PATH_MAX) throw std::runtime_error("path too long");

	std::string result;
	result.reserve(root.size() + suffix.size());
	result += root;
	result += suffix;

	return resolvepath(result);
}

bool startswith(const char* a, size_t asize, const char *b, size_t bsize) noexcept
{
	if (asize < bsize) return false;
	return strncasecmp(a, b, bsize) == 0;
}

bool endswith(const char* a, size_t asize, const char *b, size_t bsize) noexcept
{
	if (asize < bsize) return false;
	return strncasecmp(a + (asize - bsize), b, bsize) == 0;
}

static std::string content_type_for(std::string filename)
{
	if (endswith(filename, ".png")) return "image/png";
	if (endswith(filename, ".gif")) return "image/gif";
	if (endswith(filename, ".bmp")) return "image/bmp";
	if (endswith(filename, ".svg")) return "image/svg+xml";
	if (endswith(filename, ".jpg") || endswith(filename, ".jpeg")) return "image/jpeg";

	if (endswith(filename, ".htm") || endswith(filename, ".html")) return "text/html";
	if (endswith(filename, ".css")) return "text/css";
	if (endswith(filename, ".txt")) return "text/plain";

	if (endswith(filename, ".js")) return "application/javascript";
	if (endswith(filename, ".json")) return "application/json";
	if (endswith(filename, ".xml")) return "application/xml";
	if (endswith(filename, ".pdf")) return "application/pdf";
	if (endswith(filename, ".woff")) return "application/font-woff";
	if (endswith(filename, ".woff2")) return "application/font-woff2";

	return "application/octet-stream";
}

void reset_response(response_info &response, int statusCode, const std::string &status)
{
	response.status_code = statusCode;
	response.status = status;
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
	response._strings.clear();
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

bool StaticHosting::request(const request_info &request, response_info &response)
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