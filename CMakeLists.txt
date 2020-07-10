cmake_minimum_required(VERSION 3.2.3)
project(vs-ftp C)

set(SRC_DIR src)
include_directories(${SRC_DIR})

set(SOURCE_FILES
        ${SRC_DIR}/main.c
        ${SRC_DIR}/version.h
        ${SRC_DIR}/vsftp_server.c
        ${SRC_DIR}/vsftp_server.h
        ${SRC_DIR}/vsftp_commands.c
        ${SRC_DIR}/vsftp_commands.h
        ${SRC_DIR}/vsftp_filesystem.c
        ${SRC_DIR}/vsftp_filesystem.h
        ${SRC_DIR}/config.h)

add_executable(vs-ftp ${SOURCE_FILES})
