#pragma once

static constexpr auto SSL_PROTOCOL_FLAGS = SSL_OP_ALL | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
static constexpr auto SSL_CIPHER_LIST = "ECDH+AESGCM:ECDH+CHACHA20:ECDH+AES256:ECDH+AES128:!aNULL:!MD5:!DSS:!SHA1:!AESCCM:!DHE:!RSA";

class TlsContext
{
public:
	TlsContext(const char* certificate, const char* key);
	~TlsContext();

private:

	SSL_CTX* _ctx;
	const char* _cert;
	const char* _key;

	friend class TlsComboSocket;
};

class TlsComboSocket : public ComboSocket
{
public:
	TlsComboSocket(std::shared_ptr<Socket> base, const TlsContext &ctx);

	virtual ssize_t read(void* b, size_t max);
	virtual ssize_t write(void* b, size_t amt);
	virtual void close();

	virtual void read_avail();
	virtual void write_avail();
	virtual void closed();

private:
	const TlsContext &_ctx;

	ssize_t socket_read(void* b, size_t a) { return ComboSocket::read(b, a); }
	ssize_t socket_write(void* b, size_t a) { return ComboSocket::write(b, a); }
	void socket_close() { ComboSocket::close(); }

	SSL* _ssl;
	BIO *_rbio;
	BIO *_wbio;
};
