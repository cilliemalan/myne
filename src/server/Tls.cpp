#include "pch.hpp"
#include "Listener.hpp"
#include "Tls.hpp"

static int _get_tls_error_string_cb(const char *str, size_t len, void *u)
{
	std::string &s = *reinterpret_cast<std::string *>(u);
	s += std::string(str, len);
	return 0;
}

std::string get_tls_error_string()
{
	std::string output;
	ERR_print_errors_cb(_get_tls_error_string_cb, &output);
	return output;
}



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
		throw tls_error("could not open certificate file");
	}

	auto cert = PEM_read_bio_X509_AUX(bio, nullptr, nullptr, nullptr);
	if (!cert)
	{
		BIO_free(bio);
		throw tls_error("could not load certificate");
	}

	BIO_free(bio);
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

int alpn_cb(SSL *s, const unsigned char **out, unsigned char *outlen,
	const unsigned char *in, unsigned int inlen, void *usr)
{
	Tls* tls = static_cast<Tls*>(usr);
	auto result = tls->alpn_negotiate(s, const_cast<unsigned char**>(out), outlen, in, inlen);
	return result;
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
		throw tls_error("Unable to create SSL context");
	}

	auto cert = load_cert(certificate);
	if (SSL_CTX_use_certificate(ctx, cert) <= 0)
	{
		throw tls_error("Unable to use certificate");
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0)
	{
		throw tls_error("Unable to read key");
	}

	if (SSL_CTX_check_private_key(ctx) != 1)
	{
		throw tls_error("SSL_CTX_check_private_key failed");
	}

	_cert = cert;
	_hostnames = get_hostnames(cert);

	// disable anything below tls 1.2
	SSL_CTX_set_options(ctx, SSL_PROTOCOL_FLAGS);
	SSL_CTX_set_cipher_list(ctx, SSL_CIPHER_LIST);
	SSL_CTX_set_tlsext_servername_callback(ctx, ssl_servername_cb);
	SSL_CTX_set_tlsext_servername_arg(ctx, tls);
	SSL_CTX_set_alpn_select_cb(ctx, alpn_cb, tls);

	// ecdh for temp stuff
	auto ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if (!ecdh)
	{
		throw tls_error("EC_KEY_new_by_curv_name failed");
	}
	SSL_CTX_set_tmp_ecdh(ctx, ecdh);
	EC_KEY_free(ecdh);



	_ctx = ctx;

}

TlsContext::TlsContext(TlsContext && old)
	: _tls(old._tls), _ctx(old._ctx), _cert(nullptr)
{
	old._ctx = nullptr;
	old._tls = nullptr;
}

TlsContext::~TlsContext()
{
	if (_cert) { X509_free(_cert); }
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

int Tls::alpn_negotiate(SSL *s, unsigned char **out, unsigned char *outlen,
	const unsigned char *in, unsigned int inlen)
{
	if (alpn_data.size() == 0)
	{
		if (_handler_mapping.size() == 0)
		{
			return SSL_TLSEXT_ERR_NOACK;
		}
		else
		{
			for (const auto &s : _handler_mapping)
			{
				auto &protocol = s.first;
				if (protocol.size() == 0) continue;

				auto ix = alpn_data.size();
				unsigned char len = static_cast<unsigned char>(protocol.length());
				alpn_data.resize(ix + len + 1);
				alpn_data[ix] = len;
				memcpy(&alpn_data[ix + 1], &protocol[0], len);
			}
		}
	}

	if (alpn_data.size() != 0)
	{
		if (SSL_select_next_proto(out, outlen, &alpn_data[0], static_cast<unsigned int>(alpn_data.size()), in, inlen))
		{
			return SSL_TLSEXT_ERR_OK;
		}
	}

	return SSL_TLSEXT_ERR_NOACK;
}


std::shared_ptr<SocketEventReceiver> Tls::create_handler(std::string protocol, std::shared_ptr<TlsSocket> socket) const
{
	auto mapping = _handler_mapping.find(protocol);
	if (mapping != _handler_mapping.cend())
	{
		return mapping->second(socket);
	}

	// this function should never receive an invalid protocol.
	throw std::runtime_error("invalid protocol");
}


TlsSocket::TlsSocket(std::shared_ptr<Socket> base, const Tls &tls)
	:_socket(base),
	_tls(tls)
{
	_rbio = BIO_new(BIO_s_mem());
	_wbio = BIO_new(BIO_s_mem());
	_ssl = SSL_new(tls.first_ctx()->_ctx);
	if (!_rbio || !_wbio || !_ssl)
	{
		throw tls_error("failed to initialize ssl");
	}

	SSL_set_accept_state(_ssl);
	SSL_set_bio(_ssl, _rbio, _wbio);
}

TlsSocket::~TlsSocket()
{
	close();
}

ssize_t TlsSocket::read(void* b, size_t max)
{
	if (!b || !max || !_ssl) return 0;

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
			tlswarning("SSL read error: read from read bio failed\n");
			close();
			return 0;
		}
	}

	return amt;
}

ssize_t TlsSocket::write(const void* b, size_t amt)
{
	if (!b || !amt || !_ssl || !_socket) return 0;

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
			tlswarning("SSL read error: failed to fill write BIO\n");
			close();
			return 0;
		}
	}
}

void TlsSocket::close()
{
	if (_ssl) { SSL_free(_ssl); _rbio = _wbio = nullptr; _ssl = nullptr; }
	if (_socket) { _socket->close(); _socket.reset(); }
	signal_closed();
}

void TlsSocket::read_avail()
{
	if (!_ssl || !_socket) return;

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
					tlswarning("SSL read error: BIO write to read bio failed\n");
					close();
					return;
				}
				else if (consumed == 0 && amt > 0)
				{
					tlswarning("SSL read error: failed to fill read BIO\n");
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
		if (r <= 0)
		{
			auto status = SSL_get_error(_ssl, r);

			if (status == SSL_ERROR_WANT_WRITE || status == SSL_ERROR_WANT_READ)
			{
				// will write on next write cycle
				return;
			}
			else
			{
				tlswarning("SSL read error: unexpected result from accept");
				close();
				return;
			}
		}
	}

	// connect to the appropriate handler once init is finished
	if (!_connection)
	{
		if (SSL_is_init_finished(_ssl))
		{
			auto shared_me = _myself.lock();
			if (shared_me)
			{
				unsigned char* proto = nullptr;
				unsigned int len;
				SSL_get0_alpn_selected(_ssl, const_cast<const unsigned char**>(&proto), &len);
				if (len)
				{
					std::string sproto(reinterpret_cast<char*>(proto), len);
					_connection = _tls.create_handler(sproto, shared_me);
				}
				else
				{
					close();
				}
			}
			else
			{
				close();
			}
		}
	}

	if (_connection)
	{
		signal_read_avail();
		signal_write_avail();
	}
}

void TlsSocket::write_avail()
{
	if (!_ssl || !_socket) return;

	bool wouldblock;
	size_t amt_avail;
	do
	{
		// write as much data as we can from what's available in the write BIO
		void* ptr;
		amt_avail = BIO_get_mem_data(_wbio, &ptr);
		if (amt_avail > 0)
		{
			auto amount_written = _socket->write(ptr, amt_avail);
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
			tlswarning("SSL read error: BIO read from write bio failed\n");
			close();
			return;
		}
	} while (!wouldblock || amt_avail > 0);
}

void TlsSocket::closed()
{
	close();
}

ssize_t TlsSocket::socket_read(void* b, size_t a)
{
	return _socket ? _socket->read(b, a) : 0;
}

ssize_t TlsSocket::socket_write(void* b, size_t a)
{
	return _socket ? _socket->write(b, a) : 0;
}

void TlsSocket::signal_read_avail()
{
	if (_connection)
	{
		_connection->read_avail();
	}
}

void TlsSocket::signal_write_avail()
{
	if (_connection)
	{
		_connection->write_avail();
	}
}

void TlsSocket::signal_closed()
{
	if (_connection)
	{
		_connection->closed();
		_connection.reset();
	}
}

tls_error::tls_error()
	: std::runtime_error(get_tls_error_string())
{}

tls_error::tls_error(std::string msg)
	: std::runtime_error(msg + ": " + get_tls_error_string())
{
}

