#pragma once

#include <array>
#include <vector>
#include "cmsis_os2.h"

#include "main.h"
#include "PiSubmarine/I2C/Api/IDriver.h"

namespace PiSubmarine::I2C::Stm32
{
	class Driver : public Api::IDriver
	{
	public:
		static const std::vector<Driver*>& GetDrivers();

		Driver(const Driver& other) = delete;
		Driver(Driver&& other) noexcept = delete;
		Driver& operator=(const Driver& other) = delete;
		Driver& operator=(Driver&& other) noexcept = delete;
		~Driver() override;

		explicit Driver(I2C_HandleTypeDef* hi2c);

		[[nodiscard]] Error::Api::Result<void> Write(uint8_t deviceAddress, std::span<const uint8_t> txData) override;
		[[nodiscard]] Error::Api::Result<void> Read(uint8_t deviceAddress, std::span<uint8_t> rxData) override;
		[[nodiscard]] Error::Api::Result<void> WriteRead(uint8_t deviceAddress,
		                                                 std::span<const uint8_t> txData,
		                                                 std::span<uint8_t> rxData) override;

		[[nodiscard]] osThreadId_t GetTaskHandle() const;
		[[nodiscard]] I2C_HandleTypeDef* GetI2CHandle() const;

	private:
		static std::vector<Driver*> Drivers;

		I2C_HandleTypeDef* m_I2c;
		std::array<uint8_t, 8> m_Buffer{0};
		osThreadId_t m_TaskHandle{};
	};
}
