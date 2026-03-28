set(SRCS
  include/gamemode_client.h
)

if(LINUX)
	add_library(gamemode INTERFACE ${SRCS})
	target_include_directories(gamemode INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
	target_link_libraries(gamemode INTERFACE ${CMAKE_DL_LIBS})
endif()
