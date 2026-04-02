



file(GLOB_RECURSE _Heuristic_Sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(HeuristicTests ${_Heuristic_Sources})
# Optional, but common: local includes

target_link_libraries(HeuristicTests
AtlasNetTestUtils
gtest
)

gtest_discover_tests(HeuristicTests)

# Optional labels for filtering in ctest
set_tests_properties(${HeuristicTests_TESTS} PROPERTIES LABELS "Heuristic")
