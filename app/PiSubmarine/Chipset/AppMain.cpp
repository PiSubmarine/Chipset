#include "PiSubmarine/Chipset/AppMain.h"
#include <memory>
#include "i2c.h"
#include "PiSubmarine/I2C/Stm32/Driver.h"
#include "main.h"
#include "Tasks/Power.h"

extern "C" {

void Main()
{
}

void StartPowerTask()
{
    auto ChargerDriver = std::make_unique<PiSubmarine::I2C::Stm32::Driver>(&hi2c3);
    PiSubmarine::Chipset::Tasks::Power powerTask(*ChargerDriver);
    powerTask.Run();
}

int IsRtcCorrect()
{
    return false;
}
}
