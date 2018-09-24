#include "pch.hpp"
#include "Listener.hpp"


class LineEcho : public SocketStream
{
public:
	LineEcho(int fd) : SocketStream(fd) {}

	virtual void receive(void* data, int length)
	{
		auto p = static_cast<char*>(data);
		auto bufflen = _lineBuffer.size();
		_lineBuffer.insert(_lineBuffer.end(), p, p + length);

		write_lines(bufflen);
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
					buffered_send(&_lineBuffer[0], len);
					memcpy(&_lineBuffer[0], &_lineBuffer[len], lbs - len);
					_lineBuffer.resize(lbs - len);
					write_lines();
					break;
				}
			}
		}
	}

	std::vector<char> _lineBuffer;
};

int main(int argc, char *argv[])
{
	std::function<SocketStream*(int)> streamFactory = [](int fd) { return new LineEcho(fd); };

	printf("starting listener\n");
	Listener l(nullptr, 80, streamFactory);

	printf("press a to exit\n");
	while (getchar() != 'a');
	printf("exiting\n");
	return 0;
}
