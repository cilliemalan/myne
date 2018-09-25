#include "pch.hpp"
#include "Listener.hpp"

class LineEchoer : public SocketEventReceiver
{
public:
	LineEchoer(std::shared_ptr<ComboSocket> socket) : _socket(socket) {}
	~LineEchoer() { printf("LineEchoer destroyed\n"); }

	virtual void read_avail()
	{
		auto data = _socket->read_all();

		if (data.size())
		{
			auto bufflen = _lineBuffer.size();
			_lineBuffer.insert(_lineBuffer.end(), data.begin(), data.end());

			write_lines(bufflen);
		}
	}

	virtual void write_avail() { }

	virtual void closed() {
		printf("line echo has closed\n");
		_socket.reset();
	}

private:

	void write_lines(size_t start_scan_at = 0)
	{
		auto lbs = _lineBuffer.size();
		if (lbs > start_scan_at)
		{
			size_t len = start_scan_at + 1;
			for (auto pc = _lineBuffer.begin() + start_scan_at; pc != _lineBuffer.end(); pc++, len++)
			{
				if (*pc == '\n')
				{
					if (memcmp(&_lineBuffer[0], "close", 5) == 0)
					{
						printf("closing socket\n");
						_socket->close();
						return;
					}
					else
					{
						_socket->buffered_write(&_lineBuffer[0], len);
						memcpy(&_lineBuffer[0], &_lineBuffer[len], lbs - len);
						_lineBuffer.resize(lbs - len);
						write_lines();
						break;
					}
				}
			}
		}
	}

	std::vector<char> _lineBuffer;
	std::shared_ptr<ComboSocket> _socket;
};



Listener* gl = nullptr;
void sig_handler(int signum)
{
	printf("got SIGINT\n");
	if (gl)
	{
		printf("stopping the thing\n");
		gl->stop();
	}
}

int main(int argc, char *argv[])
{
	printf("starting listener\n");
	Listener l(nullptr, 80, [](std::shared_ptr<ComboSocket> producer)
	{
		producer->connect(std::make_shared<LineEchoer>(producer));
	});

	gl = &l;
	signal(SIGINT, sig_handler);
	l.wait();
	gl = nullptr;

	return 0;
}
