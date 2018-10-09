#include "pch.hpp"
#include "Listener.hpp"
#include "Tls.hpp"

void BIO_advance(BIO* bio, size_t howfar)
{
	if (howfar > 0)
	{
		BIO_clear_retry_flags(bio);
		BUF_MEM* ptr;
		BIO_get_mem_ptr(bio, &ptr);
		ptr->data += howfar;
		ptr->length -= howfar;
	}
}

X509 *load_cert(const char *file)
{
	auto *bio = BIO_new_file(file, "r");
	if (!bio)
	{
		throw std::runtime_error("could not open certificate file");
	}

	auto cert = PEM_read_bio_X509_AUX(bio, nullptr, nullptr, nullptr);
	if (!cert)
	{
		throw std::runtime_error("could not load certificate");
	}

	return cert;
}

int ssl_servername_cb(SSL *s, int *ad, Tls *tls)
{
	const char *servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

	if (servername)
	{
		auto ctx = tls->get_ctx_for_hostname(servername);
		if (ctx)
		{
			SSL_set_SSL_CTX(s, ctx->_ctx);
			return SSL_TLSEXT_ERR_OK;
		}
	}

	return SSL_TLSEXT_ERR_NOACK;
}

std::vector<std::string> get_hostnames(X509* cert)
{
	char buf[1024];
	std::vector<std::string> names;

	auto name = X509_get_subject_name(cert);
	auto namelen = X509_NAME_get_text_by_NID(name, NID_commonName, buf, sizeof(buf));
	names.emplace_back(buf, namelen);

	GENERAL_NAMES *gs = static_cast<GENERAL_NAMES*>(X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));

	for (int i = 0; i < sk_GENERAL_NAME_num(gs); i++)
	{
		GENERAL_NAME *g = sk_GENERAL_NAME_value(gs, i);
		if (g->type == GEN_DNS)
		{
			auto dns = g->d.dNSName;
			if (dns->data && dns->length > 0)
			{
				std::string s(reinterpret_cast<const char*>(dns->data), dns->length);
				if (std::find(names.begin(), names.end(), s) == names.end())
				{
					names.push_back(s);
				}
			}
		}
	}

	GENERAL_NAMES_free(gs);
	return names;
}

TlsContext::TlsContext(const char* certificate, const char* key, Tls *tls)
	:_tls(tls)
{
	const SSL_METHOD* method = SSLv23_server_method();
	SSL_CTX* ctx = SSL_CTX_new(method);
	if (!ctx)
	{
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		throw std::runtime_error("Unable to create SSL context");
	}

	auto cert = load_cert(certificate);
	if (SSL_CTX_use_certificate(ctx, cert) <= 0)
	{
		ERR_print_errors_fp(stderr);
		throw std::runtime_error("Unable to use certificate");
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		throw std::runtime_error("Unable to read key");
	}

	if (SSL_CTX_check_private_key(ctx) != 1)
	{
		throw std::runtime_error("SSL_CTX_check_private_key failed");
	}

	_cert = cert;
	_hostnames = get_hostnames(cert);

	// disable anything below tls 1.2
	SSL_CTX_set_options(ctx, SSL_PROTOCOL_FLAGS);
	SSL_CTX_set_cipher_list(ctx, SSL_CIPHER_LIST);
	SSL_CTX_set_tlsext_servername_callback(ctx, ssl_servername_cb);
	SSL_CTX_set_tlsext_servername_arg(ctx, tls);

	_ctx = ctx;
}

TlsContext::TlsContext(TlsContext && old)
	:_ctx(old._ctx), _tls(old._tls)
{
	old._ctx = nullptr;
	old._tls = nullptr;
}

TlsContext::~TlsContext()
{
	if (_ctx) { SSL_CTX_free(_ctx); _ctx = nullptr; }
}

bool TlsContext::does_hostname_match(const std::string &hostname) const
{
	return std::find(_hostnames.begin(), _hostnames.end(), hostname) != _hostnames.end();
}



// Tls

Tls::Tls()
{
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();
}

Tls::~Tls()
{
}

void Tls::add_certificate(const char* certificate, const char* key)
{
	_contexts.emplace_back(certificate, key, this);
}

const TlsContext* Tls::first_ctx() const
{
	if (_contexts.size() == 0)
	{
		return nullptr;
	}
	else
	{
		return &_contexts[0];
	}
}

const TlsContext* Tls::get_ctx_for_hostname(const std::string &hostname) const
{
	for (const auto &ctx : _contexts)
	{
		if (ctx.does_hostname_match(hostname))
		{
			return &ctx;
		}
	}

	return nullptr;
}

const TlsContext* Tls::get_ctx_for_hostname(const char* hostname) const
{
	return get_ctx_for_hostname(std::string(hostname));
}

const TlsContext* Tls::get_ctx_for_hostname(const char* hostname, size_t hostname_len) const
{
	return get_ctx_for_hostname(std::string(hostname, hostname_len));
}


TlsComboSocket::TlsComboSocket(std::shared_ptr<Socket> base, const Tls &tls)
	:ComboSocket(base),
	_tls(tls)
{
	_rbio = BIO_new(BIO_s_mem());
	_wbio = BIO_new(BIO_s_mem());
	_ssl = SSL_new(tls.first_ctx()->_ctx);
	if (!_rbio || !_wbio || !_ssl)
	{
		throw std::runtime_error("failed to initialize ssl");
	}

	SSL_set_accept_state(_ssl);
	SSL_set_bio(_ssl, _rbio, _wbio);
}

TlsComboSocket::~TlsComboSocket()
{
	if (_ssl) { SSL_free(_ssl); _rbio = _wbio = nullptr; _ssl = nullptr; }
}

ssize_t TlsComboSocket::read(void* b, size_t max)
{
	if (!b || !max) return 0;

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

ssize_t TlsComboSocket::write(const void* b, size_t amt)
{
	if (!b || !amt) return 0;

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
		else if (amt > 0)
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
		else
		{
			// socket broken
			close();
			return;
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

	bool wouldblock;
	size_t amt_avail;
	do
	{
		// write as much data as we can from what's available in the write BIO
		void* ptr;
		amt_avail = BIO_get_mem_data(_wbio, &ptr);
		if (amt_avail > 0)
		{
			auto amount_written = socket<ComboSocket>()->write(ptr, amt_avail);
			if (amount_written > 0)
			{
				BIO_advance(_wbio, amount_written);
			}
			else if (amount_written == 0)
			{
				close();
				return;
			}
			else
			{
				wouldblock = true;
			}
		}
		else if (amt_avail == 0)
		{
			wouldblock = false;
			break;
		}
		else
		{
			printf("SSL read error: BIO read from write bio failed\n");
			close();
			return;
		}
	} while (!wouldblock || amt_avail > 0);
}

void TlsComboSocket::closed()
{
	ComboSocket::closed();
}
