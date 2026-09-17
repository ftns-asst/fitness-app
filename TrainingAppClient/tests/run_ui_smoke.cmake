if (NOT DEFINED APP_EXECUTABLE OR NOT EXISTS "${APP_EXECUTABLE}")
    message(FATAL_ERROR "UI smoke executable was not found: ${APP_EXECUTABLE}")
endif()

set(ENV{QT_QPA_PLATFORM} "offscreen")
set(ENV{QT_FORCE_STDERR_LOGGING} "1")

if (WIN32)
    set(ENV{PATH} "${QT_RUNTIME_DIR};$ENV{PATH}")
endif()

execute_process(
    COMMAND "${APP_EXECUTABLE}"
        --diag
        --width 390
        --height 812
        --tab "${TAB_INDEX}"
    RESULT_VARIABLE app_result
    OUTPUT_VARIABLE app_stdout
    ERROR_VARIABLE app_stderr
    TIMEOUT 15
)

message("${app_stdout}${app_stderr}")

if (NOT app_result EQUAL 0)
    message(FATAL_ERROR "UI smoke failed with exit code ${app_result}")
endif()