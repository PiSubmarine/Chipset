cmake_minimum_required(VERSION 3.25)

add_executable(${PISUBMARINE_MODULE_NAME}
        PiSubmarine/Chipset/RegisterStorage.cpp
        PiSubmarine/Chipset/Tasks/Power.cpp
        PiSubmarine/Chipset/Tasks/Task.cpp
        PiSubmarine/I2C/Stm32/Driver.cpp
        PiSubmarine/Chipset/AppMain.cpp)
target_include_directories(${PISUBMARINE_MODULE_NAME} PUBLIC "./")
PiSubmarineInitTarget(${PISUBMARINE_MODULE_NAME})

target_link_libraries(${PISUBMARINE_MODULE_NAME} PUBLIC stm32cubemx)

PiSubmarineAddDependency("https://github.com/PiSubmarine/Bq25792" "feature/rtos")
target_link_libraries(${PISUBMARINE_MODULE_NAME} PRIVATE "PiSubmarine.Bq25792")
PiSubmarineAddDependency("https://github.com/PiSubmarine/Chipset.Api" "")
target_link_libraries(${PISUBMARINE_MODULE_NAME} PRIVATE "PiSubmarine.Chipset.Api")