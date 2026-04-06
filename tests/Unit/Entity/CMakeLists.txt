




add_executable(EntityTests ${CMAKE_CURRENT_SOURCE_DIR}/EntityTests.cpp)
# Optional, but common: local includes

target_link_libraries(EntityTests
AtlasNetCore
gtest
)

gtest_discover_tests(EntityTests)

# Optional labels for filtering in ctest
set_tests_properties(${EntityTests_TESTS} PROPERTIES LABELS "Entity")
