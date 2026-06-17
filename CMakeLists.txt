cmake_minimum_required(VERSION 3.22)

project(rtos_motion_firmware_simulator
    VERSION 0.1.0
    DESCRIPTION "RTOS-based motion-control firmware simulator for an emulated ARM Cortex-M target"
    LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Main project directories.
set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/firmware)
set(APP_DIR ${FIRMWARE_DIR}/App)
set(CORE_DIR ${FIRMWARE_DIR}/Core)
set(DRIVERS_DIR ${FIRMWARE_DIR}/Drivers)

# Firmware sources will be added here once implementation begins.
# Example future layout:
# set(FIRMWARE_SOURCES
#     ${CORE_DIR}/main.c
#     ${APP_DIR}/command/command_parser.c
#     ${APP_DIR}/motion/motion_controller.c
#     ${APP_DIR}/telemetry/telemetry.c
#     ${APP_DIR}/fault/fault_manager.c
#     ${APP_DIR}/pid/pid_controller.c
# )

# Placeholder target so CMake can configure before firmware code exists.
add_custom_target(project_info
    COMMAND ${CMAKE_COMMAND} -E echo "RTOS Motion Firmware Simulator"
    COMMAND ${CMAKE_COMMAND} -E echo "Add firmware sources after ARM toolchain and FreeRTOS setup."
)

if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()

    add_executable(mr_robo_demo
        ${CORE_DIR}/main.c
        ${APP_DIR}/command/command_parser.c
        ${APP_DIR}/fault/fault_manager.c
        ${APP_DIR}/motion/motion_controller.c
        ${APP_DIR}/telemetry/telemetry.c
    )
    target_include_directories(mr_robo_demo PRIVATE
        ${APP_DIR}/command
        ${APP_DIR}/fault
        ${APP_DIR}/motion
        ${APP_DIR}/telemetry
    )

    add_executable(test_telemetry
        tests/unit/test_telemetry.c
        ${APP_DIR}/telemetry/telemetry.c
    )
    target_include_directories(test_telemetry PRIVATE
        ${APP_DIR}/telemetry
    )
    add_test(NAME telemetry COMMAND test_telemetry)

    add_executable(test_motion_controller
        tests/unit/test_motion_controller.c
        ${APP_DIR}/motion/motion_controller.c
    )
    target_include_directories(test_motion_controller PRIVATE
        ${APP_DIR}/motion
    )
    add_test(NAME motion_controller COMMAND test_motion_controller)

    add_executable(test_command_parser
        tests/unit/test_command_parser.c
        ${APP_DIR}/command/command_parser.c
    )
    target_include_directories(test_command_parser PRIVATE
        ${APP_DIR}/command
    )
    add_test(NAME command_parser COMMAND test_command_parser)

    add_executable(test_fault_manager
        tests/unit/test_fault_manager.c
        ${APP_DIR}/fault/fault_manager.c
    )
    target_include_directories(test_fault_manager PRIVATE
        ${APP_DIR}/fault
    )
    add_test(NAME fault_manager COMMAND test_fault_manager)
endif()
