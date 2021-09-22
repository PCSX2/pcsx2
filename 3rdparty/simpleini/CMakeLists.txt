set(SRCS
    include/ConvertUTF.h
    include/SimpleIni.h
    src/ConvertUTF.c
)

add_library(simpleini ${SRCS})
target_include_directories(simpleini PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(simpleini INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
