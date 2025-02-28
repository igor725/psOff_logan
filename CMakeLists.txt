cmake_minimum_required(VERSION 3.24)

project(test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(libp7d)

add_executable(test
	main.cpp
)

add_dependencies(test p7d)

target_link_libraries(test PRIVATE p7d)
