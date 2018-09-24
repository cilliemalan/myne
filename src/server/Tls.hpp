#pragma once

class TlsContext
{
public:
	TlsContext(const char* certificate, const char* key);
	~TlsContext();

private:

	SSL_CTX* _ctx;
	const char* _cert;
	const char* _key;

	friend class TlsSocketStream;
};

class TlsSocketStream : public SocketStream
{
public:
	TlsSocketStream(int sfd, TlsContext &ctx);
	~TlsSocketStream();
private:
	void initialize();

	SSL* _ssl;
	BIO *_rbio; // SSL reads from, we write to.
	BIO *_wbio; // SSL writes to, we read from.
};