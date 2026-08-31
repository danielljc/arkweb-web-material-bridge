cmake_minimum_required(VERSION 3.16)

project(arkweb_web_material_bridge
    VERSION 0.1.0
    DESCRIPTION "Experimental semantic material bridge for ArkWeb rendering"
    LANGUAGES CXX)

option(WEB_MATERIAL_BUILD_TESTS "Build unit tests" ON)

add_library(web_material
    src/types.cpp
    src/descriptor.cpp
    src/codec.cpp
    src/policy.cpp)

target_include_directories(web_material
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(web_material PUBLIC cxx_std_17)
set_target_properties(web_material PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(web_material PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow)
elseif(MSVC)
    target_compile_options(web_material PRIVATE /W4 /permissive-)
endif()

if(WEB_MATERIAL_BUILD_TESTS)
    enable_testing()
    add_executable(web_material_tests
        tests/test_main.cpp
        tests/types_test.cpp
        tests/codec_test.cpp
        tests/policy_test.cpp)
    target_link_libraries(web_material_tests PRIVATE web_material)
    target_include_directories(web_material_tests PRIVATE tests)
    target_compile_features(web_material_tests PRIVATE cxx_std_17)
    add_test(NAME web_material_tests COMMAND web_material_tests)
endif()
