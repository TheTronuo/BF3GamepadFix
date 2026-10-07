cmake_minimum_required(VERSION 3.24)
project(BF3GamepadFix VERSION 0.22.3 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "This release requires Windows and MSVC (x86 ABI and SEH).")
endif()

option(BF3_BUILD_RUNTIME "Build the 32-bit game DLL" ON)
option(BF3_BUILD_TOOLS "Build the C++ resource generator" ON)
option(BF3_BUILD_INSTALLER "Build the C++ install/restore utility" ON)
set(BF3_RELEASE_DLL_SHA256 "a2c97811f0548bab3d0060be9bf23b901c68a05b04e29f3bcc4e92c160da7d13"
    CACHE STRING "SHA256 of the tested distribution runtime DLL")
set(BF3_MINHOOK_SOURCE_DIR "" CACHE PATH "External MinHook source directory")
set(BF3_REFERENCE_STAGE "" CACHE PATH "Optional original/patched 0.22.2 fixture directory")
set(BF3_ORIGINAL_DLL "" CACHE FILEPATH "Optional original buildinfo DLL for isolated proxy tests")
set(BF3_PROMPT_FIXTURE_DIR "" CACHE PATH "Optional private decoded Xbox prompt movies for hook tests")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(CTest)

function(bf3_cpp_target target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/source/include")
    target_compile_definitions(${target} PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
endfunction()

add_library(bf3_prompt_presentation STATIC
    source/src/crypto/hash.cpp source/src/patch/prompt_widgets.cpp source/src/runtime/prompt_movie.cpp)
bf3_cpp_target(bf3_prompt_presentation)
target_link_libraries(bf3_prompt_presentation PUBLIC bcrypt)

if(BF3_BUILD_TOOLS)
    find_package(ZLIB 1.3.1 EXACT REQUIRED)
    add_library(bf3_patch STATIC
        source/src/patch/cas_blocks.cpp
        source/src/patch/resource_builder.cpp
        source/src/patch/release_0222.cpp)
    bf3_cpp_target(bf3_patch)
    target_link_libraries(bf3_patch PUBLIC ZLIB::ZLIB bf3_prompt_presentation)
    add_executable(bf3-resource-builder source/tools/resource_builder_main.cpp)
    bf3_cpp_target(bf3-resource-builder)
    target_link_libraries(bf3-resource-builder PRIVATE bf3_patch)
    if(BUILD_TESTING)
        add_executable(bf3-patch-tests source/tests/patch_tests.cpp)
        bf3_cpp_target(bf3-patch-tests)
        target_link_libraries(bf3-patch-tests PRIVATE bf3_patch)
        add_test(NAME patch-unit COMMAND bf3-patch-tests "${PROJECT_SOURCE_DIR}/source/src/runtime/hash_rules_data.inc")
        if(BF3_REFERENCE_STAGE)
            add_test(NAME release-parity COMMAND bf3-patch-tests
                "${PROJECT_SOURCE_DIR}/source/src/runtime/hash_rules_data.inc" "${BF3_REFERENCE_STAGE}")
        endif()
    endif()
endif()

if(BF3_BUILD_INSTALLER)
    add_library(bf3_install STATIC source/src/install/transaction.cpp source/src/crypto/hash.cpp
        source/src/patch/release_0222.cpp)
    bf3_cpp_target(bf3_install)
    target_link_libraries(bf3_install PUBLIC bcrypt)
    add_executable(BF3GamepadFix source/tools/install_main.cpp)
    bf3_cpp_target(BF3GamepadFix)
    target_compile_definitions(BF3GamepadFix PRIVATE
        BF3_ORIGINAL_DLL_SHA256="a1003d8e616b6ce14d510411710a5c0c9c15ef73d3ad969989c4339e1bcdfc1c"
        BF3_RELEASE_DLL_SHA256="${BF3_RELEASE_DLL_SHA256}")
    target_link_libraries(BF3GamepadFix PRIVATE bf3_install)
    if(BUILD_TESTING)
        add_executable(bf3-install-tests source/tests/install_tests.cpp)
        bf3_cpp_target(bf3-install-tests)
        target_link_libraries(bf3-install-tests PRIVATE bf3_install)
        add_test(NAME installer-transaction COMMAND bf3-install-tests)
        add_executable(bf3-install-cli-fixture source/tools/install_main.cpp source/tests/install_fixture.cpp)
        bf3_cpp_target(bf3-install-cli-fixture)
        target_compile_definitions(bf3-install-cli-fixture PRIVATE
            BF3_ORIGINAL_DLL_SHA256="039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81"
            BF3_RELEASE_DLL_SHA256="787c798e39a5bc1910355bae6d0cd87a36b2e10fd0202a83e3bb6b005da83472")
        target_link_libraries(bf3-install-cli-fixture PRIVATE bf3_install)
        add_executable(bf3-install-cli-tests source/tests/install_cli_tests.cpp)
        bf3_cpp_target(bf3-install-cli-tests)
        target_link_libraries(bf3-install-cli-tests PRIVATE bf3_install)
        add_test(NAME installer-cli COMMAND bf3-install-cli-tests
            "$<TARGET_FILE:bf3-install-cli-fixture>")
    endif()
endif()

if(BF3_BUILD_RUNTIME)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "The game DLL must use -A Win32. For x64 tools set BF3_BUILD_RUNTIME=OFF.")
    endif()
    if(NOT EXISTS "${BF3_MINHOOK_SOURCE_DIR}/include/MinHook.h")
        message(FATAL_ERROR "Set BF3_MINHOOK_SOURCE_DIR to an external MinHook source checkout.")
    endif()
    # MinHook's C implementation is an external dependency, not project code.
    enable_language(C)
    add_library(bf3_minhook STATIC
        "${BF3_MINHOOK_SOURCE_DIR}/src/buffer.c"
        "${BF3_MINHOOK_SOURCE_DIR}/src/hook.c"
        "${BF3_MINHOOK_SOURCE_DIR}/src/trampoline.c"
        "${BF3_MINHOOK_SOURCE_DIR}/src/hde/hde32.c")
    target_include_directories(bf3_minhook PUBLIC "${BF3_MINHOOK_SOURCE_DIR}/include")
    add_library(bf3_runtime_core STATIC
        source/src/runtime/engine.cpp source/src/runtime/hash_rules.cpp
        source/src/runtime/input_frames.cpp source/src/runtime/ui_scaling.cpp source/src/runtime/config.cpp)
    bf3_cpp_target(bf3_runtime_core)
    add_library(bf3-controller SHARED
        source/src/runtime/bootstrap.cpp source/src/runtime/hooks.cpp
        source/src/runtime/log.cpp source/src/runtime/proxy.cpp source/src/runtime/proxy.def source/src/runtime/prompt_hook.cpp)
    bf3_cpp_target(bf3-controller)
    target_link_libraries(bf3-controller PRIVATE bf3_runtime_core bf3_minhook bf3_prompt_presentation)
    set_target_properties(bf3-controller PROPERTIES OUTPUT_NAME Engine.BuildInfo_Win32_Retail_dll)
    if(BUILD_TESTING)
        add_executable(bf3-runtime-tests source/tests/runtime_tests.cpp)
        bf3_cpp_target(bf3-runtime-tests)
        target_link_libraries(bf3-runtime-tests PRIVATE bf3_runtime_core)
        add_test(NAME runtime-unit COMMAND bf3-runtime-tests)
        add_executable(bf3-prompt-hook-tests source/tests/prompt_hook_tests.cpp
            source/src/runtime/prompt_hook.cpp source/src/runtime/log.cpp)
        bf3_cpp_target(bf3-prompt-hook-tests)
        target_link_libraries(bf3-prompt-hook-tests PRIVATE bf3_runtime_core bf3_minhook bf3_prompt_presentation)
        add_test(NAME prompt-hook COMMAND bf3-prompt-hook-tests)
        if(BF3_PROMPT_FIXTURE_DIR)
            add_test(NAME prompt-hook-parity COMMAND bf3-prompt-hook-tests "${BF3_PROMPT_FIXTURE_DIR}")
        endif()
        if(BF3_ORIGINAL_DLL)
            add_executable(bf3-proxy-tests source/tests/proxy_tests.cpp)
            bf3_cpp_target(bf3-proxy-tests)
            target_link_libraries(bf3-proxy-tests PRIVATE bcrypt)
            set(proxy_fixture "${PROJECT_BINARY_DIR}/proxy-fixture")
            file(MAKE_DIRECTORY "${proxy_fixture}/ControllerMod/loader")
            configure_file("${BF3_ORIGINAL_DLL}" "${proxy_fixture}/ControllerMod/loader/original-buildinfo.dll" COPYONLY)
            add_custom_command(TARGET bf3-controller POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:bf3-controller>" "${proxy_fixture}")
            add_custom_command(TARGET bf3-proxy-tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:bf3-proxy-tests>" "${proxy_fixture}")
            add_test(NAME proxy-forwarding COMMAND "${proxy_fixture}/bf3-proxy-tests.exe")
            set_tests_properties(proxy-forwarding PROPERTIES WORKING_DIRECTORY "${proxy_fixture}")
        endif()
    endif()
endif()
