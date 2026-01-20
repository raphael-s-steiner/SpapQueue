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
project(SPAPQueue VERSION 1.1 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Ensures C++ standard library is strictly enforced

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
    message(STATUS "CMAKE_BUILD_TYPE was not set. Building RelWithDebInfo.")
endif()

# Display status messages for build types
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring for Debug build. Debug symbols enabled.")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring for Release build. Optimization -O3 and -DNDEBUG enabled, and -march=native.")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Configuring for Release with Debug Info build. Optimization -O2 and debug symbols enabled.")
else()
    message(STATUS "Build type '${CMAKE_BUILD_TYPE}' not recognised.")
endif()

# --- Compile Options Configuration ---
set(COMMON_CXX_WARNING_FLAGS
    "-Wall"
    "-Wextra"
    "-pedantic"
    "-Wpedantic"
    "-Wshadow"
    "-Wnon-virtual-dtor"
    "-Wold-style-cast"
    "-Wfatal-errors"
    "-Wconversion"
    "-Wsign-conversion"
    "-Wunused"
    "-Wunreachable-code"
    "-Wuninitialized"
    "-Wno-interference-size" # disables warning that std::hardware_destructive_interference_size may depend on hardware, compiler, and flags
    "-Werror"
    "-Wnrvo"
)

# --- Project-specific Compile Flags ---
# An interface library to hold compile flags for executables built in this project.
add_library(ProjectExecutableFlags INTERFACE)
target_compile_options(ProjectExecutableFlags INTERFACE ${COMMON_CXX_WARNING_FLAGS})

# Set build-type specific compile options
target_compile_options(ProjectExecutableFlags INTERFACE
    $<$<CONFIG:DEBUG>:-g>
    $<$<CONFIG:RELEASE>:-O3;-DNDEBUG;-march=native>
    $<$<CONFIG:RELWITHDEBINFO>:-O2;-g>
)

# Flag for address sanitizer
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if (ENABLE_ASAN)
    message(STATUS "Address sanitizer enabled.")
    target_compile_options(ProjectExecutableFlags INTERFACE -fsanitize=address)
    target_link_options(ProjectExecutableFlags INTERFACE -fsanitize=address)
endif ()

# Flag for undefined behaviour sanitizer
option(ENABLE_UBSAN "Enable UndefinedBehaviourSanitizer" OFF)
if (ENABLE_UBSAN)
    message(STATUS "Undefined behaviour sanitizer enabled.")
    target_compile_options(ProjectExecutableFlags INTERFACE -fsanitize=undefined)
    target_link_options(ProjectExecutableFlags INTERFACE -fsanitize=undefined)
endif ()

# --- Library Definition ---
add_library(SPAPQueue INTERFACE)

# Setting environment variable for debugging
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(SPAPQueue INTERFACE SPAPQ_DEBUG)
endif()

target_include_directories(SPAPQueue INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Using and linking pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(SPAPQueue INTERFACE Threads::Threads)

# --- Build Tests Configuration ---
# Option to control whether tests are built.
option(BUILD_TESTS "Build tests for the project" ON)

if(BUILD_TESTS)
    message(STATUS "Tests will be built.")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Tests will not be built (BUILD_TESTS is OFF).")
endif()

# Option to control whether benchmarks are built.
option(BUILD_BENCHMARK "Build benchmarks for the project" ON)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_BENCHMARK OFF)
endif()

if(BUILD_BENCHMARK)
    message(STATUS "Benchmarks will be built.")
    add_subdirectory(benchmarks)
else()
    message(STATUS "Benchmarks will not be built (BUILD_BENCHMARK is OFF).")
endif()

# --- Documentation Generation ---
# Add a custom target to build Doxygen documentation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    message(STATUS "FOUND Doxygen. 'doc' target available for building documentation.")
    include(FetchContent)
    FetchContent_Declare(
        doxygen-awesome-css
        URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/heads/main.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(doxygen-awesome-css)

    # Save the location the files were cloned into
    # This allows us to get the path to doxygen-awesome.css
    FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

    # Generate the Doxyfile
    set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in)
    set(DOXYFILE_OUT ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile)
    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else()
    message(STATUS "Doxygen not found. Documentation target 'doc' will not be available.")
endif()