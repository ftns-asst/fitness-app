if (NOT DEFINED APP_EXECUTABLE OR NOT EXISTS "${APP_EXECUTABLE}")
    message(FATAL_ERROR "UI smoke executable was not found: ${APP_EXECUTABLE}")
endif()

set(ENV{QT_QPA_PLATFORM} "offscreen")
set(ENV{QT_FORCE_STDERR_LOGGING} "1")

if (WIN32)
    set(ENV{PATH} "${QT_RUNTIME_DIR};$ENV{PATH}")
endif()

set(app_arguments --diag --width 390 --height 812)
if (DEFINED SCREEN)
    list(APPEND app_arguments --screen "${SCREEN}")
    if (VIEWPORT STREQUAL "360")
        set(app_arguments --diag --screen "${SCREEN}" --width 360 --height 780)
    elseif(VIEWPORT STREQUAL "480")
        set(app_arguments --diag --screen "${SCREEN}" --width 480 --height 900)
    elseif(VIEWPORT STREQUAL "frame")
        set(app_arguments --diag --screen "${SCREEN}" --frame)
    elseif(VIEWPORT STREQUAL "dark")
        list(APPEND app_arguments --dark)
    endif()
elseif (DEFINED OPEN_AUTH)
    # Гостевой сценарий «Профиль → Войти»: без --screen, без сессии.
    if (VIEWPORT STREQUAL "360")
        set(app_arguments --diag --open-auth --width 360 --height 780)
    elseif(VIEWPORT STREQUAL "480")
        set(app_arguments --diag --open-auth --width 480 --height 900)
    elseif(VIEWPORT STREQUAL "frame")
        set(app_arguments --diag --open-auth --frame)
    elseif(VIEWPORT STREQUAL "dark")
        set(app_arguments --diag --open-auth --dark)
    else()
        set(app_arguments --diag --open-auth)
    endif()
else()
    list(APPEND app_arguments --tab "${TAB_INDEX}" --authenticated)
endif()

if (UNAUTH)
    list(APPEND app_arguments --unauth)
endif()

if (OPEN_AUTH)
    list(APPEND app_arguments --open-auth)
endif()

set(ENV{AUTH_ORIGIN} "${AUTH_ORIGIN}")

execute_process(
    COMMAND "${APP_EXECUTABLE}" ${app_arguments}
    RESULT_VARIABLE app_result
    OUTPUT_VARIABLE app_stdout
    ERROR_VARIABLE app_stderr
    TIMEOUT 15
)

message("${app_stdout}${app_stderr}")

if (NOT app_result EQUAL 0)
    message(FATAL_ERROR "UI smoke failed with exit code ${app_result}")
endif()