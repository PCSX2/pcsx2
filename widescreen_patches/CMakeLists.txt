set(patchlist "${CMAKE_CURRENT_BINARY_DIR}/cheatwsgenlist.txt")
set(output "${CMAKE_CURRENT_BINARY_DIR}/cheats_ws.zip")

add_custom_target(widescreen_filelist ALL
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/genfilelist.sh ${patchlist}
    BYPRODUCTS ${patchlist}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/cheats_ws
    DEPENDS genfilelist.sh
)

add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${output}" --format=zip --files-from=${patchlist}
    DEPENDS ${patchlist}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/cheats_ws
)

add_custom_target(widescreen_zip ALL DEPENDS "${output}")

if (PACKAGE_MODE)
    install(FILES "${output}" DESTINATION "${GAMEINDEX_DIR}")
else()
    install(FILES "${output}" DESTINATION "${CMAKE_SOURCE_DIR}/bin")
endif()
