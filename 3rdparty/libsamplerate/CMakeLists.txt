add_library(samplerate
	samplerate.c
	src_linear.c
	src_sinc.c
	src_zoh.c
	common.h
	config.h
	fastest_coeffs.h
	high_qual_coeffs.h
	mid_qual_coeffs.h
	samplerate.h
)
target_include_directories(samplerate PUBLIC .)
add_library(PkgConfig::SAMPLERATE ALIAS samplerate)
