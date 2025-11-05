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

add_pcsx2_test(debugtools_memory_scanner_tests
        memory_scanner_tests.cpp
        ../../../pcsx2/DebugTools/MemoryScanner.cpp
        ../../../pcsx2/DebugTools/DebugInterface.cpp
        ../../../pcsx2/DebugTools/Breakpoints.cpp
        ../../../pcsx2/DebugTools/SymbolGuardian.cpp
        ../../../pcsx2/DebugTools/SymbolImporter.cpp
        ../../../pcsx2/DebugTools/ExpressionParser.cpp
        ../../../pcsx2/DebugTools/MIPSAnalyst.cpp
)

target_include_directories(debugtools_memory_scanner_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/pcsx2
        ${CMAKE_SOURCE_DIR}/common
)

add_pcsx2_test(debugtools_mcp_tools_tests
        mcp_tools_tests.cpp
        ../../../pcsx2/DebugTools/MCPServer.cpp
)

target_include_directories(debugtools_mcp_tools_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/pcsx2
        ${CMAKE_SOURCE_DIR}/3rdparty/rapidjson/include
)
