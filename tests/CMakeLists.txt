# EH3D-NODE-BUNGARD host tests: the control link's rules, the node's settings and what every node shares (retries, Improv Wi-Fi, users, sessions,
# network plan, JSON), built with any C++17 compiler: cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host -C Debug
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
cmake_minimum_required(VERSION 3.16)
project(eh3d_node_bungard_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W4 /WX /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(core_tests core_tests.cpp)
add_executable(test_base test_base.cpp)
add_executable(test_clock test_clock.cpp)
add_executable(test_settings test_settings.cpp)
add_executable(test_machine test_machine.cpp)
add_executable(test_release test_release.cpp)
# the rules shared with the other nodes; the _eth build is the s3-eth board profile
add_executable(test_shared test_shared.cpp)
add_executable(test_shared_eth test_shared.cpp)
target_compile_definitions(test_shared_eth PRIVATE EH3D_BOARD_S3_ETH=1)

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
add_test(NAME base COMMAND test_base)
add_test(NAME clock COMMAND test_clock)
add_test(NAME settings COMMAND test_settings)
add_test(NAME machine COMMAND test_machine)
add_test(NAME release COMMAND test_release)
add_test(NAME shared COMMAND test_shared)
add_test(NAME shared_eth COMMAND test_shared_eth)

# the panel's texts: eight languages in every row and a text for every error the firmware can answer with
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME panel_text COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/check_panel_text.py)
  # no task keeps a big copy of the settings on its small stack and every task has the stack its job needs
  add_test(NAME task_safety COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/check_task_safety.py)
endif()
