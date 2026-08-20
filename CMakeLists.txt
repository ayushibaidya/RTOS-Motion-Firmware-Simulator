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

option(USE_FREERTOS "Build with real FreeRTOS backend" OFF)

# Main project directories.
set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/firmware)
set(APP_DIR ${FIRMWARE_DIR}/App)
set(CORE_DIR ${FIRMWARE_DIR}/Core)
set(DRIVERS_DIR ${FIRMWARE_DIR}/Drivers)

set(RTOS_PORT_SOURCES
    ${APP_DIR}/rtos/rtos_port_host.c
)
set(RTOS_INCLUDE_DIRS
    ${APP_DIR}/rtos
)
set(RTOS_COMPILE_DEFINITIONS)
set(RTOS_LINK_LIBRARIES)

if(USE_FREERTOS)
    set(FREERTOS_KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS-Kernel)

    set(RTOS_PORT_SOURCES
        ${APP_DIR}/rtos/rtos_port_freertos.c
        ${FREERTOS_KERNEL_DIR}/tasks.c
        ${FREERTOS_KERNEL_DIR}/queue.c
        ${FREERTOS_KERNEL_DIR}/list.c
        ${FREERTOS_KERNEL_DIR}/event_groups.c
        ${FREERTOS_KERNEL_DIR}/portable/ThirdParty/GCC/Posix/port.c
        ${FREERTOS_KERNEL_DIR}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
    )
    list(APPEND RTOS_INCLUDE_DIRS
        ${FIRMWARE_DIR}/Config
        ${FREERTOS_KERNEL_DIR}/include
        ${FREERTOS_KERNEL_DIR}/portable/ThirdParty/GCC/Posix
        ${FREERTOS_KERNEL_DIR}/portable/ThirdParty/GCC/Posix/utils
    )
    list(APPEND RTOS_COMPILE_DEFINITIONS USE_FREERTOS=1)

    find_package(Threads REQUIRED)
    list(APPEND RTOS_LINK_LIBRARIES Threads::Threads)
endif()

function(configure_rtos_target target_name)
    target_include_directories(${target_name} PRIVATE
        ${RTOS_INCLUDE_DIRS}
    )

    if(RTOS_COMPILE_DEFINITIONS)
        target_compile_definitions(${target_name} PRIVATE
            ${RTOS_COMPILE_DEFINITIONS}
        )
    endif()

    if(RTOS_LINK_LIBRARIES)
        target_link_libraries(${target_name} PRIVATE
            ${RTOS_LINK_LIBRARIES}
        )
    endif()
endfunction()

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
        ${APP_DIR}/tasks/app_tasks.c
        ${RTOS_PORT_SOURCES}
        ${APP_DIR}/command/command_parser.c
        ${APP_DIR}/fault/fault_manager.c
        ${APP_DIR}/motion/motion_controller.c
        ${APP_DIR}/telemetry/telemetry.c
    )
    target_include_directories(mr_robo_demo PRIVATE
        ${APP_DIR}/tasks
        ${APP_DIR}/command
        ${APP_DIR}/fault
        ${APP_DIR}/motion
        ${APP_DIR}/telemetry
    )
    configure_rtos_target(mr_robo_demo)

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

    add_executable(test_pid_controller
        tests/unit/test_pid_controller.c
        ${APP_DIR}/pid/pid_controller.c
    )
    target_include_directories(test_pid_controller PRIVATE
        ${APP_DIR}/pid
    )
    add_test(NAME pid_controller COMMAND test_pid_controller)

    add_executable(test_rtos_port
        tests/unit/test_rtos_port.c
        ${RTOS_PORT_SOURCES}
    )
    configure_rtos_target(test_rtos_port)
    add_test(NAME rtos_port COMMAND test_rtos_port)

    add_executable(test_app_tasks
        tests/unit/test_app_tasks.c
        ${APP_DIR}/tasks/app_tasks.c
        ${RTOS_PORT_SOURCES}
        ${APP_DIR}/command/command_parser.c
        ${APP_DIR}/fault/fault_manager.c
        ${APP_DIR}/motion/motion_controller.c
        ${APP_DIR}/telemetry/telemetry.c
    )
    target_include_directories(test_app_tasks PRIVATE
        ${APP_DIR}/tasks
        ${APP_DIR}/command
        ${APP_DIR}/fault
        ${APP_DIR}/motion
        ${APP_DIR}/telemetry
    )
    configure_rtos_target(test_app_tasks)
    add_test(NAME app_tasks COMMAND test_app_tasks)

    if(USE_FREERTOS)
        add_executable(motion_freertos_scheduler_demo
            ${CORE_DIR}/main_freertos_scheduler.c
            ${APP_DIR}/tasks/app_scheduler.c
            ${APP_DIR}/tasks/app_tasks.c
            ${RTOS_PORT_SOURCES}
            ${APP_DIR}/command/command_parser.c
            ${APP_DIR}/fault/fault_manager.c
            ${APP_DIR}/motion/motion_controller.c
            ${APP_DIR}/telemetry/telemetry.c
        )
        target_include_directories(motion_freertos_scheduler_demo PRIVATE
            ${APP_DIR}/tasks
            ${APP_DIR}/command
            ${APP_DIR}/fault
            ${APP_DIR}/motion
            ${APP_DIR}/telemetry
        )
        configure_rtos_target(motion_freertos_scheduler_demo)

        add_test(NAME freertos_scheduler_demo COMMAND motion_freertos_scheduler_demo)
        set_tests_properties(freertos_scheduler_demo PROPERTIES TIMEOUT 10)
    endif()
endif()
