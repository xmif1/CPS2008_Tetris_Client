cmake_minimum_required(VERSION 3.17)
project(CPS2008_Tetris_Client C)

set(CMAKE_C_STANDARD 99)

add_library(CPS2008_Tetris_Client SHARED client_server.c client_server.h)