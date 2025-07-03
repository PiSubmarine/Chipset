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

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM CXX)

# Core project settings
project("PiSubmarine.Chipset")
set(_build_cmake_url "https://raw.githubusercontent.com/PiSubmarine/Build.CMake/main/PiSubmarine.Build.CMake.cmake")
set(_build_cmake_local "${CMAKE_SOURCE_DIR}/PiSubmarine.Build.CMake.cmake")
if(NOT EXISTS "${_build_cmake_local}")
    message(STATUS "Downloading ${_build_cmake_url}...")
    file(DOWNLOAD "${_build_cmake_url}" "${_build_cmake_local}"
        INACTIVITY_TIMEOUT 60
        STATUS _status
        LOG _log
    )

    list(GET _status 0 _status_code)
    if(NOT _status_code EQUAL 0)
        message(FATAL_ERROR "Failed to download ${_build_cmake_url}: ${_log}")
    endif()
else()
    message(STATUS "Using cached file: ${_build_cmake_local}")
endif()
include("${_build_cmake_local}")
PiSubmarineInitModule()


message("Build type: " ${CMAKE_BUILD_TYPE})

add_executable(${CMAKE_PROJECT_NAME})

PiSubmarineAddDependency("https://github.com/PiSubmarine/Bq25792" "")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "PiSubmarine.Bq25792")
PiSubmarineAddDependency("https://github.com/PiSubmarine/Max1726" "")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "PiSubmarine.Max1726")
PiSubmarineAddDependency("https://github.com/PiSubmarine/Chipset.Api" "")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "PiSubmarine.Chipset.Api")

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    "Core/App/PiSubmarine/Chipset/AppMain.cpp"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
   "Core/App"
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
    stm32cubemx

    # Add user defined libraries
)

