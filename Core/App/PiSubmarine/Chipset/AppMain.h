#pragma once

#ifdef __cplusplus

#include <chrono>
#include "PiSubmarine/Chipset/I2CDriver.h"
#include "main.h"
#include "i2c.h"

namespace PiSubmarine::Chipset
{
	class AppMain
	{
	public:

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

		bool m_Lptim1Expired = false;

		void InitMotherboard();

		void SleepWait(std::chrono::milliseconds delay);

	};
}

extern "C"
{
#endif
void AppMainRun(void *argument);

#ifdef __cplusplus
}
#endif
