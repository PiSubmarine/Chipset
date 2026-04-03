#include "PiSubmarine/Chipset/AppMain.h"
#include <memory>
#include "i2c.h"
#include "PiSubmarine/I2C/Stm32/Driver.h"
#include "main.h"
#include "Tasks/Power.h"
#include "app_freertos.h"
#include "Tasks/Adc.h"

namespace
{
    PiSubmarine::Chipset::SharedState SharedState(SharedStateMutexHandle);
}

extern "C" {
void Main()
{
}

void StartPowerTask(void* argument)
{
    (void)argument;

    auto ChargerDriver = std::make_unique<PiSubmarine::I2C::Stm32::Driver>(&hi2c3);
    PiSubmarine::Chipset::Tasks::Power powerTask(SharedState, *ChargerDriver);
    powerTask.Run();
}

void StartAdcTask(void* argument)
{
    (void)argument;
    PiSubmarine::Chipset::Tasks::Adc adcTask(SharedState);
    adcTask.Run();
}

int IsRtcCorrect()
{
    return false;
}
}
