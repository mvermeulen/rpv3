# CMake configuration for tests

# Build unit test executable
add_executable(test_rpv3_options 
    test_rpv3_options.c
    ${CMAKE_SOURCE_DIR}/rpv3_options.c
)

target_include_directories(test_rpv3_options PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(test_rpv3_options PROPERTIES
    C_STANDARD 11
)

# Add unit tests to CTest
add_test(NAME UnitTests COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_unit_tests.sh)

# Add integration tests to CTest
add_test(NAME IntegrationTests COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_integration.sh)

# Add regression tests to CTest
add_test(NAME RegressionTests COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_regression.sh)

# Add master test runner
add_test(NAME AllTests COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh)

# Set test properties
set_tests_properties(UnitTests IntegrationTests RegressionTests AllTests PROPERTIES
    ENVIRONMENT "BUILD_DIR=${CMAKE_BINARY_DIR}"
)
