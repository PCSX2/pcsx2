add_library(pcsx2-rapidyaml
  include/c4/base64.hpp
  include/c4/blob.hpp
  include/c4/charconv.hpp
  include/c4/compiler.hpp
  include/c4/config.hpp
  include/c4/cpu.hpp
  include/c4/dump.hpp
  include/c4/error.hpp
  include/c4/export.hpp
  include/c4/format.hpp
  include/c4/language.hpp
  include/c4/memory_util.hpp
  include/c4/platform.hpp
  include/c4/preprocessor.hpp
  include/c4/std/std.hpp
  include/c4/std/std_fwd.hpp
  include/c4/std/string.hpp
  include/c4/std/string_fwd.hpp
  include/c4/std/string_view.hpp
  include/c4/std/tuple.hpp
  include/c4/std/vector.hpp
  include/c4/std/vector_fwd.hpp
  include/c4/substr.hpp
  include/c4/substr_fwd.hpp
  include/c4/szconv.hpp
  include/c4/types.hpp
  include/c4/utf.hpp
  include/c4/windows.hpp
  include/c4/windows_pop.hpp
  include/c4/windows_push.hpp
  include/c4/yml/common.hpp
  include/c4/yml/detail/parser_dbg.hpp
  include/c4/yml/detail/stack.hpp
  include/c4/yml/emit.def.hpp
  include/c4/yml/emit.hpp
  include/c4/yml/export.hpp
  include/c4/yml/node.hpp
  include/c4/yml/parse.hpp
  include/c4/yml/preprocess.hpp
  include/c4/yml/std/map.hpp
  include/c4/yml/std/std.hpp
  include/c4/yml/std/string.hpp
  include/c4/yml/std/vector.hpp
  include/c4/yml/tree.hpp
  include/c4/yml/writer.hpp
  include/c4/yml/yml.hpp
  include/ryml.hpp
  include/ryml_std.hpp
  src/c4/base64.cpp
  src/c4/error.cpp
  src/c4/format.cpp
  src/c4/language.cpp
  src/c4/memory_util.cpp
  src/c4/utf.cpp
  src/c4/yml/common.cpp
  src/c4/yml/node.cpp
  src/c4/yml/parse.cpp
  src/c4/yml/preprocess.cpp
  src/c4/yml/tree.cpp
)

target_include_directories(pcsx2-rapidyaml PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/../fast_float/include"
)
target_include_directories(pcsx2-rapidyaml INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(pcsx2-rapidyaml PUBLIC
  "C4_NO_DEBUG_BREAK"
)

add_library(rapidyaml::rapidyaml ALIAS pcsx2-rapidyaml)
