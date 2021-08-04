add_library(pthreads4w
	pthread.c
	config.h
	context.h
	include/pthread.h
	include/sched.h
	include/semaphore.h
	need_errno.h
)
target_compile_definitions(pthreads4w PUBLIC PTW32_STATIC_LIB __CLEANUP_SEH PRIVATE HAVE_PTW32_CONFIG_H PTW32_BUILD_INLINED)
target_include_directories(pthreads4w PUBLIC include)
