#pragma once
#include "MappedFile.h"


class response_info;
class request_info;
void reset_request(request_info &request);
void reset_response(response_info &response);
void reset_response(response_info &response, int statusCode, const std::string &status);
void response_bad_request(response_info &response);
void response_internal_server_error(response_info &response);
void response_not_found(response_info &response);
void response_ok(response_info &response, size_t content_length, std::string content_type, const void* data);


enum class Method
{
	GET,
	HEAD,
	POST,
	PUT,
	PATCH,
	OPTIONS,
	Unknown
};

enum class Connection
{
	None,
	KeepAlive,
	Close,
	Unknown
};

struct keep_alive
{
	bool present;
	int timeout; //the minimum amount of time an idle connection has to be kept opened
	int max; // the maximum number of requests that can be sent on this connection before closing it
};

struct range
{
	size_t start;
	size_t end; // -1 is no end
};

struct content_range
{
	size_t start;
	size_t end;
	size_t size;
};

struct strict_transport_security
{
	size_t max_age;
	bool includeSubdomains;
	bool preload;
};

struct response_info
{
	response_info() {}

	int status_code;
	std::string status;
	int lastModified;
	std::string etag;
	std::string contentType;
	std::string contentEncoding;
	std::string setCookie;
	char tk;
	size_t contentLength;
	bool send_zero_content_length;
	std::string location;
	Connection connection;
	bool acceptRanges;
	struct content_range content_range;

	const void* response_data;
	size_t data_sent;

	std::vector<std::string> _strings;
};

struct request_info
{
	request_info() {}

	Method method;
	std::string path;
	std::string host;
	Connection connection;
	std::string accept;
	std::string accept_charset;
	std::string accept_encoding;
	std::string cookie;
	std::string referer;
	std::string user_agent;
	bool dnt;
	std::vector<range> ranges;
	bool upgrade_insecure;

	int stream_id;
	response_info response;
};

class Hosting
{
public:
	Hosting() {}
	Hosting(const Hosting&) = delete;
	virtual ~Hosting() {}

	virtual bool request(request_info &request) = 0;
};

class StaticHosting : public Hosting
{
public:
	StaticHosting(const std::string &root);
	~StaticHosting();
	virtual bool request(request_info &request);
private:
	const MappedFile &file(const std::string&);

	std::unordered_map<std::string, MappedFile> _files;
	std::string _root;
};
