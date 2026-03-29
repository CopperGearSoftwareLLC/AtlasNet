


# Recursively collect sources under ./src
file(GLOB_RECURSE _unit_test_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(AtlasNetUnitTests ${_unit_test_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetUnitTests
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(AtlasNetUnitTests
AtlasNetCore
gtest
)
include(GoogleTest)
gtest_discover_tests(AtlasNetUnitTests)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNetUnitTests_TESTS} PROPERTIES LABELS "unit")
