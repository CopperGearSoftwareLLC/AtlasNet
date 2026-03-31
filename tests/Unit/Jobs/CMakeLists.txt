




add_executable(JobsTests ${CMAKE_CURRENT_SOURCE_DIR}/Jobs.cpp)
# Optional, but common: local includes

target_link_libraries(JobsTests
AtlasNetCore
gtest
)

gtest_discover_tests(JobsTests)

# Optional labels for filtering in ctest
set_tests_properties(${JobsTests_TESTS} PROPERTIES LABELS "Jobs")
