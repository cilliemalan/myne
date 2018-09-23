#pragma once


class SocketStream
{
public:
	SocketStream(int fd);
	virtual ~SocketStream();

	SocketStream(const SocketStream&) = delete;
	SocketStream& operator=(const SocketStream&) = delete;

	virtual void receive(void* data, int length);
	virtual void ready();
	virtual void close();

	inline bool valid() { return _fd > 0; }
	inline int sfd() { return _fd; }

protected:
	int send(void* data, int length);

private:
	int _fd;
};

class FunctionalSocketStream : public SocketStream
{
public:
	FunctionalSocketStream(int fd, std::function<void(int fd, void* data, int length, std::function<int(void*, int)> write)> receive)
		:SocketStream(fd), _receive(receive)
	{
	}

	virtual void receive(void* data, int length) override
	{
		_receive(sfd(), data, length, [this](void* data, int length) { return this->send(data, length); });
	}
private:
	std::function<void(int fd, void* data, int length, std::function<int(void*, int)> write)> _receive;
};

class Acceptor
{
public:
	Acceptor(int a);
	Acceptor(const Acceptor&) = delete;
	Acceptor(Acceptor&&) { /*should never be called*/ abort(); }
	Acceptor& operator=(const Acceptor&) = delete;
	~Acceptor();

	void accept(SocketStream* stream);
private:

	void worker();
	void stop();

	int _efd;
	bool _running;
	std::thread _thread;
	std::unordered_map<int, SocketStream*> _sockets;
};

class Listener
{
public:
	Listener(const char* address, int port, std::function<SocketStream*(int)> streamFactory);
	~Listener();
	Listener(const Listener&) = delete;
	Listener& operator=(const Listener&) = delete;

private:

	static int initialize_epoll();
	int initialize_socket(const char* address, int port);
	std::vector<Acceptor> initialize_acceptors();
	void worker();
	void stop();

	int _port;
	int _efd;
	int _sfd;
	int _counter;
	bool _running = true;
	std::thread _thread;
	std::vector<Acceptor> _acceptors;
	std::function<SocketStream*(int)> _streamFactory;
};
