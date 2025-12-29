# Copyright 2025 Raphael S. Steiner
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20)

# For fetching GoogleTests
include(FetchContent)
FetchContent_Declare(
    GoogleTest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(GoogleTest)
include(GoogleTest)

# Create an empty list for the tests
set( tests_list )

# Function to create a test
macro( _add_test name )
    # Add the test to the list
    list( APPEND tests_list ${name}Test )

    # Create the test
    add_executable(${name}Test ${name}.cpp)
    target_link_libraries(${name}Test PRIVATE SPAPQueue GTest::gtest_main ProjectExecutableFlags)
    target_include_directories( ${name}Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include )

    gtest_discover_tests(${name}Test)
endmacro()

# Adding tests
_add_test( RingBuffer )
_add_test( QNetwork )
_add_test( DiscrepancyTables )
_add_test( SpapQueue )
_add_test( Concepts )

# Custom target to compile all the tests
add_custom_target( build_tests DEPENDS ${tests_list} )
add_custom_target( run_tests COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS build_tests )