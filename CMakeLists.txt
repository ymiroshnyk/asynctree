cmake_minimum_required (VERSION 3.12)
project (asynctree)

file(GLOB_RECURSE COMPILABLE_FILES CONFIGURE_DEPENDS src/*.cpp include/*.h)

add_library(${PROJECT_NAME} STATIC 
	${COMPILABLE_FILES}
)

target_compile_definitions(${PROJECT_NAME}
	PUBLIC
	$<$<CONFIG:Debug>:ASYNCTREE_DEBUG>
)

set_target_properties(${PROJECT_NAME} 
	PROPERTIES 
	CXX_STANDARD 11
)

target_include_directories(${PROJECT_NAME}
	PUBLIC	
	include
)

target_link_libraries(${PROJECT_NAME} 
	PUBLIC
)

add_subdirectory(examples)
