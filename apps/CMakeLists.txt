# Copyright 2026 Raphael S. Steiner
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

# Create an empty list for the applications
set( App_list )

# Function to create an application
macro( _add_App name )
    # Add the application to the list
    list( APPEND App_list ${name} )
    
    # Create the Benchmark
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE SPAPQueue ProjectExecutableFlags)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endmacro()

# Adding Applications
_add_App( QNetworkDotWriter )

# Custom target to compile all the Benchmarks
add_custom_target( build_Apps DEPENDS ${App_list} )