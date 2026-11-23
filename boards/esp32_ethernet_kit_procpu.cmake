# SPDX-License-Identifier: Apache-2.0

dt_nodelabel(STORAGE_PART_NODE NODELABEL "storage_partition")
if(STORAGE_PART_NODE AND MKLITTLEFS_EXE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lfs_root")
    dt_reg_addr(STORAGE_PART_ADDR PATH "${STORAGE_PART_NODE}")
    board_finalize_runner_args(esp32
        "--esp-flash-address=${STORAGE_PART_ADDR}"
        "--esp-flash-file=${CMAKE_CURRENT_BINARY_DIR}/littlefs.bin"
    )
endif()
