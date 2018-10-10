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
