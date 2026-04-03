
add_executable(AtlasNetMessageTestsLocal ${CMAKE_CURRENT_SOURCE_DIR}/MessageLocal.cpp)

target_link_libraries(AtlasNetMessageTestsLocal
AtlasNetCore
AtlasNetTestCrashUtils
gtest
)

gtest_discover_tests(AtlasNetMessageTestsLocal)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNetMessageTestsLocal_TESTS} PROPERTIES LABELS "MessageLocal")