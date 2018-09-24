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

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

#include <array>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <queue>

class system_err : std::runtime_error
{
public:
	system_err(int _errno)
		: std::runtime_error(_errno > 0 ? strerror(_errno) : "unspecified error")
	{}

	system_err()
		: system_err(errno)
	{}
};
