# Writes REACH_BUILD_COMMIT (short git commit, "+dirty" when tracked files have uncommitted
# changes) to OUTPUT. Runs on every build and rewrites the file only when the value changes, so
# unchanged builds stay incremental. Usage: cmake -DSOURCE_DIR=<repo> -DOUTPUT=<header> -P <this>

execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0 OR commit STREQUAL "")
    set(commit "unknown")
else()
    execute_process(
        COMMAND git status --porcelain --untracked-files=no
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE changes
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT changes STREQUAL "")
        string(APPEND commit "+dirty")
    endif()
endif()

set(content "#ifndef REACH_PROTOCOL_BUILD_INFO_H\n#define REACH_PROTOCOL_BUILD_INFO_H\n\n#define REACH_BUILD_COMMIT \"${commit}\"\n\n#endif\n")
set(existing "")
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" existing)
endif()
if(NOT existing STREQUAL content)
    file(WRITE "${OUTPUT}" "${content}")
endif()
