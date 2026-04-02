#include "PiSubmarine/Chipset/Tasks/Power.h"
#include <cstdlib>
#include "main.h"
#include "stm32u0xx_hal_gpio.h"

using namespace std::chrono_literals;

namespace PiSubmarine::Chipset::Tasks
{
    Power* Power::Instance = nullptr;

    Power& Power::GetInstance()
    {
        if (Instance == nullptr)
        {
            std::abort();
        }
        return *Instance;
    }

    Power::Power(I2C::Stm32::Driver& chargerI2CDriver) : m_ChargerI2CDriver(chargerI2CDriver), m_Charger(chargerI2CDriver)
    {
        Instance = this;
    }

    void Power::Run()
    {
        SetReg5Enabled(false);
        SetReg12Enabled(false);

        SetRegPiLedEnabled(true);
        SetReg5LedEnabled(true);
        SetReg12LedEnabled(true);

        while (!InitCharger())
        {
            Delay(1000ms);
        }

        while (true)
        {
            Delay(1000ms);
        }
    }

    bool Power::IsReg12Enabled()
    {
        return HAL_GPIO_ReadPin(REG12_EN_GPIO_Port, REG12_EN_Pin);
    }

    void Power::SetReg12Enabled(bool enabled)
    {
        HAL_GPIO_WritePin(REG12_EN_GPIO_Port, REG12_EN_Pin, static_cast<GPIO_PinState>(enabled));
    }

    bool Power::IsReg5Enabled()
    {
        return HAL_GPIO_ReadPin(REG5_EN_GPIO_Port, REG5_EN_Pin);
    }

    void Power::SetReg5Enabled(bool enabled)
    {
        HAL_GPIO_WritePin(REG5_EN_GPIO_Port, REG5_EN_Pin, static_cast<GPIO_PinState>(enabled));
    }

    void Power::SetReg12LedEnabled(bool enabled)
    {
        HAL_GPIO_WritePin(LED_REG12_GPIO_Port, LED_REG12_Pin, static_cast<GPIO_PinState>(enabled));
    }

    void Power::SetReg5LedEnabled(bool enabled)
    {
        HAL_GPIO_WritePin(LED_REG5_GPIO_Port, LED_REG5_Pin, static_cast<GPIO_PinState>(enabled));
    }

    void Power::SetRegPiLedEnabled(bool enabled)
    {
        HAL_GPIO_WritePin(LED_REGPI_GPIO_Port, LED_REGPI_Pin, static_cast<GPIO_PinState>(enabled));
    }

    bool Power::InitCharger()
    {
        auto sysV = m_Charger.GetMinimalSystemVoltage();
        if (!sysV.has_value())
        {
            return false;
        }

        return true;
    }
}
