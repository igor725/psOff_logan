cmake_minimum_required(VERSION 3.24)
include(ExternalProject)

project(psOff_logan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(THIRDPARTY_WORKDIR ${CMAKE_BINARY_DIR}/third_build)

if(WIN32)
	add_compile_definitions(NOMINMAX)
endif()

include_directories(BEFORE
	${CMAKE_SOURCE_DIR}/
	${THIRDPARTY_WORKDIR}/include/
)

add_subdirectory(libplog)

# Build third party stuff

ExternalProject_Add(zlib_project
	GIT_REPOSITORY https://github.com/madler/zlib.git
	GIT_TAG v1.3.1
	CMAKE_ARGS
	-DCMAKE_BUILD_TYPE:STRING=Release
	-DCMAKE_INSTALL_PREFIX=${THIRDPARTY_WORKDIR}
	-DCMAKE_INSTALL_MESSAGE=${CMAKE_INSTALL_MESSAGE}
	-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
	-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}

	-DZLIB_BUILD_TESTING:BOOL=OFF
	-DZLIB_BUILD_STATIC:BOOL=OFF
	-DZLIB_BUILD_MINIZIP:BOOL=OFF
)

ExternalProject_Add(libzip_project
	GIT_REPOSITORY https://github.com/nih-at/libzip.git
	GIT_TAG v1.11.3
	CMAKE_ARGS
	-DCMAKE_BUILD_TYPE:STRING=Release
	-DCMAKE_INSTALL_PREFIX=${THIRDPARTY_WORKDIR}
	-DCMAKE_INSTALL_MESSAGE=${CMAKE_INSTALL_MESSAGE}
	-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
	-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}

	-DENABLE_COMMONCRYPTO:BOOL=OFF
	-DENABLE_GNUTLS:BOOL=OFF
	-DENABLE_MBEDTLS:BOOL=OFF
	-DENABLE_OPENSSL:BOOL=OFF
	-DENABLE_FDOPEN:BOOL=OFF
	-DBUILD_TOOLS:BOOL=OFF
	-DBUILD_REGRESS:BOOL=OFF
	-DBUILD_EXAMPLES:BOOL=OFF
	-DBUILD_DOC:BOOL=OFF
	-DENABLE_ZSTD:BOOL=OFF
	-DENABLE_LZMA:BOOL=OFF
	-DENABLE_BZIP2:BOOL=OFF
	-DLIBZIP_DO_INSTALL:BOOL=ON
)
ExternalProject_Add_StepDependencies(libzip_project install zlib_project)

# - Build third party stuff

add_executable(psOff_logan
	main.cpp
)

target_link_libraries(psOff_logan PRIVATE winhttp zip)
add_dependencies(psOff_logan libzip_project)

add_dependencies(psOff_logan plog)

target_link_directories(psOff_logan PRIVATE ${THIRDPARTY_WORKDIR}/lib/)

target_link_libraries(psOff_logan PRIVATE plog)

install(DIRECTORY "${THIRDPARTY_WORKDIR}/bin/" DESTINATION bin)
install(TARGETS psOff_logan COMPONENT psOff_logan DESTINATION bin)
if(WIN32)
	install(FILES $<TARGET_PDB_FILE:psOff_logan> DESTINATION debug OPTIONAL)
endif()
