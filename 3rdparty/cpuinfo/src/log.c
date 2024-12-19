#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#if defined(__ANDROID__)
#include <android/log.h>
#endif
#if defined(__hexagon__)
#include <qurt_printf.h>
#endif

#ifndef CPUINFO_LOG_TO_STDIO
#if defined(__ANDROID__)
#define CPUINFO_LOG_TO_STDIO 0
#else
#define CPUINFO_LOG_TO_STDIO 1
#endif
#endif

#include <cpuinfo/log.h>

/* Messages up to this size are formatted entirely on-stack, and don't allocate
 * heap memory */
#define CPUINFO_LOG_STACK_BUFFER_SIZE 1024

#ifdef _WIN32
#define CPUINFO_LOG_NEWLINE_LENGTH 2

#define CPUINFO_LOG_STDERR STD_ERROR_HANDLE
#define CPUINFO_LOG_STDOUT STD_OUTPUT_HANDLE
#elif defined(__hexagon__)
#define CPUINFO_LOG_NEWLINE_LENGTH 1

#define CPUINFO_LOG_STDERR 0
#define CPUINFO_LOG_STDOUT 0
#else
#define CPUINFO_LOG_NEWLINE_LENGTH 1

#define CPUINFO_LOG_STDERR STDERR_FILENO
#define CPUINFO_LOG_STDOUT STDOUT_FILENO
#endif

#if CPUINFO_LOG_TO_STDIO
static void cpuinfo_vlog(
	int output_handle,
	const char* prefix,
	size_t prefix_length,
	const char* format,
	va_list args) {
	char stack_buffer[CPUINFO_LOG_STACK_BUFFER_SIZE];
	char* heap_buffer = NULL;
	char* out_buffer = &stack_buffer[0];

	/* The first call to vsnprintf will clobber args, thus need a copy in
	 * case a second vsnprintf call is needed */
	va_list args_copy;
	va_copy(args_copy, args);

	memcpy(stack_buffer, prefix, prefix_length * sizeof(char));
	assert((prefix_length + CPUINFO_LOG_NEWLINE_LENGTH) * sizeof(char) <= CPUINFO_LOG_STACK_BUFFER_SIZE);

	const int format_chars = vsnprintf(
		&stack_buffer[prefix_length],
		CPUINFO_LOG_STACK_BUFFER_SIZE - (prefix_length + CPUINFO_LOG_NEWLINE_LENGTH) * sizeof(char),
		format,
		args);
	if (format_chars < 0) {
		/* Format error in the message: silently ignore this particular
		 * message. */
		goto cleanup;
	}
	const size_t format_length = (size_t)format_chars;
	if ((prefix_length + format_length + CPUINFO_LOG_NEWLINE_LENGTH) * sizeof(char) >
	    CPUINFO_LOG_STACK_BUFFER_SIZE) {
		/* Allocate a buffer on heap, and vsnprintf to this buffer */
		const size_t heap_buffer_size =
			(prefix_length + format_length + CPUINFO_LOG_NEWLINE_LENGTH) * sizeof(char);
#if _WIN32
		heap_buffer = HeapAlloc(GetProcessHeap(), 0, heap_buffer_size);
#else
		heap_buffer = malloc(heap_buffer_size);
#endif
		if (heap_buffer == NULL) {
			goto cleanup;
		}

		/* Copy pre-formatted prefix into the on-heap buffer */
		memcpy(heap_buffer, prefix, prefix_length * sizeof(char));
		vsnprintf(
			&heap_buffer[prefix_length],
			(format_length + CPUINFO_LOG_NEWLINE_LENGTH) * sizeof(char),
			format,
			args_copy);
		out_buffer = heap_buffer;
	}
#ifdef _WIN32
	out_buffer[prefix_length + format_length] = '\r';
	out_buffer[prefix_length + format_length + 1] = '\n';

	DWORD bytes_written;
	WriteFile(
		GetStdHandle((DWORD)output_handle),
		out_buffer,
		(prefix_length + format_length + CPUINFO_LOG_NEWLINE_LENGTH) * sizeof(char),
		&bytes_written,
		NULL);
#elif defined(__hexagon__)
	qurt_printf("%s", out_buffer);
#else
	out_buffer[prefix_length + format_length] = '\n';

	ssize_t bytes_written = write(
		output_handle, out_buffer, (prefix_length + format_length + CPUINFO_LOG_NEWLINE_LENGTH) * sizeof(char));
	(void)bytes_written;
#endif

cleanup:
#ifdef _WIN32
	HeapFree(GetProcessHeap(), 0, heap_buffer);
#else
	free(heap_buffer);
#endif
	va_end(args_copy);
}
#elif defined(__ANDROID__) && CPUINFO_LOG_LEVEL > CPUINFO_LOG_NONE
static const char cpuinfo_module[] = "XNNPACK";
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_DEBUG
void cpuinfo_vlog_debug(const char* format, va_list args) {
#if CPUINFO_LOG_TO_STDIO
	static const char debug_prefix[17] = {
		'D', 'e', 'b', 'u', 'g', ' ', '(', 'c', 'p', 'u', 'i', 'n', 'f', 'o', ')', ':', ' '};
	cpuinfo_vlog(CPUINFO_LOG_STDOUT, debug_prefix, 17, format, args);
#elif defined(__ANDROID__)
	__android_log_vprint(ANDROID_LOG_DEBUG, cpuinfo_module, format, args);
#else
#error "Platform-specific implementation required"
#endif
}
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_INFO
void cpuinfo_vlog_info(const char* format, va_list args) {
#if CPUINFO_LOG_TO_STDIO
	static const char info_prefix[16] = {
		'N', 'o', 't', 'e', ' ', '(', 'c', 'p', 'u', 'i', 'n', 'f', 'o', ')', ':', ' '};
	cpuinfo_vlog(CPUINFO_LOG_STDOUT, info_prefix, 16, format, args);
#elif defined(__ANDROID__)
	__android_log_vprint(ANDROID_LOG_INFO, cpuinfo_module, format, args);
#else
#error "Platform-specific implementation required"
#endif
}
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_WARNING
void cpuinfo_vlog_warning(const char* format, va_list args) {
#if CPUINFO_LOG_TO_STDIO
	static const char warning_prefix[20] = {'W', 'a', 'r', 'n', 'i', 'n', 'g', ' ', 'i', 'n',
						' ', 'c', 'p', 'u', 'i', 'n', 'f', 'o', ':', ' '};
	cpuinfo_vlog(CPUINFO_LOG_STDERR, warning_prefix, 20, format, args);
#elif defined(__ANDROID__)
	__android_log_vprint(ANDROID_LOG_WARN, cpuinfo_module, format, args);
#else
#error "Platform-specific implementation required"
#endif
}
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_ERROR
void cpuinfo_vlog_error(const char* format, va_list args) {
#if CPUINFO_LOG_TO_STDIO
	static const char error_prefix[18] = {
		'E', 'r', 'r', 'o', 'r', ' ', 'i', 'n', ' ', 'c', 'p', 'u', 'i', 'n', 'f', 'o', ':', ' '};
	cpuinfo_vlog(CPUINFO_LOG_STDERR, error_prefix, 18, format, args);
#elif defined(__ANDROID__)
	__android_log_vprint(ANDROID_LOG_ERROR, cpuinfo_module, format, args);
#else
#error "Platform-specific implementation required"
#endif
}
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_FATAL
void cpuinfo_vlog_fatal(const char* format, va_list args) {
#if CPUINFO_LOG_TO_STDIO
	static const char fatal_prefix[24] = {'F', 'a', 't', 'a', 'l', ' ', 'e', 'r', 'r', 'o', 'r', ' ',
					      'i', 'n', ' ', 'c', 'p', 'u', 'i', 'n', 'f', 'o', ':', ' '};
	cpuinfo_vlog(CPUINFO_LOG_STDERR, fatal_prefix, 24, format, args);
#elif defined(__ANDROID__)
	__android_log_vprint(ANDROID_LOG_FATAL, cpuinfo_module, format, args);
#else
#error "Platform-specific implementation required"
#endif
}
#endif
