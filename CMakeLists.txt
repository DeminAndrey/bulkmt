cmake_minimum_required(VERSION 3.12)

set(PATCH_VERSION "1" CACHE INTERNAL "Patch version")
set(PROJECT_VESRION 0.0.${PATCH_VERSION})

project(async VERSION ${PROJECT_VESRION})

configure_file(version.h.in version.h)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads)
find_package(Boost)

if(WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set(BINARY_DIR "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BINARY_DIR}/lib")

add_library(async
  SHARED
  async.h
  async.cxx
  command_processor.h)

target_include_directories(async
  PRIVATE ${Boost_INCLUDE_DIR})

target_include_directories(async
  PRIVATE ${Boost_INCLUDE_DIR})

target_link_libraries(async
  PRIVATE ${Boost_LIBRARY_DIR})

add_executable(async_cli main.cxx)

target_link_libraries(async_cli async)
