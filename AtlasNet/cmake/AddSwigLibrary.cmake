find_package(SWIG REQUIRED)
include(UseSWIG)
set(CMAKE_SWIG_FLAGS
    -c++
    -std=c++20
)
function(add_swig_library_for_target TARGET_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs STL_INCLUDES STL_TEMPLATES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_target_property(TARGET_SRCS ${TARGET_NAME} SOURCES)
    get_target_property(TARGET_INCLUDES ${TARGET_NAME} INCLUDE_DIRECTORIES)

    set(SWIG_I ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.i)

    # Collect headers
    set(HEADERS "")
    foreach(src IN LISTS TARGET_SRCS)
        if(src MATCHES "\\.(h|hpp)$")
            get_filename_component(abs "${src}" ABSOLUTE)
            list(APPEND HEADERS "${abs}")
        endif()
    endforeach()

    # Force CMake to reconfigure if headers change
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HEADERS})

    # Generate .i at CONFIGURE TIME
    file(WRITE "${SWIG_I}" "%module ${TARGET_NAME}\n\n%{\n")
    foreach(h IN LISTS HEADERS)
        file(APPEND "${SWIG_I}" "#include \"${h}\"\n")
    endforeach()
    file(APPEND "${SWIG_I}" "%}\n\n")

    if(ARG_STL_INCLUDES)
        foreach(stl IN LISTS ARG_STL_INCLUDES)
            file(APPEND "${SWIG_I}" "%include \"${stl}\"\n")
        endforeach()
    endif()

    foreach(h IN LISTS HEADERS)
        file(APPEND "${SWIG_I}" "%include \"${h}\"\n")
    endforeach()

    if(ARG_STL_TEMPLATES)
        foreach(temp IN LISTS ARG_STL_TEMPLATES)
            set(temp_name "${temp}")
            string(REPLACE " " "_" temp_name "${temp_name}")
            string(REPLACE "<" "_" temp_name "${temp_name}")
            string(REPLACE ">" "_" temp_name "${temp_name}")
            string(REPLACE "," "_" temp_name "${temp_name}")
            string(REPLACE "::" "_" temp_name "${temp_name}")
            file(APPEND "${SWIG_I}" "%template(${temp_name}) ${temp};\n")
        endforeach()
    endif()

    # SWIG target
    swig_add_library(${TARGET_NAME}_swig
        TYPE MODULE
        LANGUAGE javascript
        SOURCES ${SWIG_I}
    )

    # Force C++ compilation
    get_target_property(SWIG_SRCS ${TARGET_NAME}_swig SOURCES)
    set_source_files_properties(${SWIG_SRCS} PROPERTIES LANGUAGE CXX)

    swig_link_libraries(${TARGET_NAME}_swig ${TARGET_NAME})

    set_property(TARGET ${TARGET_NAME}_swig PROPERTY
        SWIG_COMPILE_OPTIONS
            -c++
            -javascript
            -napi
            -DSWIG
    )

    # Force SWIG wrapper to compile as C++
get_target_property(SWIG_SRCS ${TARGET_NAME}_swig SOURCES)
set_source_files_properties(${SWIG_SRCS} PROPERTIES LANGUAGE CXX)

# Ensure C++20
target_compile_features(${TARGET_NAME}_swig PUBLIC cxx_std_20)
set_target_properties(${TARGET_NAME}_swig PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
    target_compile_definitions(${TARGET_NAME}_swig PRIVATE SWIG)
    target_include_directories(${TARGET_NAME}_swig PRIVATE ${TARGET_INCLUDES} ${NODE_INCLUDE_DIR}
    ${NODE_ADDON_API_INCLUDE})

    # Ensure target always rebuilds
    set_target_properties(${TARGET_NAME}_swig PROPERTIES
        PREFIX ""
        SUFFIX ".node"
        OUTPUT_NAME "${TARGET_NAME}"
        LIBRARY_OUTPUT_DIRECTORY ${NATIVE_JS_OUT}
        RUNTIME_OUTPUT_DIRECTORY ${NATIVE_JS_OUT}
        # <-- This line makes it always recompile
        ADDITIONAL_CLEAN_FILES "${SWIG_I}"
    )

    # Mark SWIG I file as GENERATED (optional but helps)
    set_source_files_properties(${SWIG_I} PROPERTIES GENERATED TRUE)
    get_target_property(SWIG_SRCS ${TARGET_NAME}_swig SOURCES)

set_source_files_properties(${SWIG_SRCS} PROPERTIES
    LANGUAGE CXX
    COMPILE_FLAGS "-std=c++20"
)
endfunction()
