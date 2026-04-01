include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

set(BUILD_REDIS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  drogon
  GIT_REPOSITORY https://github.com/drogonframework/drogon.git
  GIT_TAG v1.9.12
  GIT_PROGRESS TRUE
  )
  FetchContent_MakeAvailable(drogon)
