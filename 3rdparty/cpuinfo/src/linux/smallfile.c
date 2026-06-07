#include <alloca.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if CPUINFO_MOCK
#include <cpuinfo-mock.h>
#endif
#include <cpuinfo/log.h>
#include <linux/api.h>

bool cpuinfo_linux_parse_small_file(
	const char* filename,
	size_t buffer_size,
	cpuinfo_smallfile_callback callback,
	void* context) {
	int file = -1;
	bool status = false;
	char* buffer = (char*)alloca(buffer_size);

#if CPUINFO_LOG_DEBUG_PARSERS
	cpuinfo_log_debug("parsing small file %s", filename);
#endif

#if CPUINFO_MOCK
	file = cpuinfo_mock_open(filename, O_RDONLY);
#else
	file = open(filename, O_RDONLY);
#endif
	if (file == -1) {
		cpuinfo_log_info("failed to open %s: %s", filename, strerror(errno));
		goto cleanup;
	}

	size_t buffer_position = 0;
	ssize_t bytes_read;
	do {
#if CPUINFO_MOCK
		bytes_read = cpuinfo_mock_read(file, &buffer[buffer_position], buffer_size - buffer_position);
#else
		bytes_read = read(file, &buffer[buffer_position], buffer_size - buffer_position);
#endif
		if (bytes_read < 0) {
			cpuinfo_log_info(
				"failed to read file %s at position %zu: %s",
				filename,
				buffer_position,
				strerror(errno));
			goto cleanup;
		}
		buffer_position += (size_t)bytes_read;
		if (buffer_position >= buffer_size) {
			cpuinfo_log_error(
				"failed to read file %s: insufficient buffer of size %zu", filename, buffer_size);
			goto cleanup;
		}
	} while (bytes_read != 0);

	status = callback(filename, buffer, &buffer[buffer_position], context);

cleanup:
	if (file != -1) {
#if CPUINFO_MOCK
		cpuinfo_mock_close(file);
#else
		close(file);
#endif
		file = -1;
	}
	return status;
}
