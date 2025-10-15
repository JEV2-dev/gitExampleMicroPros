cmake_minimum_required(VERSION 3.10)
project(tcp_caesar C)

set(CMAKE_C_STANDARD 11)
add_library(caesar STATIC caesar.c)
add_executable(server server.c)
add_executable(client client.c)
target_link_libraries(server PRIVATE caesar)
target_link_libraries(client PRIVATE caesar)
