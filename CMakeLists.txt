cmake_minimum_required(VERSION 3.21)
project(snake LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROOT "${CMAKE_CURRENT_LIST_DIR}")

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

message(STATUS "SNAKE_SOURCES = ${SNAKE_SOURCES}")

if (SNAKE_SOURCES STREQUAL "")
  message(FATAL_ERROR "No .c files found in ${ROOT}/src")
endif()

add_executable(snake ${SNAKE_SOURCES})

target_include_directories(snake PRIVATE include)

find_package(SDL3 CONFIG REQUIRED)

if (WIN32)
  # Prefer static target on Windows if available
  if (TARGET SDL3::SDL3-static)
    target_link_libraries(snake PRIVATE SDL3::SDL3-static)
  else()
    target_link_libraries(snake PRIVATE SDL3::SDL3)
  endif()
else()
  target_link_libraries(snake PRIVATE SDL3::SDL3)
endif()


target_compile_options(snake PRIVATE
  $<$<C_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

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
