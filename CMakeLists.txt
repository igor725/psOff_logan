cmake_minimum_required(VERSION 3.24)

project(psOff_logan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(libp7d)

add_executable(psOff_logan
	main.cpp
)

add_dependencies(psOff_logan p7d)

target_link_libraries(psOff_logan PRIVATE p7d)

install(TARGETS psOff_logan COMPONENT psOff_logan DESTINATION .)

install(FILES $<TARGET_PDB_FILE:psOff_logan> DESTINATION debug OPTIONAL)
