#include "hook_impl.h"

__attribute__((visibility("default"))) void *android_dlopen_ext(const char *filename, int flags, const android_dlextinfo *extinfo) {
	return hook_android_dlopen_ext(filename, flags, extinfo);
}

__attribute__((visibility("default"))) void *android_load_sphal_library(const char *filename, int flags) {
	return hook_android_load_sphal_library(filename, flags);
}