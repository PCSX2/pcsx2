add_library(des STATIC
	src/des.c
)

target_include_directories(des PUBLIC src)