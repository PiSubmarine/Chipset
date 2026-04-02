cmake_minimum_required (VERSION 3.25)

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()


# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(FetchContent)

FetchContent_Declare(
        PiSubmarineBuild
        GIT_REPOSITORY "https://github.com/PiSubmarine/Build.CMake.git"
        GIT_TAG        "main"
)

FetchContent_MakeAvailable(PiSubmarineBuild)

set(PISUBMARINE_BUILD_SYSTEM "${pisubmarinebuild_SOURCE_DIR}/PiSubmarine.Build.CMake.cmake")

# For legacy modules
set(_build_cmake_local "${pisubmarinebuild_SOURCE_DIR}/PiSubmarine.Build.CMake.cmake")

include("${pisubmarinebuild_SOURCE_DIR}/PiSubmarine.Build.CMake.cmake")

PiSubmarineInitModule("")
project(${PISUBMARINE_MODULE_NAME} C CXX ASM)
PiSubmarineConfigureModule()

add_subdirectory(cmake/stm32cubemx)

