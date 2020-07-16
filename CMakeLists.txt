cmake_minimum_required(VERSION 3.2.3)
project(vs-ftp C)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -Wextra -Os -s")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -fprofile-arcs -ftest-coverage")

set(COMMON_SRC_DIR src/common)
set(LINUX_SRC_DIR src/linux)
include_directories(${COMMON_SRC_DIR} ${LINUX_SRC_DIR})

set(SOURCE_FILES
    ${LINUX_SRC_DIR}/main.c
    ${LINUX_SRC_DIR}/version.h
    ${LINUX_SRC_DIR}/config.h
    ${LINUX_SRC_DIR}/io.c
    ${LINUX_SRC_DIR}/io.h
    ${COMMON_SRC_DIR}/vsftp_server.c
    ${COMMON_SRC_DIR}/vsftp_server.h
    ${COMMON_SRC_DIR}/vsftp_commands.c
    ${COMMON_SRC_DIR}/vsftp_commands.h
    ${COMMON_SRC_DIR}/vsftp_filesystem.c
    ${COMMON_SRC_DIR}/vsftp_filesystem.h)

add_executable(vs-ftp ${SOURCE_FILES})
