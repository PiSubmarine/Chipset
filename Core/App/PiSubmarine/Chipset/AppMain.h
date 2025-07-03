#pragma once

#ifdef __cplusplus

#include <chrono>
#include "PiSubmarine/Chipset/I2CDriver.h"
#include "PiSubmarine/Max1726/Max1726.h"
#include "PiSubmarine/Bq25792/Bq25792.h"
#include "main.h"
#include "i2c.h"

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

		bool m_HasInitError = false;

		bool m_Lptim1Expired = false;

		bool InitMotherboard();
		void SleepWait(std::chrono::milliseconds delay);
		void UpdateLeds();

	};
}

extern "C"
{
#endif
void AppMainRun(void *argument);

#ifdef __cplusplus
}
#endif
