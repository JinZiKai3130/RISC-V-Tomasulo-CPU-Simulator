cmake_minimum_required(VERSION 3.16)

project(RISCV_Tomasulo_Simulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(simulator
  src/main.cpp
  src/simulator.cpp
  src/decode.cpp
  src/memory.cpp
  src/ROB.cpp
  src/RS.cpp
)

target_include_directories(simulator PRIVATE include)

# 模拟器是 CPU 密集型程序，启用优化选项
target_compile_options(simulator PRIVATE
  $<$<CONFIG:Release>:-O2>
)
