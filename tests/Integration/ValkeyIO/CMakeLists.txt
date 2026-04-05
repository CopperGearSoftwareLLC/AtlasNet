

#add_executable(ValkeyIOTest ValkeyConnTest.cpp)
#target_link_libraries(ValkeyIOTest PRIVATE AtlasNetCore gtest)


#add_test(
#    NAME Valkey_DockerNet
#    COMMAND ${VENV_PYTHON} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/ValkeySetup.py -v
#    WORKING_DIRECTORY $<TARGET_FILE_DIR:ValkeyIOTest>  # so ./ValkeyIOTest is found
#)
#set_tests_properties(Valkey_DockerNet PROPERTIES
#    ENVIRONMENT "NATIVE_BINARY=$<TARGET_FILE:ValkeyIOTest>"
#)
#
#
#add_test(
#    NAME ValkeyCluster_DockerNet
#    COMMAND ${VENV_PYTHON} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/ValkeySetupCluster.py -v
#    WORKING_DIRECTORY $<TARGET_FILE_DIR:ValkeyIOTest>  # so ./ValkeyIOTest is found
#)
#set_tests_properties(ValkeyCluster_DockerNet PROPERTIES
#    ENVIRONMENT "NATIVE_BINARY=$<TARGET_FILE:ValkeyIOTest>"
#)

add_executable(ValkeyIO ValkeyIO.cpp)
target_link_libraries(ValkeyIO PRIVATE AtlasNetCore gtest)
gtest_discover_tests(ValkeyIO)

# Optional labels for filtering in ctest
set_tests_properties(${ValkeyIO_TESTS} PROPERTIES LABELS "ValkeyIO")