add_pcsx2_test(debugtools_mcp_server_tests
        mcp_server_tests.cpp
        ../../../pcsx2/DebugTools/MCPServer.cpp
)

target_include_directories(debugtools_mcp_server_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/pcsx2
        ${CMAKE_SOURCE_DIR}/3rdparty/rapidjson/include
)

add_pcsx2_test(debugtools_instruction_tracer_tests
        instruction_tracer_tests.cpp
        ../../../pcsx2/DebugTools/InstructionTracer.cpp
        ../../../pcsx2/DebugTools/DebugInterface.cpp
)

target_include_directories(debugtools_instruction_tracer_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/pcsx2
        ${CMAKE_SOURCE_DIR}/common
)
