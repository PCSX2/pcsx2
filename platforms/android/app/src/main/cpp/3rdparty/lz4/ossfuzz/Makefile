# ##########################################################################
# LZ4 oss fuzzer - Makefile
#
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - LZ4 homepage : http://www.lz4.org
#  - LZ4 source repository : https://github.com/lz4/lz4
# ##########################################################################
# compress_fuzzer : OSS Fuzz test tool
# decompress_fuzzer : OSS Fuzz test tool
# ##########################################################################

LZ4DIR  := ../lib
LIB_FUZZING_ENGINE ?=

DEBUGLEVEL?= 1
DEBUGFLAGS = -g -DLZ4_DEBUG=$(DEBUGLEVEL)

LZ4_CFLAGS  = $(CFLAGS) $(DEBUGFLAGS) $(MOREFLAGS)
LZ4_CXXFLAGS = $(CXXFLAGS) $(DEBUGFLAGS) $(MOREFLAGS)
LZ4_CPPFLAGS = $(CPPFLAGS) -I$(LZ4DIR) -DXXH_NAMESPACE=LZ4_ \
               -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

FUZZERS := \
	compress_fuzzer \
	decompress_fuzzer \
	round_trip_fuzzer \
	round_trip_stream_fuzzer \
	compress_hc_fuzzer \
	round_trip_hc_fuzzer \
	compress_frame_fuzzer \
	round_trip_frame_fuzzer \
	round_trip_frame_uncompressed_fuzzer \
	decompress_frame_fuzzer

.PHONY: all
all: $(FUZZERS)

# Include a rule to build the static library if calling this target
# directly.
$(LZ4DIR)/liblz4.a:
	$(MAKE) -C $(LZ4DIR) CFLAGS="$(LZ4_CFLAGS)" liblz4.a

%.o: %.c
	$(CC) -c $(LZ4_CFLAGS) $(LZ4_CPPFLAGS) $< -o $@

# Generic rule for generating fuzzers
ifeq ($(LIB_FUZZING_ENGINE),)
  LIB_FUZZING_DEPS := standaloneengine.o
else
  LIB_FUZZING_DEPS :=
endif
%_fuzzer: %_fuzzer.o lz4_helpers.o fuzz_data_producer.o $(LZ4DIR)/liblz4.a $(LIB_FUZZING_DEPS)
	$(CXX) $(LZ4_CXXFLAGS) $(LZ4_CPPFLAGS) $(LDFLAGS) $(LIB_FUZZING_ENGINE) $^ -o $@$(EXT)

%_fuzzer_clean:
	$(RM) $*_fuzzer $*_fuzzer.o standaloneengine.o

.PHONY: clean
clean: compress_fuzzer_clean decompress_fuzzer_clean \
	compress_frame_fuzzer_clean compress_hc_fuzzer_clean \
	decompress_frame_fuzzer_clean round_trip_frame_fuzzer_clean \
	round_trip_fuzzer_clean round_trip_hc_fuzzer_clean round_trip_stream_fuzzer_clean
	$(MAKE) -C $(LZ4DIR) clean
