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

# For fetching GoogleTests -- Required by GoogleBenchmark (seemingly)
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

# For fetching GoogleBenchmark
include(FetchContent)
FetchContent_Declare(
    GoogleBenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG 31c66f47f60d06790a4885a261d682ba339bd100 #refs/tags/v1.9.4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(GoogleBenchmark)

# Create an empty list for the Benchmark
set( BM_list )
set( RUN_BM_COMMANDS )

# Function to create a Benchmark
macro( _add_BM name )
    # Add the benchmark to the list
    list( APPEND BM_list ${name}_BM )
    
    # Create the Benchmark
    add_executable(${name}_BM ${name}.cpp)
    target_link_libraries(${name}_BM PRIVATE SPAPQueue benchmark::benchmark ProjectExecutableFlags)
    target_include_directories(${name}_BM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    # Extra required library for Windows
    if ("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
        target_link_libraries(${name}_BM PRIVATE Shlwapi)
    endif()

    # Adding executable to to the run all benchmark command
    list(APPEND RUN_BM_COMMANDS COMMAND $<TARGET_FILE:${name}_BM>)
endmacro()

# Adding Benchmarks
_add_BM( RingBuffer )
_add_BM( SpapqFibonacci )
_add_BM( SpapqSSSP )

# Custom target to compile all the Benchmarks
add_custom_target( build_BM DEPENDS ${BM_list} )
add_custom_target( run_BM COMMAND ${RUN_BM_COMMANDS} DEPENDS build_BM )