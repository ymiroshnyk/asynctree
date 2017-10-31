cmake_minimum_required (VERSION 3.2)
project (asynctree)

find_package(Boost REQUIRED COMPONENTS system thread)
add_definitions(-DBOOST_ALL_NO_LIB)


set(HEADERS
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree.h
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree_access_key.h
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree_config.h
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree_mutex.h
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree_qt_connector.h
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree_service.h
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree_task.h
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree_task_callbacks.h
	${CMAKE_CURRENT_LIST_DIR}/include/asynctree_task_typedefs.h
	)

set(SOURCES 
	${CMAKE_CURRENT_LIST_DIR}/src/asynctree_service.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/asynctree_task.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/asynctree_mutex.cpp
	)
	
add_library(asynctree STATIC ${HEADERS} ${SOURCES})

set_target_properties(asynctree PROPERTIES CXX_STANDARD 11)

target_include_directories(
	asynctree
	PUBLIC	
	${CMAKE_CURRENT_LIST_DIR}/include
#	${CMAKE_CURRENT_LIST_DIR}/externals/pools/include
	${Boost_INCLUDE_DIRS} 
	)

target_link_libraries(asynctree 
	${Boost_LIBRARIES}
)