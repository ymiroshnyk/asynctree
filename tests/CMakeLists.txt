cmake_minimum_required(VERSION 3.12)

set(gtest_force_shared_crt ON CACHE BOOL "Force use dynamic C++ runtime" FORCE)
add_subdirectory(googletest-release-1.10.0 EXCLUDE_FROM_ALL)

project(asynctree.tests)

file(GLOB_RECURSE COMPILABLE_FILES CONFIGURE_DEPENDS *.cpp *.h *.hpp)

add_executable(${PROJECT_NAME}
        ${COMPILABLE_FILES})

target_include_directories(${PROJECT_NAME}
        PUBLIC
        PRIVATE
        )

target_link_libraries(${PROJECT_NAME}
        PUBLIC
        PRIVATE
        asynctree
        gtest
        )

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES TIMEOUT 5)
