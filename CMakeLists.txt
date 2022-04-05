cmake_minimum_required(VERSION 3.7.2)
project(Test C)

set(CMAKE_C_STANDARD 99)

add_executable(Test main.c main.c main.c functions.c functions.h server.c server.h)
