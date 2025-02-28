cmake_minimum_required(VERSION 3.24)

project(test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INSTALL_MESSAGE LAZY)

add_executable(test
	main.cpp
	p7d.cpp
	p7da.cpp
)
