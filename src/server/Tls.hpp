#pragma once

static constexpr auto SSL_PROTOCOL_FLAGS = SSL_OP_ALL | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_COMPRESSION | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;
static constexpr auto SSL_CIPHER_LIST = "ECDH+AESGCM:ECDH+CHACHA20:ECDH+AES256:ECDH+AES128:!aNULL:!MD5:!DSS:!SHA1:!AESCCM:!DHE:!RSA";


std::string get_tls_error_string();
inline void tlserror(const char* msg) { error("%s: %s", msg, get_tls_error_string().c_str()); }
inline void tlserror(const std::string &msg) { logperror(msg.c_str()); }
inline void tlsfatal(const char* msg) { fatal("%s: %s", msg, get_tls_error_string().c_str()); }
inline void tlsfatal(const std::string &msg) { logpfatal(msg.c_str()); }
inline void tlswarning(const char* msg) { warning("%s: %s", msg, get_tls_error_string().c_str()); }
inline void tlswarning(const std::string &msg) { logpwarning(msg.c_str()); }


class Tls;
class TlsSocket;

class TlsContext
{
public:
	TlsContext(const char* certificate, const char* key, Tls *tls);
	~TlsContext();
	TlsContext(const TlsContext &) = delete;
	TlsContext& operator=(const TlsContext&) = delete;
	TlsContext(TlsContext &&);

	bool does_hostname_match(const std::string &hostname) const;

private:

	Tls *_tls;
	SSL_CTX* _ctx;
	X509* _cert;
	std::vector<std::string> _hostnames;

	friend class Tls;
	friend class TlsSocket;
	friend int ssl_servername_cb(SSL *s, int *ad, Tls *ctx);
};

class Tls
{
public:
	Tls();
	~Tls();
	Tls(const Tls &) = delete;
	Tls& operator=(const Tls&) = delete;
	Tls(Tls &&) = delete;

	void add_certificate(const char* certificate, const char* key);

	inline void add_handler(std::function<std::shared_ptr<SocketEventReceiver>(std::shared_ptr<TlsSocket> socket)> factory)
	{
		_handler_mapping.insert_or_assign(std::string(), factory);
	}

	inline void add_handler(std::string protocol, std::function<std::shared_ptr<SocketEventReceiver>(std::shared_ptr<TlsSocket> socket)> factory)
	{
		_handler_mapping.insert_or_assign(protocol, factory);
	}

	const TlsContext* first_ctx() const;
	const TlsContext* get_ctx_for_hostname(const std::string &hostname) const;
	const TlsContext* get_ctx_for_hostname(const char* hostname) const;
	const TlsContext* get_ctx_for_hostname(const char* hostname, size_t hostname_len) const;
private:
	int alpn_negotiate(SSL *s, unsigned char **out, unsigned char *outlen,
		const unsigned char *in, unsigned int inlen);

	std::shared_ptr<SocketEventReceiver> create_handler(std::string protocol, std::shared_ptr<TlsSocket> socket) const;

	std::vector<TlsContext> _contexts;
	std::unordered_map<std::string, std::function<std::shared_ptr<SocketEventReceiver>(std::shared_ptr<TlsSocket> socket)>> _handler_mapping;
	std::vector<unsigned char> alpn_data;

	friend class TlsSocket;
	friend int alpn_cb(SSL *s, const unsigned char **out, unsigned char *outlen,
		const unsigned char *in, unsigned int inlen, void *usr);
};

class TlsSocket : public SocketEventReceiver, public Socket
{
public:
	TlsSocket(std::shared_ptr<Socket> base, const Tls &ctx);
	~TlsSocket();

	virtual ssize_t read(void* b, size_t max) override;
	virtual ssize_t write(const void* b, size_t amt) override;
	virtual void close() override;

	virtual void read_avail() override;
	virtual void write_avail() override;
	virtual void closed() override;

	inline void set_shared_ptr(std::shared_ptr<TlsSocket> myself) { _myself = myself; }

private:
	std::shared_ptr<Socket> _socket;
	const Tls &_tls;

	std::weak_ptr<TlsSocket> _myself;

	ssize_t socket_read(void* b, size_t a);
	ssize_t socket_write(void* b, size_t a);

	void signal_read_avail();
	void signal_write_avail();
	void signal_closed();

	SSL* _ssl;
	BIO *_rbio;
	BIO *_wbio;

	std::shared_ptr<SocketEventReceiver> _connection;
};

class tls_error : public std::runtime_error
{
public:
	tls_error();
	tls_error(std::string msg);
};