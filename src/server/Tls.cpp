#include "pch.hpp"
#include "Listener.hpp"
#include "Tls.hpp"


// TlsContext

TlsContext::TlsContext(const char* certificate, const char* key)
{
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();

	const SSL_METHOD* method = SSLv23_server_method();
	SSL_CTX* ctx = SSL_CTX_new(method);
	if (!ctx) {
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		throw std::runtime_error("Unable to create SSL context");
	}

	if (SSL_CTX_use_certificate_file(ctx, certificate, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		throw std::runtime_error("Unable to read certificate");
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		throw std::runtime_error("Unable to read key");
	}

	if (SSL_CTX_check_private_key(ctx) != 1)
	{
		throw std::runtime_error("SSL_CTX_check_private_key failed");
	}

	// disable anything below tls 1.2
	SSL_CTX_set_options(ctx, SSL_PROTOCOL_FLAGS);
	SSL_CTX_set_cipher_list(ctx, SSL_CIPHER_LIST);

	_ctx = ctx;

}

TlsContext::~TlsContext()
{
	SSL_CTX_free(_ctx);
	EVP_cleanup();
}



TlsComboSocket::TlsComboSocket(std::shared_ptr<Socket> base, const TlsContext &ctx)
	:ComboSocket(base),
	_ctx(ctx)
{
	_rbio = BIO_new(BIO_s_mem());
	_wbio = BIO_new(BIO_s_mem());
	_ssl = SSL_new(ctx._ctx);
	if (!_rbio || !_wbio || !_ssl)
	{
		throw std::runtime_error("failed to initialize ssl");
	}

	SSL_set_accept_state(_ssl);
	SSL_set_bio(_ssl, _rbio, _wbio);
}

ssize_t TlsComboSocket::read(void* b, size_t max)
{
	auto amt = SSL_read(_ssl, b, static_cast<int>(max));
	if (amt == 0)
	{
		// act like a would-block
		return -1;
	}
	else if (amt < 0)
	{
		auto status = SSL_get_error(_ssl, amt);

		if (status == SSL_ERROR_WANT_READ ||
			status == SSL_ERROR_WANT_WRITE ||
			status == SSL_ERROR_NONE)
		{
			// act like wouldblock
			return -1;
		}
		else
		{
			printf("SSL read error: read from read bio failed\n");
			close();
			return 0;
		}
	}

	return amt;
}

ssize_t TlsComboSocket::write(void* b, size_t amt)
{
	auto amtwritten = SSL_write(_ssl, b, static_cast<int>(amt));

	if (amtwritten > 0)
	{
		// force the socket to write even if it may not be possible
		write_avail();
		return amtwritten;
	}
	else
	{
		auto status = SSL_get_error(_ssl, amtwritten);

		if (status == SSL_ERROR_WANT_READ ||
			status == SSL_ERROR_WANT_WRITE ||
			status == SSL_ERROR_NONE)
		{
			// act like wouldblock
			return -1;
		}
		else
		{
			printf("SSL read error: failed to fill write BIO\n");
			close();
			return 0;
		}
	}
}

void TlsComboSocket::close()
{
	ComboSocket::close();
}

void TlsComboSocket::read_avail()
{
	for (;;)
	{
		char buf[4096];

		auto amt = socket_read(buf, sizeof(buf));

		if (amt <= 0)
		{
			// no data available
			break;
		}
		else
		{
			char* p = buf;
			while (amt > 0)
			{
				auto consumed = BIO_write(_rbio, p, static_cast<int>(amt));
				if (consumed < 0)
				{
					printf("SSL read error: BIO write to read bio failed\n");
					close();
					return;
				}
				else if (consumed == 0 && amt > 0)
				{
					printf("SSL read error: failed to fill read BIO\n");
					close();
					return;
				}

				p += consumed;
				amt -= consumed;
			}
		}
	}

	// SSL state machine
	if (!SSL_is_init_finished(_ssl))
	{
		auto r = SSL_accept(_ssl);
		auto status = SSL_get_error(_ssl, r);

		if (status == SSL_ERROR_WANT_READ)
		{
			// need to wait for more data
			return;
		}
		else if (status == SSL_ERROR_WANT_WRITE)
		{
			// will write on next write cycle
			return;
		}
		else if (status == SSL_ERROR_NONE)
		{
			// init may be done already
		}
		else
		{
			printf("SSL read error: unexpected result from accept\n");
			close();
			return;
		}
	}

	if (SSL_is_init_finished(_ssl))
	{
		signal_read_avail();
		signal_write_avail();
	}
}

void TlsComboSocket::write_avail()
{
	flush_pending_writes();

	bool wouldblock = false;

	// write as much data as we can from what's available in the write BIO
	char write_buffer[4096];
	int amt_avail;
	do {
		amt_avail = BIO_read(_wbio, write_buffer, sizeof(write_buffer));
		if (amt_avail > 0)
		{
			auto amount_flushed = socket<ComboSocket>()->buffered_write(write_buffer, amt_avail);
			if (amount_flushed < amt_avail)
			{
				// the write buffered, wait for the next write cycle
				wouldblock = true;
				return;
			}
		}
		else if (BIO_should_retry(_wbio))
		{
			// TODO: ??
		}
		else
		{
			printf("SSL read error: BIO read from write bio failed\n");
			close();
			return;
		}
	} while (amt_avail > 0);

	if (!wouldblock && SSL_is_init_finished(_ssl))
	{
		// the write bio has been flushed, signal that a write is available
		signal_write_avail();
	}
}

void TlsComboSocket::closed()
{
	ComboSocket::closed();
}
