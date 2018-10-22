#include "pch.hpp"


int case_insensitive_compare(const char *a, size_t asize, const char *b, size_t bsize) noexcept
{
	if (!a && !b) return 0;
	if (b && !a) return -1;
	if (a && !b) return 1;
	if (asize == 0 && bsize == 0) return 0;
	else if (asize < bsize) return -1;
	else if (asize > bsize) return 1;
	else return strncasecmp(&a[0], &b[0], asize);
}

std::string resolvepath(const std::string &path) noexcept
{
	char buf[PATH_MAX + 1];
	if (!realpath(path.c_str(), buf) || !buf[0])
	{
		return std::string();
	}
	else
	{
		struct ::stat di;
		if (stat(buf, &di) == -1)
		{
			return std::string();
		}
		else if (S_ISREG(di.st_mode))
		{
			return std::string(buf);
		}
		else if (S_ISDIR(di.st_mode))
		{
			auto buflen = strlen(buf);
			if (buf[buflen - 1] == '/')
			{
				return std::string(buf, buflen);
			}
			else
			{
				if (buflen >= PATH_MAX)
				{
					return std::string();
				}
				else
				{
					buf[buflen] = '/';
					buf[++buflen] = 0;
					return std::string(buf, buflen);
				}
			}
		}

		return std::string(buf);
	}
}

std::string combinepath(const std::string &root, const std::string &suffix)
{
	if (root.size() == 0) throw std::runtime_error("root must be an absolute path");
	else if (root[0] != '/') throw std::runtime_error("root must be an absolute path");
	else if (root.size() == 1) throw std::runtime_error("root cannot be the system root");

	if (suffix.size() == 0) throw std::runtime_error("suffix cannot be empty");
	if (suffix[0] != '/') throw std::runtime_error("suffix must start with slash");

	if (root.size() + suffix.size() > PATH_MAX) throw std::runtime_error("path too long");

	std::string result;
	result.reserve(root.size() + suffix.size());
	result += root;
	result += suffix;

	return resolvepath(result);
}

bool startswith(const char* a, size_t asize, const char *b, size_t bsize) noexcept
{
	if (asize < bsize) return false;
	return strncasecmp(a, b, bsize) == 0;
}

bool endswith(const char* a, size_t asize, const char *b, size_t bsize) noexcept
{
	if (asize < bsize) return false;
	return strncasecmp(a + (asize - bsize), b, bsize) == 0;
}

static const char _fat[] = "\x1B[1;31m[fatal]";
static const char _err[] = "\x1B[0;31m[error]";
static const char _wrn[] = "\x1B[0;33m[warng]";
static const char _ifo[] = "\x1B[0;34m[ info]";
static const char _dbg[] = "\x1B[0;37m[debug]";
static const char _rst[] = "\x1B[0m\n";
void log(LogLevel ll, const char *format, ...)
{
	const char *_pfx;
	FILE *fp;
	switch (ll)
	{
	case LogLevel::Fatal:
		_pfx = _fat;
		fp = stderr;
		break;
	case LogLevel::Error:
		_pfx = _err;
		fp = stderr;
		break;
	case LogLevel::Warning:
		_pfx = _wrn;
		fp = stderr;
		break;
	case LogLevel::Info:
		_pfx = _ifo;
		fp = stdout;
		break;
	case LogLevel::Debug:
		_pfx = _dbg;
		fp = stdout;
		break;
	}

	time_t secs;
	auto t = localtime(&secs);
	time(&secs);
	fprintf(fp, "[%4d-%2d-%2d %2d:%2d:%2d]%s", t->tm_year, t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, _pfx);

	va_list args;
	va_start(args, format);
	vfprintf(fp, format, args);
	va_end(args);

	fwrite(_rst, 1, sizeof(_rst) - 1, fp);
}