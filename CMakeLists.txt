# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# 4diac FORTE as Zephyr library module

zephyr_get_compile_definitions_for_lang_as_string(CXX cxx_definitions)
zephyr_get_compile_options_for_lang_as_string(CXX cxx_options)
zephyr_get_include_directories_for_lang_as_string(CXX cxx_includes)
zephyr_get_system_include_directories_for_lang_as_string(CXX cxx_system_includes)

set(external_project_cxxflags
	"${cxx_definitions} ${cxx_options} ${cxx_includes} ${cxx_system_includes} -Wno-error"
)

include(ExternalProject)

set(forte_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/lib/org.eclipse.4diac.forte)
set(FORTE_LIB_DIR ${forte_src_dir}/bin/zephyr/src)

ExternalProject_Add(forte
	SOURCE_DIR ${forte_src_dir}
	BINARY_DIR ${forte_src_dir}/bin/zephyr
	CONFIGURE_COMMAND
		cmake -G "Ninja"
			-DCMAKE_C_COMPILER_WORKS=ON
			-DCMAKE_CXX_COMPILER_WORKS=ON
			-DNULLPTR_SUPPORTED=ON
			-DCMAKE_SYSTEM_NAME=Generic
			-DFORTE_ARCHITECTURE=Zephyr
			-DFORTE_TESTS=OFF
			-DFORTE_SUPPORT_BOOT_FILE=ON
			-DFORTE_BUILD_EXECUTABLE=OFF
			-DFORTE_BUILD_STATIC_LIBRARY=ON
			-DFORTE_MODULE_CONVERT=ON
			-DFORTE_MODULE_IEC61131=ON
			-DFORTE_IO=ON
			-DFORTE_COM_ETH=ON
			-DFORTE_COM_FBDK=ON
			-DFORTE_COM_LOCAL=ON
			-DFORTE_MODULE_UTILS=ON
			-DFORTE_EXTERNAL_MODULES_DIRECTORY=${forte_src_dir}/ExportedFBs
			-DFORTE_MODULE_EXTERNAL_PIDS=ON
			-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
			-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
			-DFORTE_LOGLEVEL=NOLOG
			-DCMAKE_BUILD_TYPE=MINSIZEREL
			-DCMAKE_C_FLAGS_DEBUG="-g"
			-DCMAKE_CXX_FLAGS=${external_project_cxxflags}
			-DFORTE_SOURCE_DIR=${forte_src_dir}
			../../
	BUILD_COMMAND
		ninja
	BUILD_BYPRODUCTS ${FORTE_LIB_DIR}/libforte-static.a
	INSTALL_COMMAND ""
)

add_dependencies(forte zephyr_interface)

add_library(forte_lib STATIC IMPORTED GLOBAL)
add_dependencies(forte_lib forte)
set_target_properties(forte_lib PROPERTIES IMPORTED_LOCATION ${FORTE_LIB_DIR}/libforte-static.a)
target_link_libraries(app PUBLIC forte_lib)

