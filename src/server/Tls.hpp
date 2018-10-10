#pragma once

static constexpr auto SSL_PROTOCOL_FLAGS = SSL_OP_ALL | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
static constexpr auto SSL_CIPHER_LIST = "ECDH+AESGCM:ECDH+CHACHA20:ECDH+AES256:ECDH+AES128:!aNULL:!MD5:!DSS:!SHA1:!AESCCM:!DHE:!RSA";

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

	X509* _cert;
	std::vector<std::string> _hostnames;
	SSL_CTX* _ctx;
	Tls *_tls;

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

	const TlsContext* first_ctx() const;
	const TlsContext* get_ctx_for_hostname(const std::string &hostname) const;
	const TlsContext* get_ctx_for_hostname(const char* hostname) const;
	const TlsContext* get_ctx_for_hostname(const char* hostname, size_t hostname_len) const;
private:

	std::vector<TlsContext> _contexts;

	friend class TlsSocket;
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

	void connect(std::shared_ptr<SocketEventReceiver> consumer) { _producer.connect(consumer); }
private:

	class LocalSocketEventProducer : public SocketEventProducer
	{
	public:
		void signal_read_avail() { SocketEventProducer::signal_read_avail(); }
		void signal_write_avail() { SocketEventProducer::signal_write_avail(); }
		void signal_closed() { SocketEventProducer::signal_closed(); }
		void reset() { SocketEventProducer::reset(); }
	};

	std::shared_ptr<Socket> _socket;
	const Tls &_tls;

	ssize_t socket_read(void* b, size_t a) { return _socket ? _socket->read(b, a) : 0; }
	ssize_t socket_write(void* b, size_t a) { return _socket ? _socket->write(b, a) : 0; }
	
	SSL* _ssl;
	BIO *_rbio;
	BIO *_wbio;

	LocalSocketEventProducer _producer;
};
