 message(STATUS "Node.js integration enabled")

    set(NODE_PROJECT_DIR apps/Cartograph/web)

    execute_process(
        COMMAND node -p "process.execPath"
        OUTPUT_VARIABLE NODE_EXEC
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NODE_EXEC STREQUAL "")
        message(FATAL_ERROR "Node.js not found but ENABLE_NODE=ON") 
    endif()

    get_filename_component(NODE_BIN_DIR "${NODE_EXEC}" DIRECTORY)
    get_filename_component(NODE_ROOT_DIR "${NODE_BIN_DIR}" DIRECTORY)

    set(NODE_INCLUDE_DIR "${NODE_ROOT_DIR}/include/node")

    if (NOT EXISTS "${NODE_INCLUDE_DIR}/node_api.h")
        message(FATAL_ERROR
            "Node headers not found at ${NODE_INCLUDE_DIR}\n"
            "Make sure Node is installed with development headers"
        )
    endif()

    message(STATUS "Node include dir = ${NODE_INCLUDE_DIR}")

        message(STATUS "installing npm to ${CARTOGRAPH_WEB_ROOT}")
    message(STATUS "npm install")
    execute_process(
        COMMAND "npm install"
        WORKING_DIRECTORY ${CARTOGRAPH_WEB_ROOT}
    )
    execute_process(
        COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CARTOGRAPH_WEB_ROOT}
        OUTPUT_VARIABLE NODE_ADDON_API_INCLUDE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT NODE_ADDON_API_INCLUDE)
    message(FATAL_ERROR "❌ node-addon-api include path is empty. Make sure 'node-addon-api' is installed in ${NODE_PROJECT_DIR}.")
endif()
    string(REPLACE "\"" "" NODE_ADDON_API_INCLUDE "${NODE_ADDON_API_INCLUDE}")
    include(cmake/AddSwigLibrary.cmake)