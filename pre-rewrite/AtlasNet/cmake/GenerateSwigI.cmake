# GenerateSwigI.cmake

set(CMAKE_LIST_SEPARATOR "|")

file(WRITE "${OUT}"
"%module ${MODULE}\n\n%{\n"
)

foreach(h IN LISTS HEADERS)
    file(APPEND "${OUT}" "#include \"${h}\"\n")
endforeach()

file(APPEND "${OUT}" "%}\n\n")

foreach(h IN LISTS HEADERS)
    file(APPEND "${OUT}" "%include \"${h}\"\n")
endforeach()
