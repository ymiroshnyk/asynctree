# This file builds example

cmake_minimum_required (VERSION 2.6)
project (blurtest)

# boost >>
find_package(Boost REQUIRED COMPONENTS chrono)
# << boost

# Qt >>
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)
set(CMAKE_AUTOMOC ON)
# << Qt

file(GLOB_RECURSE COMPILABLE_FILES CONFIGURE_DEPENDS *.cpp *.h *.hpp)

add_executable(${PROJECT_NAME} 
	${COMPILABLE_FILES}
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
	PUBLIC
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
	asynctree 
	Boost::chrono
	Qt5::Widgets
)

# Copy Qt5 dll in the binaries directory
if(MSVC)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different 
			$<TARGET_FILE:Qt5::Core> 
			$<TARGET_FILE:Qt5::Gui> 
			$<TARGET_FILE:Qt5::Widgets>
			$<TARGET_FILE_DIR:${PROJECT_NAME}>
	)
endif()
