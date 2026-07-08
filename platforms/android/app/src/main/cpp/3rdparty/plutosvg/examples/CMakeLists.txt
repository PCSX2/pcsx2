set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

FILE(COPY camera.svg DESTINATION "${CMAKE_BINARY_DIR}/examples")

add_executable(camera2png camera2png.c)
target_link_libraries(camera2png plutosvg)

add_executable(svg2png svg2png.c)
target_link_libraries(svg2png plutosvg)

if(PLUTOSVG_ENABLE_FREETYPE)
    add_executable(emoji2png emoji2png.c)
    target_link_libraries(emoji2png plutosvg)
endif()
