cmake_minimum_required(VERSION 3.21)
project(snake LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROOT "${CMAKE_CURRENT_LIST_DIR}")

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(SDL3 CONFIG REQUIRED)
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libs for vendored deps" FORCE)
set(SDL3MIXER_VENDORED OFF CACHE BOOL "Use system SDL3" FORCE)
set(SDL3MIXER_SAMPLES OFF CACHE BOOL "Skip SDL3_mixer sample programs" FORCE)
set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "Disable dynamic dependency loading" FORCE)
set(SDLMIXER_OPUS_SHARED OFF CACHE BOOL "Link libopus/opusfile statically" FORCE)
set(SDLMIXER_OPUSFILE_SHARED OFF CACHE BOOL "Link opusfile statically" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE_SHARED OFF CACHE BOOL "Link vorbisfile statically" FORCE)
set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "Disable mpg123 MP3 backend" FORCE)
set(SDLMIXER_MIDI_FLUIDSYNTH OFF CACHE BOOL "Disable FluidSynth backend" FORCE)
set(SDLMIXER_MOD_XMP OFF CACHE BOOL "Disable libxmp MOD backend" FORCE)
set(SDLMIXER_FLAC OFF CACHE BOOL "Disable FLAC backends" FORCE)
set(SDLMIXER_GME OFF CACHE BOOL "Disable GME backends" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "Disable WavPack backend" FORCE)

FetchContent_Declare(
  SDL3_mixer
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
  GIT_TAG main
)
FetchContent_MakeAvailable(SDL3_mixer)

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

if (NOT WIN32)
  set_property(TARGET snake PROPERTY LINKER_LANGUAGE CXX)
endif()

if (WIN32)
  set_target_properties(snake PROPERTIES WIN32_EXECUTABLE ON)
endif()

target_compile_options(snake PRIVATE
  $<$<C_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

# This is also a hack
if (WIN32 AND TARGET SDL3_mixer::SDL3_mixer)
  add_custom_command(TARGET snake POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
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
