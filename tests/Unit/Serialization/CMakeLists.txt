




add_executable(SerializationTests ${CMAKE_CURRENT_SOURCE_DIR}/Serialization.cpp)
# Optional, but common: local includes

target_link_libraries(SerializationTests
AtlasNetTestUtils
gtest
)

gtest_discover_tests(SerializationTests)

# Optional labels for filtering in ctest
set_tests_properties(${SerializationTests_TESTS} PROPERTIES LABELS "Serialization")
