#include "pch.hpp"
#include "Listener.hpp"

int main(int argc, char *argv[])
{
	std::function<SocketStream*(int)> streamFactory = [](int fd) { return new FunctionalSocketStream(fd, 
		[](int fd, void* data, int length, std::function<int(void* data, int length)> write)
	{
		write(data, length);
	}); };

	printf("starting listener\n");
	Listener l(nullptr, 80, streamFactory);

	printf("press a to exit\n");
	while (getchar() != 'a');
	printf("exiting\n");
	return 0;
}