# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.5)

# Include the components directory of the main application:
#
set(EXTRA_COMPONENT_DIRS "../components")

# Set the components to include the tests for.
# - cmake -D TEST_COMPONENTS="spotify" - this requires the 'CACHE' in the set arguments
# - idf.py -D TEST_COMPONENTS="spotify"
#
set(TEST_COMPONENTS "rfid_reader spotify" CACHE STRING "List of components to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(espotify_test)
