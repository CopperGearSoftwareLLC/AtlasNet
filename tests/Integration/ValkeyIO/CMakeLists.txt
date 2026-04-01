

add_executable(ValkeyIOTest ValkeyConnTest.cpp)
target_link_libraries(ValkeyIOTest PRIVATE AtlasNetCore gtest)


add_test(
    NAME IntegrationTest_Valkey
    COMMAND ${VENV_PYTHON} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/ValkeySetup.py -v
    WORKING_DIRECTORY $<TARGET_FILE_DIR:ValkeyIOTest>  # so ./ValkeyIOTest is found
)
set_tests_properties(IntegrationTest_Valkey PROPERTIES
    ENVIRONMENT "NATIVE_BINARY=$<TARGET_FILE:ValkeyIOTest>"
)


#add_test(
#    NAME IntegrationTest_ValkeyCluster
#    COMMAND ${VENV_PYTHON} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/ValkeySetupCluster.py -v
#    WORKING_DIRECTORY $<TARGET_FILE_DIR:ValkeyIOTest>  # so ./ValkeyIOTest is found
#)
#set_tests_properties(IntegrationTest_ValkeyCluster PROPERTIES
#    ENVIRONMENT "NATIVE_BINARY=$<TARGET_FILE:ValkeyIOTest>"
#)
