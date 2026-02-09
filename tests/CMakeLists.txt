
cmake_minimum_required(VERSION 3.25)
project(flow_tests)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(flow_integration_tests
    integration/flow-test.cpp
    ${CMAKE_SOURCE_DIR}/src/flow/core/BackendAPI.cpp
    ${CMAKE_SOURCE_DIR}/src/flow/core/ProtocolGuard.cpp
    ${CMAKE_SOURCE_DIR}/src/flow/core/ConnectionManager.cpp
    ${CMAKE_SOURCE_DIR}/src/flow/measurement/ISPDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/flow/protocols/XRayHandler.cpp
)

target_link_libraries(flow_integration_tests PRIVATE
    Qt6::Core
    Qt6::Network
    GTest::gtest_main
)

target_include_directories(flow_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/flow
    ${CMAKE_SOURCE_DIR}/client
    ${CMAKE_SOURCE_DIR}/common/logger
)

# Mock definitions if needed
add_compile_definitions(AMNEZIA_DESKTOP)

add_test(NAME FlowIntegrationTest COMMAND flow_integration_tests)
