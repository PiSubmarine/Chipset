#include "PiSubmarine/I2C/Stm32/Driver.h"
#include <cassert>

namespace PiSubmarine::I2C::Stm32
{
    std::vector<Driver*> Driver::Drivers{};

    const std::vector<Driver*>& Driver::GetDrivers()
    {
        return Drivers;
    }

    Driver::~Driver()
    {
        // In production, you'd want to remove 'this' from the Drivers vector here
        assert(false);
    }

    Driver::Driver(I2C_HandleTypeDef* hi2c) : m_I2c(hi2c), m_TaskHandle(nullptr)
    {
        Drivers.emplace_back(this);
    }

    bool Driver::Write(uint8_t deviceAddress, uint8_t* txData, std::size_t len)
    {
        m_TaskHandle = osThreadGetId();

        // Ensure flags are clear before starting
        osThreadFlagsClear(0x01);

        if (HAL_I2C_Master_Transmit_DMA(m_I2c, deviceAddress << 1, txData, len) != HAL_OK)
        {
            m_TaskHandle = nullptr;
            return false;
        }

        // Equivalent to ulTaskNotifyTake: wait for flag 0x01
        uint32_t flags = osThreadFlagsWait(0x01, osFlagsWaitAny, (100U * osKernelGetTickFreq()) / 1000U);

        m_TaskHandle = nullptr;

        // Check if we actually got the flag or if it timed out/errored
        return !(flags & osFlagsError);
    }

    bool Driver::Read(uint8_t deviceAddress, uint8_t* rxData, std::size_t len)
    {
        m_TaskHandle = osThreadGetId();

        osThreadFlagsClear(0x01);

        if (HAL_I2C_Master_Receive_DMA(m_I2c, deviceAddress << 1, rxData, len) != HAL_OK)
        {
            m_TaskHandle = nullptr;
            return false;
        }

        uint32_t flags = osThreadFlagsWait(0x01, osFlagsWaitAny, (100U * osKernelGetTickFreq()) / 1000U);

        m_TaskHandle = nullptr;

        return !(flags & osFlagsError);
    }

    osThreadId_t Driver::GetTaskHandle() const
    {
        return m_TaskHandle;
    }

    I2C_HandleTypeDef* Driver::GetI2CHandle() const
    {
        return m_I2c;
    }
}

/**
 * Helper to notify the driver from HAL Callbacks
 */
static void DispatchI2CEvent(I2C_HandleTypeDef* hi2c)
{
    for (const auto& driver : PiSubmarine::I2C::Stm32::Driver::GetDrivers())
    {
        if (driver->GetI2CHandle() == hi2c)
        {
            osThreadId_t tid = driver->GetTaskHandle();
            if (tid != nullptr)
            {
                // osThreadFlagsSet is safe to call from ISR
                osThreadFlagsSet(tid, 0x01);
            }
            break;
        }
    }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    DispatchI2CEvent(hi2c);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    DispatchI2CEvent(hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* hi2c)
{
    // Important: Also notify on error so the thread doesn't hang for the full 100ms
    DispatchI2CEvent(hi2c);
}