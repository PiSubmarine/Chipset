#pragma once

#include "PiSubmarine/Chipset/Tasks/Task.h"
#include "PiSubmarine/I2C/Stm32/Driver.h"
#include "PiSubmarine/Bq25792/Device.h"

namespace PiSubmarine::Chipset::Tasks
{
    class Power : public Task
    {
    public:
        static Power& GetInstance();

        explicit Power(I2C::Stm32::Driver& chargerI2CDriver);

        [[noreturn]] void Run();

    private:
        static Power* Instance;

        static bool IsReg12Enabled();
        static void SetReg12Enabled(bool enabled);
        static bool IsReg5Enabled();
        static void SetReg5Enabled(bool enabled);
        static void SetReg12LedEnabled(bool enabled);
        static void SetReg5LedEnabled(bool enabled);
        static void SetRegPiLedEnabled(bool enabled);

        I2C::Stm32::Driver& m_ChargerI2CDriver;
        Bq25792::Device m_Charger;

        bool InitCharger();
    };
}
