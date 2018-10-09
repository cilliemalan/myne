#include "pch.hpp"
#include "MappedFile.h"

MappedFile::MappedFile(std::string filename)
	:filename(filename)
{
	fd = open(filename.c_str(), O_RDONLY);
	if (fd == -1) throw std::runtime_error("could not open file");

	struct stat filestats;
	if (fstat(fd, &filestats) != 0)
	{
		close(fd);
		throw std::runtime_error("could not stat file");
	}
	filesize = filestats.st_size;

	pointer = static_cast<char*>(mmap(nullptr, filesize, PROT_READ, MAP_SHARED, fd, 0));
	if (!pointer || pointer == MAP_FAILED)
	{
		close(fd);
		throw std::runtime_error("could not map file");
	}
}

MappedFile::~MappedFile()
{
	if (filesize && pointer) { munmap(pointer, filesize); pointer = nullptr; filesize = 0; }
	if (fd) { close(fd); fd = 0; }
}
