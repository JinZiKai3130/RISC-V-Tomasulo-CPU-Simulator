cmake_minimum_required(VERSION 3.16)

project(RISCV_Tomasulo_Simulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 可执行文件输出到项目根目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR})

add_executable(code
  src/main.cpp
  src/simulator.cpp
  src/decode.cpp
  src/memory.cpp
  src/ROB.cpp
  src/RS.cpp
  src/RAT.cpp
  src/LSQ.cpp
)

target_include_directories(code PRIVATE include)

# 模拟器是 CPU 密集型程序，启用优化选项
target_compile_options(code PRIVATE
  $<$<CONFIG:Release>:-O2>
)
