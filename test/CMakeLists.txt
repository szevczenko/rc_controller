cmake_minimum_required(VERSION 3.16)
project(rc_controller_tests C)

set(CMAKE_C_STANDARD 11)

# Unity test framework
add_library(unity STATIC
    unity/src/unity.c
)
target_include_directories(unity PUBLIC unity/src)

# Host stubs (replace ESP-IDF headers for native build)
set(STUB_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/stubs)

# rc_protocol component (depends on rc_core for rc_channel_state_t)
add_library(rc_protocol STATIC
    ../components/rc_protocol/src/rc_protocol.c
)
target_include_directories(rc_protocol PUBLIC
    ../components/rc_protocol/include
    ../components/rc_core/include
    ${STUB_INCLUDE}
)

# rc_core component
add_library(rc_core STATIC
    ../components/rc_core/src/rc_core.c
)
target_include_directories(rc_core PUBLIC
    ../components/rc_core/include
    ${STUB_INCLUDE}
)

# radio component (loopback only — no ESP-NOW in host tests)
add_library(radio STATIC
    ../components/radio/src/radio.c
    ../components/radio/src/radio_loopback.c
)
target_include_directories(radio PUBLIC
    ../components/radio/include
    ${STUB_INCLUDE}
)

# ── Test executables ──────────────────────────────────────────────────────────

add_executable(test_rc_protocol test_rc_protocol.c)
target_link_libraries(test_rc_protocol rc_protocol rc_core unity)
add_test(NAME rc_protocol COMMAND test_rc_protocol)

add_executable(test_rc_core test_rc_core.c)
target_link_libraries(test_rc_core rc_core unity)
add_test(NAME rc_core COMMAND test_rc_core)

add_executable(test_radio test_radio.c)
target_link_libraries(test_radio radio rc_protocol rc_core unity)
add_test(NAME radio COMMAND test_radio)

enable_testing()
