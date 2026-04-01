

# SERVER


add_executable(MessageTestsSimpleServer ${CMAKE_CURRENT_SOURCE_DIR}/Server.cpp)
# Optional, but common: local includes

target_link_libraries(MessageTestsSimpleServer
AtlasNetCore
AtlasNetTestUtils
)
target_compile_definitions(MessageTestsSimpleServer PRIVATE)

# CLIENT
add_executable(MessageTestsSimpleClient ${CMAKE_CURRENT_SOURCE_DIR}/Client.cpp)
# Optional, but common: local includes
target_link_libraries(MessageTestsSimpleClient
AtlasNetCore
AtlasNetTestUtils
)

target_compile_definitions(MessageTestsSimpleClient PRIVATE )
# Tests

#add_executable(MessageTestsSimple ${CMAKE_CURRENT_SOURCE_DIR}/Tests.cpp)
## Optional, but common: local includes
#
#target_link_libraries(MessageTestsSimple
#AtlasNetTestUtils
#gtest
#)

#target_compile_definitions(MessageTestsSimple PRIVATE 
#SERVER_BINARY="$<TARGET_FILE:MessageTestsSimpleServer>"
#CLIENT_BINARY="$<TARGET_FILE:MessageTestsSimpleClient>"
#)

#add_dependencies(MessageTestsSimple MessageTestsSimpleServer MessageTestsSimpleClient)
#
#gtest_discover_tests(MessageTestsSimple)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
  NAME MessageTest0
  COMMAND ${VENV_PYTHON} -m pytest -q ${CMAKE_CURRENT_SOURCE_DIR}/MessageTests.py -k test_message_0
)

add_test(
  NAME MessageTest1
  COMMAND ${VENV_PYTHON} -m pytest -q ${CMAKE_CURRENT_SOURCE_DIR}/MessageTests.py -k test_message_1
)

add_test(
  NAME MessageTest2
  COMMAND ${VENV_PYTHON} -m pytest -q ${CMAKE_CURRENT_SOURCE_DIR}/MessageTests.py -k test_message_2
)
set_tests_properties(MessageTest0 MessageTest1 MessageTest2 PROPERTIES
  ENVIRONMENT
    "SERVER_BINARY=$<TARGET_FILE:MessageTestsSimpleServer>;CLIENT_BINARY=$<TARGET_FILE:MessageTestsSimpleClient>"
)