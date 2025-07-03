#pragma once

#ifdef __cplusplus

#include <chrono>
#include "PiSubmarine/Chipset/I2CDriver.h"
#include "PiSubmarine/Max1726/Max1726.h"
#include "PiSubmarine/Bq25792/Bq25792.h"
#include "PiSubmarine/Chipset/Api/MicroVolts.h"
#include "main.h"
#include "i2c.h"
#include <array>

enum class PowerState
{
	FullReset,
	WaitForReg12,
	WaitForReg5,
	WaitForRegPi,
	Running,
	Standby
};

namespace PiSubmarine::Chipset
{
	class AppMain
	{
	public:

		static constexpr Max1726::MicroAmpereHours BatmonCapacity{3500000};
		static constexpr Max1726::MicroAmperes BatmonTerminationCurrent{200000};
		static constexpr Max1726::MicroVolts BatmonEmptyVoltage {3500000};

		static AppMain* GetInstance(){return Instance;}
		AppMain();

		virtual ~AppMain();
		void Run();

		void LpTimCallback(LPTIM_HandleTypeDef *hlptim);
		void OnAdcConvertionCompletedHandler(ADC_HandleTypeDef *hadc);

		I2CDriver& GetRpiDriver();
		I2CDriver& GetChipsetDriver();
		I2CDriver& GetBatchgDriver();

	private:
		static AppMain* Instance;
		I2CDriver m_RpiI2CDriver{hi2c1};
		I2CDriver m_ChipsetI2CDriver{hi2c2};
		I2CDriver m_BatchgI2CDriver{hi2c3};
		PiSubmarine::Bq25792::Device<I2CDriver> m_Batchg{m_BatchgI2CDriver};
		PiSubmarine::Max1726::Device<I2CDriver> m_Batmon{m_RpiI2CDriver};
		bool m_Lptim1Expired = false;
		bool m_AdcComplete = false;
		PowerState m_PowerState = PowerState::FullReset;
		std::array<uint16_t, 4> m_AdcBuffer{0};

		bool InitBatteryManagers();
		void SleepWait(std::chrono::milliseconds delay);
		void UpdateLeds();

		void TickFullReset();

		void EnterWaitForReg12(PowerState oldState);
		void TickWaitForReg12();

		void EnterWaitForReg5(PowerState oldState);
		void TickWaitForReg5();

		void EnterWaitForRegPi(PowerState oldState);
		void TickWaitForRegPi();

		void EnterRunning(PowerState oldState);
		void TickRunning();

		void EnterStandby(PowerState oldState);
		void TickStandby();

		uint16_t GetAdcBallast() const;
		uint16_t GetAdcReg5() const;
		uint16_t GetAdcRegPi() const;
		uint16_t GetAdcTemp() const;
		Api::MicroVolts GetVoltageReg5(uint16_t reg5Adc);
		Api::MicroVolts GetVoltageRegPi(uint16_t regPiAdc);
	};
}

extern "C"
{
#endif
void AppMainRun(void *argument);

#ifdef __cplusplus
}
#endif
