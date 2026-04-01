




add_executable(RPCTests ${CMAKE_CURRENT_SOURCE_DIR}/RPC.cpp)
# Optional, but common: local includes

target_link_libraries(RPCTests
AtlasNetCore
gtest
)

gtest_discover_tests(RPCTests)

# Optional labels for filtering in ctest
set_tests_properties(${RPCTests_TESTS} PROPERTIES LABELS "RPC")
