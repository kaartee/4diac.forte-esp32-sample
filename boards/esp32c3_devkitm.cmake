if(MKLITTLEFS_EXE AND EXISTS "${LFS_ROOT_DIR}" AND LFS_PART_SIZE)
    set_property(GLOBAL APPEND PROPERTY EXTRA_POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E echo "LittleFS image ready at ${LFS_BIN_FILE} (offset ${LFS_PART_ADDR})"
    )
    board_finalize_runner_args(esp32 "--esp-flash-address=${LFS_PART_ADDR}" "--esp-flash-file=${LFS_BIN_FILE}")
endif()
