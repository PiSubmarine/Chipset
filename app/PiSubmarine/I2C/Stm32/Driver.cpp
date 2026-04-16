#include "PiSubmarine/I2C/Stm32/Driver.h"
#include <cassert>

#include "PiSubmarine/Chipset/Tasks/HostProtocol.h"

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

	Error::Api::Result<void> Driver::Write(uint8_t deviceAddress, std::span<const uint8_t> txData)
	{
		m_TaskHandle = osThreadGetId();

		// Ensure flags are clear before starting
		osThreadFlagsClear(0x01);

		if (HAL_I2C_Master_Transmit_DMA(m_I2c, deviceAddress << 1, const_cast<uint8_t*>(txData.data()), txData.size())
			!= HAL_OK)
		{
			m_TaskHandle = nullptr;
			return std::unexpected{Error::Api::Error{.Condition = Error::Api::ErrorCondition::CommunicationError}};
		}

		// Equivalent to ulTaskNotifyTake: wait for flag 0x01
		uint32_t flags = osThreadFlagsWait(0x01, osFlagsWaitAny, (100U * osKernelGetTickFreq()) / 1000U);

		m_TaskHandle = nullptr;

		// Check if we actually got the flag or if it timed out/errored
		if (flags & osFlagsError)
		{
			return std::unexpected{Error::Api::Error{.Condition = Error::Api::ErrorCondition::CommunicationError}};
		}
		return {};
	}

	Error::Api::Result<void> Driver::Read(uint8_t deviceAddress, std::span<uint8_t> rxData)
	{
		m_TaskHandle = osThreadGetId();

		osThreadFlagsClear(0x01);

		if (HAL_I2C_Master_Receive_DMA(m_I2c, deviceAddress << 1, rxData.data(), rxData.size()) != HAL_OK)
		{
			m_TaskHandle = nullptr;
			return std::unexpected{Error::Api::Error{.Condition = Error::Api::ErrorCondition::CommunicationError}};;
		}

		uint32_t flags = osThreadFlagsWait(0x01, osFlagsWaitAny, (100U * osKernelGetTickFreq()) / 1000U);

		m_TaskHandle = nullptr;

		if (flags & osFlagsError)
		{
			return std::unexpected{Error::Api::Error{.Condition = Error::Api::ErrorCondition::CommunicationError}};
		}
		return {};
	}

	osThreadId_t Driver::GetTaskHandle() const
	{
		return m_TaskHandle;
	}

	I2C_HandleTypeDef* Driver::GetI2CHandle() const
	{
		return m_I2c;
	}

	Error::Api::Result<void> Driver::WriteRead(uint8_t deviceAddress,
	                                           std::span<const uint8_t> txData,
	                                           std::span<uint8_t> rxData)
	{
		if (auto result = Write(deviceAddress, txData); !result.has_value())
		{
			return result;
		}
		return Read(deviceAddress, rxData);
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

	auto hostProtocolTask = PiSubmarine::Chipset::Tasks::HostProtocol::GetInstanceISR();
	if (hostProtocolTask && hostProtocolTask->GetI2CHandle() == hi2c)
	{
		hostProtocolTask->HalErrorCallbackISR();
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
