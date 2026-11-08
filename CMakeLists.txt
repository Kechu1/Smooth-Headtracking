# --- Proyecto ---

cmake_minimum_required(VERSION 4.3.0)

# version del mod (4 numeros, el ultimo es obligatorio para el .rc)
project(
	"SmoothHeadtracking"
	VERSION 2.4.0.0
	DESCRIPTION "Head and spine tracking that follows third-person aim"
	LANGUAGES CXX
)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

# --- CommonLibF4 AV (LucaDotGit) ---
# OG + NG + AE en un solo .dll, igual que antes

set(COMMONLIB_RUNTIME_VR OFF CACHE BOOL "" FORCE)
set(COMMONLIB_RUNTIME_OG ON CACHE BOOL "" FORCE)
set(COMMONLIB_RUNTIME_NG ON CACHE BOOL "" FORCE)
set(COMMONLIB_RUNTIME_AE ON CACHE BOOL "" FORCE)

set(COMMONLIB_OPTION_FMT ON CACHE BOOL "" FORCE)
set(COMMONLIB_OPTION_XBYAK OFF CACHE BOOL "" FORCE)
set(COMMONLIB_OPTION_INI ON CACHE BOOL "" FORCE)
set(COMMONLIB_OPTION_TOML OFF CACHE BOOL "" FORCE)
set(COMMONLIB_OPTION_JSON OFF CACHE BOOL "" FORCE)

set(COMMONLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/CommonLibF4")

add_subdirectory("${COMMONLIB_ROOT}" CommonLibF4)

include("${COMMONLIB_ROOT}/cmake/GenerateXsePluginInfo.cmake")
include("${COMMONLIB_ROOT}/cmake/SetXseOutputPath.cmake")
include("${COMMONLIB_ROOT}/cmake/CopyXseOutput.cmake")

# --- Info del plugin (va al .dll y a F4SE) ---

set(COMMONLIB_PLUGIN_NAME "${PROJECT_NAME}")
set(COMMONLIB_PLUGIN_AUTHOR "Kechu")
set(COMMONLIB_PLUGIN_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(COMMONLIB_PLUGIN_COMMENTS "")
set(COMMONLIB_PLUGIN_LICENSE "")
set(COMMONLIB_PLUGIN_VERSION "${PROJECT_VERSION}")
string(TIMESTAMP COMMONLIB_PLUGIN_COPYRIGHT_YEAR "%Y")

generate_xse_plugin_info("${COMMONLIB_ROOT}")

# --- Plugin ---

add_library(
	"${PROJECT_NAME}"
	SHARED
		"src/Main.cpp"
		"${XSE_PLUGIN_INFO_CPP_TARGET}"
		"${WIN32_VERSION_TARGET}"
)

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_CURRENT_BINARY_DIR}/include"
)

target_precompile_headers(
	"${PROJECT_NAME}"
	PRIVATE
		"src/PCH.hpp"
)

target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
		"CommonLibF4::CommonLibF4"
)

# copia el .dll al juego / mod manager si existe la variable de entorno
# XSE_FO4_MODS_PATH o XSE_FO4_GAME_PATH
set_xse_output_path("${PROJECT_NAME}")
copy_xse_copy_output("${PROJECT_NAME}")