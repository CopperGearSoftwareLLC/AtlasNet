cmake_minimum_required(VERSION 3.16)

find_package(JNI REQUIRED)
find_package(Java REQUIRED)

# ----------------------------
# JNI native library (this is THE only target)
# ----------------------------
file(GLOB_RECURSE ATLASNETSERVER_JNI_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(AtlasNetServer_Java SHARED ${ATLASNETSERVER_JNI_SOURCES})

target_link_libraries(AtlasNetServer_Java
  PRIVATE
    AtlasNetServer_Shared
    ${JNI_LIBRARIES}
)

target_include_directories(AtlasNetServer_Java
  PRIVATE
    ${JNI_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set_target_properties(AtlasNetServer_Java PROPERTIES
  OUTPUT_NAME "atlasnetserver_java"
  POSITION_INDEPENDENT_CODE ON
)

# Make libAtlasNetServer_Shared discoverable when next to JNI library
if(APPLE)
  set_target_properties(AtlasNetServer_Java PROPERTIES
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
  )
elseif(UNIX)
  set_target_properties(AtlasNetServer_Java PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
  )
endif()

# ----------------------------
# Java sources (API jar)
# ----------------------------
file(GLOB_RECURSE ATLASNETSERVER_JAVA_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/java/*.java"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/java/**/*.java"
)

set(JAVA_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/java_classes")
set(JAVA_JAR         "${CMAKE_CURRENT_BINARY_DIR}/atlasnetserver-java.jar")

# ----------------------------
# Dist folder (what users consume)
# ----------------------------
if(WIN32)
  set(PLATFORM_DIR "windows-x86_64")
elseif(APPLE)
  set(PLATFORM_DIR "macos-x86_64")
else()
  set(PLATFORM_DIR "linux-x86_64")
endif()

set(DIST_DIR         "${CMAKE_BINARY_DIR}/dist/atlasnetserver-java")
set(DIST_NATIVES_DIR "${DIST_DIR}/natives/${PLATFORM_DIR}")

# ----------------------------
# POST_BUILD: build jar + stage dist
# (No extra CMake targets; only AtlasNetServer_Java exists)
# ----------------------------
add_custom_command(TARGET AtlasNetServer_Java POST_BUILD
  BYPRODUCTS
    "${JAVA_JAR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${JAVA_CLASSES_DIR}"
  COMMAND "${Java_JAVAC_EXECUTABLE}" -encoding UTF-8 -d "${JAVA_CLASSES_DIR}" ${ATLASNETSERVER_JAVA_SOURCES}
  COMMAND "${Java_JAR_EXECUTABLE}" cf "${JAVA_JAR}" -C "${JAVA_CLASSES_DIR}" .

  COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_NATIVES_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${JAVA_JAR}"
          "${DIST_DIR}/atlasnetserver-java.jar"

  # Copy JNI shim
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "$<TARGET_FILE:AtlasNetServer_Java>"
          "${DIST_NATIVES_DIR}/$<TARGET_FILE_NAME:AtlasNetServer_Java>"

  # Copy core server shared lib
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "$<TARGET_FILE:AtlasNetServer_Shared>"
          "${DIST_NATIVES_DIR}/$<TARGET_FILE_NAME:AtlasNetServer_Shared>"

  VERBATIM
)

# Optional: expose dist info to other subprojects (nice for CMake2 staging)
set_target_properties(AtlasNetServer_Java PROPERTIES
  ATLASNETSERVER_JAVA_DIST_DIR         "${DIST_DIR}"
  ATLASNETSERVER_JAVA_DIST_JAR         "${DIST_DIR}/atlasnetserver-java.jar"
  ATLASNETSERVER_JAVA_DIST_NATIVES_DIR "${DIST_DIR}/natives/${PLATFORM_DIR}"
)
