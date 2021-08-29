add_pcsx2_test(
pstring_test
${CMAKE_SOURCE_DIR}/common/include/Utilities/PString.h 
${CMAKE_SOURCE_DIR}/common/src/Utilities/PString.cpp
${CMAKE_SOURCE_DIR}/3rdparty/include/ghc/filesystem.h
test.cpp)
target_include_directories(pstring_test PRIVATE ${CMAKE_SOURCE_DIR}/common/include/Utilities ${CMAKE_SOURCE_DIR}/3rdparty/include/)