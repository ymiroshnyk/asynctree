# This file builds example

include(${CMAKE_CURRENT_LIST_DIR}/../../CMakeLists.txt)

cmake_minimum_required (VERSION 2.6)
project (asynctree-blurtest)

find_package(Boost REQUIRED)
add_definitions(-DBOOST_ALL_NO_LIB)

#Qt
find_package(Qt4 REQUIRED QtCore QtGui)

qt4_wrap_cpp(HEADERS_MOC ${CMAKE_CURRENT_LIST_DIR}/main.h)
#qt4_wrap_ui(HEADERS_UI )
#qt4_add_resources(QT_ADDED_RESOURCES )

include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

set(SOURCES 
	${CMAKE_CURRENT_LIST_DIR}/main.h
	${CMAKE_CURRENT_LIST_DIR}/main.cpp
	${HEADERS_MOC}
	${HEADERS_UI}
	${QT_ADDED_RESOURCES}
	)
set(HEADERS
	)

#for generated files by qt wrappers
include_directories(${CMAKE_CURRENT_BINARY_DIR})
	
add_executable(asynctree-blurtest ${SOURCES} ${HEADERS})
target_include_directories(asynctree-blurtest
	PRIVATE
	${Boost_INCLUDE_DIRS} 
	)
target_link_libraries(asynctree-blurtest
	asynctree 
	${Boost_LIBRARIES}
	${QT_LIBRARIES})

