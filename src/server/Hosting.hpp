#pragma once
#include "MappedFile.h"


int case_insensitive_compare(const char *a, size_t asize, const char *b, size_t bsize) noexcept;
std::string resolvepath(const std::string &path) noexcept;
std::string combinepath(const std::string &root, const std::string &suffix);
bool startswith(const char* a, size_t asize, const char *b, size_t bsize) noexcept;
bool endswith(const char* a, size_t asize, const char *b, size_t bsize) noexcept;

inline int case_insensitive_compare(const std::string &a, const std::string &b) noexcept
{
	return case_insensitive_compare(a.c_str(), a.size(), b.c_str(), b.size());
}

inline int case_insensitive_compare(const std::string &a, const char* b, size_t bsize) noexcept
{
	return case_insensitive_compare(a.c_str(), a.size(), b, bsize);
}

template<size_t BSIZE>
inline int case_insensitive_compare(const std::string &a, const char (&b)[BSIZE]) noexcept
{
	return case_insensitive_compare(a.c_str(), a.size(), b, sizeof(b) - 1);
}

inline bool s_eq(const std::string &a, const std::string &b) noexcept
{
	return case_insensitive_compare(a, b) == 0;
}

inline bool s_eq(const char *a, size_t asize, const char *b, size_t bsize) noexcept
{
	return case_insensitive_compare(a, asize, b, bsize) == 0;
}

template<size_t ASIZE, size_t BSIZE>
inline bool s_eq(const char (&a)[ASIZE], const char (&b)[BSIZE]) noexcept
{
	return case_insensitive_compare(a, sizeof(a) - 1, b, sizeof(b) - 1) == 0;
}

template<size_t BSIZE>
inline bool s_eq(const std::string &a, const char (&b)[BSIZE]) noexcept
{
	return case_insensitive_compare(a, b) == 0;
}

inline bool startswith(const std::string &a, const std::string &b) noexcept
{
	return startswith(a.c_str(), a.size(), b.c_str(), b.size());
}

inline bool startswith(const std::string &a, const char* b, size_t bsize) noexcept
{
	return startswith(a.c_str(), a.size(), b, bsize);
}

template<size_t BSIZE>
inline bool startswith(const std::string &a, const char (&b)[BSIZE]) noexcept
{
	return startswith(a.c_str(), a.size(), b, sizeof(b) - 1);
}

inline bool endswith(const std::string &a, const std::string &b) noexcept
{
	return endswith(a.c_str(), a.size(), b.c_str(), b.size());
}

inline bool endswith(const std::string &a, const char* b, size_t bsize) noexcept
{
	return endswith(a.c_str(), a.size(), b, bsize);
}

template<size_t BSIZE>
inline bool endswith(const std::string &a, const char (&b)[BSIZE]) noexcept
{
	return endswith(a.c_str(), a.size(), b, sizeof(b) - 1);
}

class response_info;
std::string zeropad(int i, unsigned char width);
std::string serialize_date(time_t date);
std::vector<char> serialize_headers(const response_info& r);

void reset_response(response_info &response, int statusCode, std::string status);
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
};

class Hosting
{
public:
	Hosting() {}
	Hosting(const Hosting&) = delete;
	virtual ~Hosting() {}

	virtual bool request(const request_info &request, response_info &response) = 0;
};

class StaticHosting : public Hosting
{
public:
	StaticHosting(const std::string &root);
	~StaticHosting();
	virtual bool request(const request_info &request, response_info &response);
private:
	const MappedFile &file(const std::string&);

	std::unordered_map<std::string, MappedFile> _files;
	std::string _root;
};
