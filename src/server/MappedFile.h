#pragma once

#include <string>

class MappedFile
{
public:
	MappedFile(std::string filename);
	MappedFile(const MappedFile &) = delete;
	MappedFile& operator=(const MappedFile&) = delete;
	MappedFile(MappedFile &&o)
		:pointer(o.pointer), filesize(o.filesize), fd(o.fd), filename(std::move(o.filename))
	{
		o.pointer = nullptr;
		o.filesize = 0;
		o.fd = 0;
	}

	~MappedFile();

	inline bool empty() const { return !pointer || !filesize || !fd; }
	inline char &operator[](size_t pos) { if (pos > filesize) throw std::out_of_range("pos out of range"); return *(pointer + pos); }
	inline const char &operator[](size_t pos) const { if (pos > filesize) throw std::out_of_range("pos out of range"); return *(pointer + pos); }
	inline size_t size() const { return filesize; }
	inline const std::string &name() const { return filename; }
private:
	char *pointer;
	size_t filesize;
	int fd;
	std::string filename;
};
