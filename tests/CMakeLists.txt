# For fetching GoogleTests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Create an empty list for the tests
set( tests_list )

# Function to create a test
macro( _add_test name )
  # Add the test to the list
  list( APPEND tests_list ${name}Test )
  # Create the test
  add_executable(${name}Test ${name}.cpp)
  target_link_libraries(${name}Test PRIVATE ParallelPriorityQueue GTest::gtest_main ProjectExecutableFlags)
  target_include_directories( ${name}Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include )

  gtest_discover_tests(${name}Test)
endmacro()



_add_test( ringBuffer )



# Custom target to compile all the tests
add_custom_target( build_tests DEPENDS ${tests_list} )
add_custom_target( run_tests COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS build_tests )