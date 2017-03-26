cmake_minimum_required(VERSION 2.8)

project(josch)

set (SOURCES "main.cpp" "tlv.h"
    "job.cpp" "job.h"
    "josch.cpp" "josch.h"
    "client_handler.cpp" "client_handler.h"
    "conn_handler.cpp" "conn_handler.h"
    "tlv_client.cpp" "tlv_client.h")

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} pthread)
