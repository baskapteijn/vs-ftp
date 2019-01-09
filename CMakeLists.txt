cmake_minimum_required(VERSION 3.2.3)
project(vs-ftp C)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

set(SRC_DIR src)
include_directories(${SRC_DIR})

set(SOURCE_FILES ${SRC_DIR}/main.c ${SRC_DIR}/version.h)

add_executable(vs-ftp ${SOURCE_FILES})
