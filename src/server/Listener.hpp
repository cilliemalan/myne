#pragma once

class Socket;
class SocketEventProducer;

typedef std::function<void(std::shared_ptr<Socket>, std::shared_ptr<SocketEventProducer>)> socket_handler;

// receives events that happen on a socket
class SocketEventReceiver
{
public:
	virtual ~SocketEventReceiver() { }

	virtual void read_avail() {}
	virtual void write_avail() {}
	virtual void closed() {}

protected:

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
	void reset() { _receiver.reset(); }
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
	LinuxSocket(int fd);
	~LinuxSocket() { }

	virtual ssize_t read(void* b, size_t max) override;
	virtual ssize_t write(const void* b, size_t amt) override;
	virtual void close() override;

	int fd() { return _fd; }
private:
	int _fd;
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

	void accept(std::shared_ptr<LinuxSocket>, socket_handler acceptHandler);
private:

	class LocalSocketEventProducer : public SocketEventProducer
	{
	public:
		void signal_read_avail() { SocketEventProducer::signal_read_avail(); }
		void signal_write_avail() { SocketEventProducer::signal_write_avail(); }
		void signal_closed() { SocketEventProducer::signal_closed(); }
		void reset() { SocketEventProducer::reset(); }
	};

	class LocalSocket : public Socket
	{
	public:
		LocalSocket(std::shared_ptr<LinuxSocket> socket, Acceptor& acceptor) :_socket(socket), _acceptor(acceptor) {}
		virtual ~LocalSocket() { close(); }

		virtual ssize_t read(void* b, size_t max) override
		{
			if (!_socket) return 0;
			return _socket->read(b, max);
		}

		virtual ssize_t write(const void* b, size_t amt) override
		{
			if (!_socket) return 0;
			return _socket->write(b, amt);
		}

		virtual void close() override
		{
			if (!_socket) return;

			auto iter = _acceptor._sockets.find(_socket->fd());
			if (iter != _acceptor._sockets.end())
			{
				_acceptor._sockets.erase(iter);
			}
			_socket->close();
			_socket.reset();
		}

	private:
		std::shared_ptr<LinuxSocket> _socket;
		Acceptor& _acceptor;
	};

	void worker();
	void stop();

	int _nr;
	int _pfd[2];
	int _efd;
	bool _running;
	std::thread _thread;
	std::unordered_map<int, std::pair<std::shared_ptr<LocalSocketEventProducer>,std::shared_ptr<LinuxSocket>>> _sockets;
};

// listens for traffic and forwards accepted sockets to an Acceptor
class Listener
{
public:
	Listener(const char* address, int port, socket_handler acceptHandler);
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
	socket_handler _acceptHandler;
};
