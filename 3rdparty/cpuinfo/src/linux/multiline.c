#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#if CPUINFO_MOCK
	#include <cpuinfo-mock.h>
#endif
#include <linux/api.h>
#include <cpuinfo/log.h>


bool cpuinfo_linux_parse_multiline_file(const char* filename, size_t buffer_size, cpuinfo_line_callback callback, void* context)
{
	int file = -1;
	bool status = false;
	char* buffer = (char*) alloca(buffer_size);

#if CPUINFO_MOCK
	file = cpuinfo_mock_open(filename, O_RDONLY);
#else
	file = open(filename, O_RDONLY);
#endif
	if (file == -1) {
		cpuinfo_log_info("failed to open %s: %s", filename, strerror(errno));
		goto cleanup;
	}

	/* Only used for error reporting */
	size_t position = 0;
	uint64_t line_number = 1;
	const char* buffer_end = &buffer[buffer_size];
	char* data_start = buffer;
	ssize_t bytes_read;
	do {
#if CPUINFO_MOCK
		bytes_read = cpuinfo_mock_read(file, data_start, (size_t) (buffer_end - data_start));
#else
		bytes_read = read(file, data_start, (size_t) (buffer_end - data_start));
#endif
		if (bytes_read < 0) {
			cpuinfo_log_info("failed to read file %s at position %zu: %s",
				filename, position, strerror(errno));
			goto cleanup;
		}

		position += (size_t) bytes_read;
		const char* data_end = data_start + (size_t) bytes_read;
		const char* line_start = buffer;

		if (bytes_read == 0) {
			/* No more data in the file: process the remaining text in the buffer as a single entry */
			const char* line_end = data_end;
			if (!callback(line_start, line_end, context, line_number)) {
				goto cleanup;
			}
		} else {
			const char* line_end;
			do {
				/* Find the end of the entry, as indicated by newline character ('\n') */
				for (line_end = line_start; line_end != data_end; line_end++) {
					if (*line_end == '\n') {
						break;
					}
				}

				/*
				 * If we located separator at the end of the entry, parse it.
				 * Otherwise, there may be more data at the end; read the file once again.
				 */
				if (line_end != data_end) {
					if (!callback(line_start, line_end, context, line_number++)) {
						goto cleanup;
					}
					line_start = line_end + 1;
				}
			} while (line_end != data_end);

			/* Move remaining partial line data at the end to the beginning of the buffer */
			const size_t line_length = (size_t) (line_end - line_start);
			memmove(buffer, line_start, line_length);
			data_start = &buffer[line_length];
		}
	} while (bytes_read != 0);

	/* Commit */
	status = true;

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
