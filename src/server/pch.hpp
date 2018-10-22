#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/objects.h>
#include <openssl/ts.h>
#include <openssl/pkcs7.h>

#include <nghttp2/nghttp2.h>

#include <array>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <queue>
#include <atomic>

class system_err : std::runtime_error
{
public:
	system_err(int _errno)
		: std::runtime_error(_errno > 0 ? strerror(_errno) : "unspecified error")
	{
		auto error_message = strerror(_errno);
		printf("encountered error: %s\n", error_message);
	}

	system_err()
		: system_err(errno)
	{}
};




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
inline int case_insensitive_compare(const std::string &a, const char(&b)[BSIZE]) noexcept
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
inline bool s_eq(const char(&a)[ASIZE], const char(&b)[BSIZE]) noexcept
{
	return case_insensitive_compare(a, sizeof(a) - 1, b, sizeof(b) - 1) == 0;
}

template<size_t BSIZE>
inline bool s_eq(const std::string &a, const char(&b)[BSIZE]) noexcept
{
	return case_insensitive_compare(a, b) == 0;
}

template<size_t BSIZE>
inline bool s_eq(const char *a, size_t asize, const char(&b)[BSIZE]) noexcept
{
	return case_insensitive_compare(a, asize, b, sizeof(b) - 1) == 0;
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
inline bool startswith(const std::string &a, const char(&b)[BSIZE]) noexcept
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
inline bool endswith(const std::string &a, const char(&b)[BSIZE]) noexcept
{
	return endswith(a.c_str(), a.size(), b, sizeof(b) - 1);
}


enum class LogLevel
{
	Fatal,
	Error,
	Warning,
	Info,
	Debug
};

void log(LogLevel ll, const char *format, ...);
#define fatal(...) log(LogLevel::Fatal, __VA_ARGS__)
#define error(...) log(LogLevel::Error, __VA_ARGS__)
#define warning(...) log(LogLevel::Warning, __VA_ARGS__)
#define info(...) log(LogLevel::Info, __VA_ARGS__)
#define debug(...) log(LogLevel::Debug, __VA_ARGS__)
