




add_executable(AddressTests ${CMAKE_CURRENT_SOURCE_DIR}/Address.cpp)
# Optional, but common: local includes

target_link_libraries(AddressTests
AtlasNetTestUtils
gtest
)

gtest_discover_tests(AddressTests)

# Optional labels for filtering in ctest
set_tests_properties(${AddressTests_TESTS} PROPERTIES LABELS "Address")
