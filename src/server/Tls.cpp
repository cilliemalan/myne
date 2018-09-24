#include "pch.hpp"
#include "Listener.hpp"
#include "Tls.hpp"

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

	SSL_CTX_set_options(ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
}

TlsContext::~TlsContext()
{
	SSL_CTX_free(_ctx);
	EVP_cleanup();
}






TlsSocketStream::TlsSocketStream(int sfd, TlsContext &ctx)
	:SocketStream(sfd)
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

TlsSocketStream::~TlsSocketStream()
{

}

void TlsSocketStream::initialize()
{

}