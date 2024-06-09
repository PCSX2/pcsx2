#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if !CPUINFO_MOCK
#error This file should be built only in mock mode
#endif

#include <arm/linux/api.h>
#include <arm/midr.h>
#include <cpuinfo-mock.h>
#include <cpuinfo/log.h>

static struct cpuinfo_mock_file* cpuinfo_mock_files = NULL;
static uint32_t cpuinfo_mock_file_count = 0;

void CPUINFO_ABI cpuinfo_mock_filesystem(struct cpuinfo_mock_file* files) {
	cpuinfo_log_info("filesystem mocking enabled");
	uint32_t file_count = 0;
	while (files[file_count].path != NULL) {
		/* Indicate that file is not opened */
		files[file_count].offset = SIZE_MAX;
		file_count += 1;
	}
	cpuinfo_mock_files = files;
	cpuinfo_mock_file_count = file_count;
}

int CPUINFO_ABI cpuinfo_mock_open(const char* path, int oflag) {
	if (cpuinfo_mock_files == NULL) {
		cpuinfo_log_warning("cpuinfo_mock_open called without mock filesystem; redictering to open");
		return open(path, oflag);
	}

	for (uint32_t i = 0; i < cpuinfo_mock_file_count; i++) {
		if (strcmp(cpuinfo_mock_files[i].path, path) == 0) {
			if (oflag != O_RDONLY) {
				errno = EACCES;
				return -1;
			}
			if (cpuinfo_mock_files[i].offset != SIZE_MAX) {
				errno = ENFILE;
				return -1;
			}
			cpuinfo_mock_files[i].offset = 0;
			return (int)i;
		}
	}
	errno = ENOENT;
	return -1;
}

int CPUINFO_ABI cpuinfo_mock_close(int fd) {
	if (cpuinfo_mock_files == NULL) {
		cpuinfo_log_warning("cpuinfo_mock_close called without mock filesystem; redictering to close");
		return close(fd);
	}

	if ((unsigned int)fd >= cpuinfo_mock_file_count) {
		errno = EBADF;
		return -1;
	}
	if (cpuinfo_mock_files[fd].offset == SIZE_MAX) {
		errno = EBADF;
		return -1;
	}
	cpuinfo_mock_files[fd].offset = SIZE_MAX;
	return 0;
}

ssize_t CPUINFO_ABI cpuinfo_mock_read(int fd, void* buffer, size_t capacity) {
	if (cpuinfo_mock_files == NULL) {
		cpuinfo_log_warning("cpuinfo_mock_read called without mock filesystem; redictering to read");
		return read(fd, buffer, capacity);
	}

	if ((unsigned int)fd >= cpuinfo_mock_file_count) {
		errno = EBADF;
		return -1;
	}
	if (cpuinfo_mock_files[fd].offset == SIZE_MAX) {
		errno = EBADF;
		return -1;
	}

	const size_t offset = cpuinfo_mock_files[fd].offset;
	size_t count = cpuinfo_mock_files[fd].size - offset;
	if (count > capacity) {
		count = capacity;
	}
	memcpy(buffer, (void*)cpuinfo_mock_files[fd].content + offset, count);
	cpuinfo_mock_files[fd].offset += count;
	return (ssize_t)count;
}
