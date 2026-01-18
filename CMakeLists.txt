cmake_minimum_required(VERSION 3.21)
project(snake LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROOT "${CMAKE_CURRENT_LIST_DIR}")

# Collect ALL .c files from src/ without globbing
aux_source_directory("${ROOT}/src" SNAKE_SOURCES)

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
