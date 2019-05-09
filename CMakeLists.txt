cmake_minimum_required (VERSION 3.12)
project (asynctree)

file(GLOB_RECURSE COMPILABLE_FILES CONFIGURE_DEPENDS src/*.cpp include/*.h)

# boost >>
if (MSVC)
	set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost REQUIRED COMPONENTS system thread)
add_definitions(-DBOOST_ALL_NO_LIB)
# << boost
	
add_library(${PROJECT_NAME} STATIC 
	${COMPILABLE_FILES}
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
	Boost::boost
	Boost::system
	Boost::thread
)

add_subdirectory(examples)
