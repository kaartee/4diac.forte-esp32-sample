# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 4.5)

# Standard Zephyr package initialization
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(forte_zephyr_app LANGUAGES C CXX)

target_sources(app PRIVATE src/main.cpp)

include(ExternalProject)

set(FORTE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/org.eclipse.4diac.forte")
set(FORTE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/forte-build")
set(FORTE_MAIN_LIB "${FORTE_BINARY_DIR}/libforte.a")
set(FORTE_LD_FILE "${CMAKE_CURRENT_BINARY_DIR}/forte_libs.ld")
set(FORTE_COLLECT_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/collect_forte_libs.cmake")

# Auto-detect ccache if available on PATH
find_program(CCACHE_PROGRAM ccache)
set(FORTE_CCACHE_CACHE_ENTRIES "")
if(CCACHE_PROGRAM)
    message(STATUS "Auto-detected ccache for forte_external: ${CCACHE_PROGRAM}")
    set(FORTE_CCACHE_CACHE_ENTRIES "
set(CMAKE_C_COMPILER_LAUNCHER [==[${CCACHE_PROGRAM}]==] CACHE FILEPATH \"\" FORCE)
set(CMAKE_CXX_COMPILER_LAUNCHER [==[${CCACHE_PROGRAM}]==] CACHE FILEPATH \"\" FORCE)
")
endif()

# 1. Extract Zephyr's flags (this strips single-arg $<TARGET_PROPERTY:prop> expressions)
zephyr_get_include_directories_for_lang_as_string(C ZEPHYR_C_INCLUDES)
zephyr_get_include_directories_for_lang_as_string(CXX ZEPHYR_CXX_INCLUDES)

zephyr_get_compile_definitions_for_lang_as_string(C ZEPHYR_C_DEFINES)
zephyr_get_compile_definitions_for_lang_as_string(CXX ZEPHYR_CXX_DEFINES)

zephyr_get_compile_options_for_lang_as_string(C ZEPHYR_C_OPTIONS)
zephyr_get_compile_options_for_lang_as_string(CXX ZEPHYR_CXX_OPTIONS)

set(RAW_C_FLAGS "${CMAKE_C_FLAGS} ${ZEPHYR_C_OPTIONS} ${ZEPHYR_C_DEFINES} ${ZEPHYR_C_INCLUDES}")
set(RAW_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ZEPHYR_CXX_OPTIONS} ${ZEPHYR_CXX_DEFINES} ${ZEPHYR_CXX_INCLUDES}")

# 2. Use file(GENERATE) to evaluate the remaining $<JOIN:...> and $<TARGET_PROPERTY:compiler,...> expressions
set(FORTE_CACHE_FILE "${CMAKE_CURRENT_BINARY_DIR}/forte_initial_cache.cmake")

file(GENERATE OUTPUT "${FORTE_CACHE_FILE}" CONTENT "
set(CMAKE_SYSTEM_NAME Generic CACHE STRING \"\" FORCE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE STRING \"\" FORCE)
set(CMAKE_C_COMPILER [==[${CMAKE_C_COMPILER}]==] CACHE FILEPATH \"\" FORCE)
set(CMAKE_CXX_COMPILER [==[${CMAKE_CXX_COMPILER}]==] CACHE FILEPATH \"\" FORCE)
${FORTE_CCACHE_CACHE_ENTRIES}
set(CMAKE_AR [==[${CMAKE_AR}]==] CACHE FILEPATH \"\" FORCE)
set(CMAKE_RANLIB [==[${CMAKE_RANLIB}]==] CACHE FILEPATH \"\" FORCE)
set(CMAKE_C_FLAGS [==[${RAW_C_FLAGS}]==] CACHE STRING \"\" FORCE)
set(CMAKE_CXX_FLAGS [==[${RAW_CXX_FLAGS}]==] CACHE STRING \"\" FORCE)

# Required by 4diac FORTE for static library linking
set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE \"LINKER:--whole-archive\" \"<LINK_ITEM>\" \"LINKER:--no-whole-archive\" CACHE STRING \"\" FORCE)
set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE_SUPPORTED TRUE CACHE BOOL \"\" FORCE)
set(CMAKE_LINK_LIBRARY_WHOLE_ARCHIVE_ATTRIBUTES LIBRARY_TYPE=STATIC DEDUPLICATION=YES OVERRIDE=DEFAULT CACHE STRING \"\" FORCE)
")

# 3. Generate a build-time helper script that globs all compiled *.a files
#    and writes a GNU linker script fragment (forte_libs.ld)
file(WRITE "${FORTE_COLLECT_SCRIPT}" "
file(GLOB_RECURSE ALL_FORTE_ARCHIVES \"${FORTE_BINARY_DIR}/*.a\")
list(FILTER ALL_FORTE_ARCHIVES EXCLUDE REGEX \"CMakeFiles/\")
set(LD_CONTENT \"INPUT(\\n\")
foreach(archive IN LISTS ALL_FORTE_ARCHIVES)
    string(APPEND LD_CONTENT \"  \\\"\${archive}\\\"\\n\")
endforeach()
string(APPEND LD_CONTENT \")\\n\")
file(WRITE \"${FORTE_LD_FILE}\" \"\${LD_CONTENT}\")
")

# 4. Build FORTE and collect all generated .a archives right after compilation
ExternalProject_Add(forte_external
    DEPENDS zephyr_generated_headers
    SOURCE_DIR "${FORTE_SOURCE_DIR}"
    BINARY_DIR "${FORTE_BINARY_DIR}"
    CMAKE_ARGS
        -G "Ninja"
        -C "${FORTE_CACHE_FILE}"
        # FORTE Zephyr Architecture & Library Settings
        -DFORTE_ARCHITECTURE=Zephyr
        -DFORTE_BUILD_EXECUTABLE=OFF
        -DFORTE_TESTS=OFF
        -DFORTE_LOGLEVEL=LOGWARNING
        -DFORTE_SUPPORT_BOOT_FILE=ON
        -DFORTE_BOOT_FILE_LOCATION=/lfs1/forte.fboot
        -DFORTE_COM_ETH=ON
        # Function Block Modules
        -DFORTE_MODULE_CONVERT=OFF
        -DFORTE_MODULE_IEC61131=ON
        -DFORTE_MODULE_ESP32ETHERNETKIT=ON
        -DFORTE_IO=ON
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
          COMMAND ${CMAKE_COMMAND} -P "${FORTE_COLLECT_SCRIPT}"
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${FORTE_MAIN_LIB}" "${FORTE_LD_FILE}"
)

# Ensure forte_external finishes building and generating forte_libs.ld before 'app' links
add_dependencies(app forte_external)

# Include FORTE source and generated headers for main.cpp
target_include_directories(app PRIVATE
    "${FORTE_SOURCE_DIR}/core/include"
    "${FORTE_SOURCE_DIR}/core/arch/common/include"
    "${FORTE_SOURCE_DIR}/core/arch/zephyr/include"
    "${FORTE_BINARY_DIR}/core/include"
    "${FORTE_BINARY_DIR}/include"
    "${FORTE_BINARY_DIR}/core/src_gen"
)

# Pass the generated linker script fragment inside an atomic --whole-archive group
target_link_libraries(app PRIVATE
    "-Wl,--whole-archive,${FORTE_LD_FILE},--no-whole-archive"
)

# 5. Pack lfs_root/ into a LittleFS image matching storage_partition in devicetree
find_program(MKLITTLEFS_EXE mklittlefs)
set(LFS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lfs_root")
set(LFS_BIN_FILE "${CMAKE_CURRENT_BINARY_DIR}/littlefs.bin")

dt_nodelabel(STORAGE_PART_NODE NODELABEL "storage_partition")
if(STORAGE_PART_NODE)
    dt_reg_addr(STORAGE_PART_ADDR PATH "${STORAGE_PART_NODE}")
    dt_reg_size(STORAGE_PART_SIZE PATH "${STORAGE_PART_NODE}")
endif()

if(MKLITTLEFS_EXE AND EXISTS "${LFS_ROOT_DIR}" AND STORAGE_PART_SIZE)
    file(GLOB_RECURSE LFS_ROOT_FILES CONFIGURE_DEPENDS "${LFS_ROOT_DIR}/*")
    add_custom_command(
        OUTPUT "${LFS_BIN_FILE}"
        COMMAND "${MKLITTLEFS_EXE}" -c "${LFS_ROOT_DIR}" -b 4096 -p 16 -s ${STORAGE_PART_SIZE} "${LFS_BIN_FILE}"
        DEPENDS ${LFS_ROOT_FILES}
        COMMENT "Packing ${LFS_ROOT_DIR} into LittleFS image (${LFS_BIN_FILE}) for offset ${STORAGE_PART_ADDR}"
    )
    add_custom_target(littlefs_image ALL DEPENDS "${LFS_BIN_FILE}")
elseif(EXISTS "${LFS_ROOT_DIR}" AND NOT MKLITTLEFS_EXE)
    message(WARNING "mklittlefs not found on PATH; skipping ${LFS_BIN_FILE} generation")
endif()
