cmake_minimum_required(VERSION 3.17)
project(CPS2008_Tetris_Client C)
set(CMAKE_C_STANDARD 99)

set(DEFAULT_BUILD_TYPE "Release")
set(SOURCE_FILES src/client_server.c)
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} pthread m)

install(TARGETS CPS2008_Tetris_Client DESTINATION lib)
install(FILES include/client_server.h DESTINATION include)
