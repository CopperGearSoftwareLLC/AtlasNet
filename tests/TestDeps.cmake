include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

message(STATUS "Fetching GLM")
SET(GLM_BUILD_TESTS OFF)
FetchContent_Declare(
  gtest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(gtest)
