cmake_minimum_required(VERSION 3.12)
project(LabAllocator C)

set(CMAKE_C_STANDARD 11)

add_executable(LabAllocator main.c mem.c mem.h)