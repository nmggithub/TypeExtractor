cmake_minimum_required(VERSION 3.31.0)
project(typeextractor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
        execute_process(
                COMMAND brew --prefix llvm
                OUTPUT_VARIABLE LLVM_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE BREW_RESULT
        )
        if(NOT BREW_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to find LLVM via brew")
        endif()
        set(CMAKE_MODULE_DIR "${LLVM_PREFIX}/lib/cmake")
elseif(UNIX AND NOT APPLE)
        execute_process(
                COMMAND llvm-config --libdir
                OUTPUT_VARIABLE LLVM_LIBRARY_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE LLVM_CONFIG_RESULT
        )
        if(NOT LLVM_CONFIG_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to find LLVM via llvm-config")
        endif()
        set(CMAKE_MODULE_DIR "${LLVM_LIBRARY_DIR}/cmake")
elseif(WIN32)
        message(FATAL_ERROR "Windows is not supported yet.")
else()
        message(FATAL_ERROR "Unsupported platform.")
endif()

if(NOT CMAKE_MODULE_DIR)
        message(FATAL_ERROR "Failed to find the CMake module directory.")
endif()

message(STATUS "CMake module directory: ${CMAKE_MODULE_DIR}")

# https://github.com/llvm/llvm-project/blob/llvmorg-20.1.8/llvm/docs/CMake.rst#embedding-llvm-in-your-project
set(LLVM_DIR "${CMAKE_MODULE_DIR}/llvm")
find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})

# This isn't officially documented, but Clang's CMake files are similarly located to LLVM's.
set(Clang_DIR "${CMAKE_MODULE_DIR}/clang")
find_package(Clang REQUIRED CONFIG)

# === Shared Library ===

file(GLOB_RECURSE SHARED_LIB_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/shared/*.h
        ${CMAKE_CURRENT_LIST_DIR}/src/shared/*.cpp
)
add_library(shared STATIC ${SHARED_LIB_SOURCES})

target_link_directories(shared PRIVATE ${LLVM_LIBRARY_DIRS})
target_include_directories(shared PRIVATE ${LLVM_INCLUDE_DIRS})

target_link_libraries(shared PRIVATE
        clang-cpp
        LLVM
)

target_include_directories(shared
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/shared/include>
)

set_target_properties(shared PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(shared PROPERTIES OUTPUT_NAME action)


# === Clang Plugin ===

file(GLOB_RECURSE PLUGIN_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/clang-plugin/*.h
        ${CMAKE_CURRENT_LIST_DIR}/src/clang-plugin/*.cpp
)
add_library(clang-plugin MODULE ${PLUGIN_SOURCES})

target_link_libraries(clang-plugin PRIVATE
        shared
)

set_target_properties(clang-plugin PROPERTIES OUTPUT_NAME te)

# === Executable ===

file(GLOB_RECURSE BINARY_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/executable/*.h
        ${CMAKE_CURRENT_LIST_DIR}/src/executable/*.cpp
)

add_executable(executable ${BINARY_SOURCES})

target_link_libraries(executable PRIVATE
        shared
)
set_target_properties(executable PROPERTIES OUTPUT_NAME te)
