project(p7d VERSION 0.1)

add_library(p7d SHARED
	p7d.cpp
	p7da.cpp
)

install(TARGETS p7d RUNTIME DESTINATION bin)
if(WIN32)
	install(FILES $<TARGET_PDB_FILE:p7d> DESTINATION debug OPTIONAL)
endif()
