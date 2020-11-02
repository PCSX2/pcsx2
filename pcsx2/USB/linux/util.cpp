#include "util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

bool file_exists(std::string path)
{
	struct stat s;
	return !stat(path.c_str(), &s) && !S_ISDIR(s.st_mode);
}

bool dir_exists(std::string path)
{
	struct stat s;
	return !stat(path.c_str(), &s) && S_ISDIR(s.st_mode);
}
