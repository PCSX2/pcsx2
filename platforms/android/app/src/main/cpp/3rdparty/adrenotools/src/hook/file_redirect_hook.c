#include "hook_impl.h"

__attribute__((visibility("default"))) FILE *fopen(const char *filename, const char *mode) {
	return hook_fopen(filename, mode);
}