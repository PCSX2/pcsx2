set(SRCS
	include/discord_register.h
	include/discord_rpc.h
	src/backoff.h
	src/connection.h
	src/discord_rpc.cpp
	src/msg_queue.h
	src/rpc_connection.cpp
	src/rpc_connection.h
	src/serialization.cpp
	src/serialization.h
)

add_library(discord-rpc ${SRCS})
target_include_directories(discord-rpc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(discord-rpc rapidjson)

# Needed for macOS compile.
set_property(TARGET discord-rpc PROPERTY CXX_STANDARD 17)
set_property(TARGET discord-rpc PROPERTY CXX_STANDARD_REQUIRED ON)

if(WIN32)
	target_sources(discord-rpc PRIVATE
		src/connection_win.cpp
		src/discord_register_win.cpp
	)
elseif(APPLE)
	target_sources(discord-rpc PRIVATE
		src/connection_unix.cpp
		src/discord_register_osx.m
	)
elseif(UNIX)
	target_sources(discord-rpc PRIVATE
		src/connection_unix.cpp
		src/discord_register_linux.cpp
	)
endif()
