# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

cmake_minimum_required(VERSION 3.24)
project(VoelundReferenceContract LANGUAGES CXX)
enable_testing()
add_executable(reference_contract ContractProof.cpp)
target_compile_features(reference_contract PRIVATE cxx_std_20)
target_include_directories(reference_contract PRIVATE ../../include)
if(MSVC)
    target_compile_options(reference_contract PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(reference_contract PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME contract.reference_fixtures COMMAND reference_contract)
