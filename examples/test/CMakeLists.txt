# This file builds example

include(${CMAKE_CURRENT_LIST_DIR}/../../CMakeLists.txt)

cmake_minimum_required (VERSION 2.6)
project (asynctree-test)

find_package(Boost REQUIRED COMPONENTS chrono)
add_definitions(-DBOOST_ALL_NO_LIB)

set(SOURCES 
	${CMAKE_CURRENT_LIST_DIR}/main.cpp
	)
set(HEADERS
	)

add_executable(asynctree-test ${SOURCES} ${HEADERS})
target_include_directories(
	asynctree-test
	PRIVATE
	${Boost_INCLUDE_DIRS} 
	)
target_link_libraries(asynctree-test asynctree ${Boost_LIBRARIES})

