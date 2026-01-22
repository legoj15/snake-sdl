cmake_minimum_required(VERSION 3.21)
project(snake LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROOT "${CMAKE_CURRENT_LIST_DIR}")

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(SDL3 CONFIG REQUIRED)

# --- PLAN B: Fetch SDL3_mixer manually ---
include(FetchContent)

# Prevent SDL_mixer from building its own shared library (we just want to link it statically if possible)
# or just let it build defaults. We set shared to OFF to simplify DLL management usually, 
# but for simplicity let's rely on its defaults or force it to static to avoid copying another DLL.
option(SDL3MIXER_BUILD_EXAMPLES "Build examples" OFF)
option(SDL3MIXER_BUILD_TESTS "Build tests" OFF)

message(STATUS "Fetching SDL3_mixer from GitHub...")
FetchContent_Declare(
  SDL3_mixer
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
  GIT_TAG main  # Pulling from main branch since SDL3 is bleeding edge
)
FetchContent_MakeAvailable(SDL3_mixer)
# ------------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}")
endforeach()

# Collect ALL .c files from src/ without globbing
aux_source_directory("${ROOT}/src" SNAKE_SOURCES)

# botlib.c is used for the shared library, not the game executable.
list(REMOVE_ITEM SNAKE_SOURCES "${ROOT}/src/botlib.c")

if (SNAKE_SOURCES STREQUAL "")
  message(FATAL_ERROR "No .c files found in ${ROOT}/src")
endif()

# ------------------------------------------------------------
# Main Executable
# ------------------------------------------------------------
add_executable(snake ${SNAKE_SOURCES})

target_include_directories(snake PRIVATE include)

# Link against SDL3 (from vcpkg) and SDL3_mixer (from FetchContent)
if (WIN32 AND TARGET SDL3::SDL3-static)
  target_link_libraries(snake PRIVATE SDL3::SDL3-static SDL3_mixer::SDL3_mixer)
else()
  target_link_libraries(snake PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer)
endif()

target_compile_options(snake PRIVATE
  $<$<C_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

# This is like uber temporary for the love of god replace this immediately
set(BGM_PATH "${CMAKE_BINARY_DIR}/bgm.wav")
if (NOT EXISTS "${BGM_PATH}")
  message(STATUS "Downloading BGM from legoj15.net...")
  file(DOWNLOAD 
    "https://legoj15.net/cdn/bgm.wav" 
    "${BGM_PATH}" 
    SHOW_PROGRESS
    tls_verify ON
  )
endif()

#This is also a hack
if (WIN32)
  add_custom_command(TARGET snake POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL3_mixer-shared>
    $<TARGET_FILE_DIR:snake>
    COMMENT "Copying SDL3_mixer.dll to executable directory"
  )
endif()

# ------------------------------------------------------------
# Shared library for the Python GUI (ctypes)
# ------------------------------------------------------------

add_library(snakebot SHARED src/botlib.c)
target_include_directories(snakebot PUBLIC include)

if (WIN32)
  target_compile_definitions(snakebot PRIVATE SNAKEBOT_EXPORTS)
endif()

set_target_properties(snakebot PROPERTIES
  OUTPUT_NAME "snakebot"
)

target_compile_options(snakebot PRIVATE
  $<$<C_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
