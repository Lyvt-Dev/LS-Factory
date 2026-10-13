add_executable(forge_tests CoreTests.cpp)
target_link_libraries(forge_tests PRIVATE forge_core Catch2::Catch2WithMain)
forge_warnings(forge_tests)
include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(forge_tests)

target_compile_definitions(forge_tests PRIVATE FORGE_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
