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

add_subdirectory(libp7d)

if(CMAKE_JS_VERSION)
	add_library(psOff_logan SHARED
		nodelib/main.cpp

		${CMAKE_JS_SRC}
	)

	execute_process(COMMAND node -p "require('node-addon-api').include"
								WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
								OUTPUT_VARIABLE NODE_ADDON_API_DIR)
	string(REPLACE "\n" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
	string(REPLACE "\"" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
	target_include_directories(psOff_logan PRIVATE ${NODE_ADDON_API_DIR})

	add_compile_definitions(-DNAPI_VERSION=8)
	set_target_properties(psOff_logan PROPERTIES PREFIX "" SUFFIX ".node")
	target_include_directories(psOff_logan PRIVATE ${CMAKE_JS_INC})
	target_link_libraries(psOff_logan PRIVATE ${CMAKE_JS_LIB})

	if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
		execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
	endif()
else()
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
		standalone/main.cpp
	)
	
	target_link_libraries(psOff_logan PRIVATE winhttp zip)
	add_dependencies(psOff_logan libzip_project)
endif()

add_dependencies(psOff_logan p7d)

target_link_directories(psOff_logan PRIVATE ${THIRDPARTY_WORKDIR}/lib/)

target_link_libraries(psOff_logan PRIVATE p7d)

install(DIRECTORY "${THIRDPARTY_WORKDIR}/bin/" DESTINATION bin)
install(TARGETS psOff_logan COMPONENT psOff_logan DESTINATION bin)
install(FILES $<TARGET_PDB_FILE:psOff_logan> DESTINATION debug OPTIONAL)
