project(p7d VERSION 0.1)

if(EMSCRIPTEN)
	set(PRJ_TYPE STATIC)
else()
	set(PRJ_TYPE SHARED)
endif()

add_library(p7d ${PRJ_TYPE}
	p7d.cpp
	p7da.cpp
)


if(EMSCRIPTEN)
target_link_libraries(p7d PRIVATE embind)

set_target_properties(p7d PROPERTIES LINK_FLAGS "-sMODULARIZE=1 -sEXPORT_ES6=1")
else()
	install(TARGETS p7d RUNTIME DESTINATION .)
	install(FILES $<TARGET_PDB_FILE:p7d> DESTINATION debug OPTIONAL)
endif()
