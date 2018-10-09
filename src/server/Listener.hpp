#pragma once

class SocketEventProducer;

// receives events that happen on a socket
class SocketEventReceiver
{
public:
	virtual ~SocketEventReceiver() { }

protected:
	virtual void read_avail() {}
	virtual void write_avail() {}
	virtual void closed() {}

	friend class SocketEventProducer;
};

// produces events that happen on a socket and allows the connection of a receiver
class SocketEventProducer
{
public:
	virtual ~SocketEventProducer() { }

	virtual void connect(std::shared_ptr<SocketEventReceiver> receiver) { _receiver = receiver; }
protected:
	std::shared_ptr<SocketEventReceiver> receiver() { return _receiver; }
	void signal_read_avail() { if (_receiver) _receiver->read_avail(); }
	void signal_write_avail() { if (_receiver) _receiver->write_avail(); }
	void signal_closed() { if (_receiver) { _receiver->closed(); _receiver.reset(); } }
private:
	std::shared_ptr< SocketEventReceiver> _receiver;
};

// represents a socket that can be read, written, or closed
// writes and reads may fail because the operation would block
class Socket
{
public:
	Socket() { }
	virtual ~Socket() { close(); }
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	virtual ssize_t read(void* b, size_t max) = 0;
	virtual ssize_t write(const void* b, size_t amt) = 0;
	virtual void close() {}
};

// an implementation for Socket on top of linux sockets.
class LinuxSocket : public Socket
{
public:
	LinuxSocket(int fd) : _fd(fd) { }
	~LinuxSocket() { }

	virtual ssize_t read(void* b, size_t max) override
	{
		if (!b || !max) return 0;

		ssize_t r = 0;
		while (r == 0) r = ::read(_fd, b, max);

		if (r <= 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return -1;
			}
			else
			{
				// socket broken
				perror("read");
				close();
				return 0;
			}
		}

		return r;
	}

	virtual ssize_t write(const void* b, size_t amt) override
	{
		if (!b || !amt) return 0;

		ssize_t r = 0;
		while (r == 0) r = ::write(_fd, b, amt);

		if (r <= 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return -1;
			}
			else
			{
				// socket broken
				perror("write");
				close();
				return 0;
			}
		}

		return r;
	}
	virtual void close() override { if (_fd) { ::close(_fd); _fd = 0; } }

	int fd() { return _fd; }
private:
	int _fd;
};

// a combination of a Socket and a SocketEventProducer
class ComboSocket : public SocketEventReceiver, public SocketEventProducer, public Socket
{
public:
	ComboSocket(std::shared_ptr<Socket> socket) :_socket(socket) {}
	virtual ~ComboSocket() { }

	virtual ssize_t read(void* b, size_t max) override;
	virtual ssize_t write(const void* b, size_t amt) override;
	virtual void close() override;

	virtual void read_avail() override;
	virtual void write_avail() override;
	virtual void closed() override;

	std::vector<char> read_all();
	ssize_t buffered_write(const void* data, size_t length);
	ssize_t buffered_write(std::vector<char> buff) { return buffered_write(&buff[0], buff.size()); }
protected:
	ssize_t flush_pending_writes();

	std::shared_ptr<Socket> socket() { return _socket; }
	template<typename T>
	std::shared_ptr<T> socket() { static_assert(std::is_base_of<Socket, T>()); return std::dynamic_pointer_cast<T>(_socket); }
private:

	std::shared_ptr<Socket> _socket;
	std::vector<char> _pending_writes;
};

// handles traffic on sockets
class Acceptor
{
public:
	Acceptor(int nr);
	Acceptor(const Acceptor&) = delete;
	Acceptor(Acceptor&&) { /*should never be called*/ abort(); }
	Acceptor& operator=(const Acceptor&) = delete;
	~Acceptor();

	std::shared_ptr<ComboSocket> accept(std::shared_ptr<LinuxSocket>, std::function<void(std::shared_ptr<ComboSocket>)> acceptHandler);
private:

	class LocalComboSocket : public ComboSocket
	{
	public:
		LocalComboSocket(std::shared_ptr<Socket> socket, Acceptor &acceptor, int fd) : ComboSocket(socket), _acceptor(acceptor), _fd(fd) {}
		~LocalComboSocket() { }

		virtual void close() override
		{
			auto sockref = _acceptor._sockets.find(_fd);
			if (sockref != _acceptor._sockets.end())
			{
				ComboSocket::close();
				ComboSocket::signal_closed();
				_acceptor._sockets.erase(sockref);
			}
		}
	private:
		friend class Acceptor;
		Acceptor &_acceptor;
		int _fd;
	};

	void worker();
	void stop();

	int _nr;
	int _pfd[2];
	int _efd;
	bool _running;
	std::thread _thread;
	std::unordered_map<int, std::shared_ptr<LocalComboSocket>> _sockets;
};

// listens for traffic and forwards accepted sockets to an Acceptor
class Listener
{
public:
	Listener(const char* address, int port, std::function<void(std::shared_ptr<ComboSocket>)> acceptHandler);
	~Listener();
	Listener(const Listener&) = delete;
	Listener& operator=(const Listener&) = delete;

	// stop the listener, close all connections
	void stop();

	// wait until the listener is explicitly stopped
	void wait();
private:

	static int initialize_epoll();
	int initialize_socket(const char* address, int port);
	std::vector<Acceptor> initialize_acceptors();
	void worker();

	int _pfd[2];
	int _port;
	int _efd;
	int _sfd;
	int _counter;
	bool _running = true;
	std::thread _thread;
	std::vector<Acceptor> _acceptors;
	std::function<void(std::shared_ptr<ComboSocket>)> _acceptHandler;
};
