


#add_subdirectory(RedisConn)


add_executable(ValkeyConnTest ValkeyConn/ValkeyConnTest.cpp)
target_link_libraries(ValkeyConnTest PRIVATE AtlasNetCore
gtest)

# Python drives the container + runs the C++ binary
# working dir set to the output of the ValkeyConnTest
add_test(
    NAME IntegrationTest_Valkey
    COMMAND ${VENV_PYTHON} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/ValkeyConn/ValkeyConnTest.py -v
    WORKING_DIRECTORY $<TARGET_FILE_DIR:ValkeyConnTest>  # so ./ValkeyConnTest is found
)
