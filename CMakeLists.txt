cmake_minimum_required (VERSION 3.8)

project (tcpp)

# Global variables are declared here
set(TCPP_TESTS_NAME "tests")

# Global options are declared here
option(IS_PLUGIN_BUILDING_ENABLED "The option shows whether plugins should be built or not" ON)
option(IS_SAMPLES_BUILDING_ENABLED "The option shows whether sample projects should be built or not" ON)
option(IS_TESTING_ENABLED "The option turns on/off tests" ON)

if (IS_TESTING_ENABLED)
	enable_testing()
	add_subdirectory(tests)
endif ()

add_executable(play play.cpp)
target_include_directories(play PUBLIC ./source)
set_property(TARGET play PROPERTY CXX_STANDARD 20)