/*
 * AppMain.cpp
 *
 *  Created on: Jan 13, 2025
 *      Author: DmitriyPC
 */

#include "PiSubmarine/Chipset/AppMain.h"
#include "usart.h"
#include "i2c.h"
#include "lptim.h"
#include <vector>
#include <stdio.h>

using namespace std::chrono_literals;

namespace PiSubmarine::Chipset
{
	AppMain *AppMain::Instance = nullptr;

	AppMain::AppMain()
	{
		Instance = this;
	}

	AppMain::~AppMain()
	{
		Instance = nullptr;
	}

	void AppMain::Run()
	{
		// Full System Reset happened.
		InitMotherboard();

		SleepWait(3000ms);

		HAL_LPTIM_PWM_Start(&hlptim2, LPTIM_CHANNEL_1);

		while (true)
		{
			HAL_Delay(1000);
		}
	}

	void AppMain::LpTimCallback(LPTIM_HandleTypeDef *hlptim)
	{
		if(hlptim == &hlptim1)
		{
			m_Lptim1Expired = true;
			HAL_LPTIM_TimeOut_Stop_IT(&hlptim1);
		}
	}

	I2CDriver& AppMain::GetRpiDriver()
	{
		return m_RpiI2CDriver;
	}

	I2CDriver& AppMain::GetChipsetDriver()
	{
		return m_ChipsetI2CDriver;
	}

	I2CDriver& AppMain::GetBatchgDriver()
	{
		return m_BatchgI2CDriver;
	}

	void AppMain::InitMotherboard()
	{
		// Force-disable regulators
		HAL_GPIO_WritePin(REG12_EN_GPIO_Port, REG12_EN_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(REG5_EN_GPIO_Port, REG5_EN_Pin, GPIO_PIN_RESET);
	}

	void AppMain::SleepWait(std::chrono::milliseconds delay)
	{
		if (delay.count() == 0)
		{
			return;
		}

		HAL_SuspendTick();

		// One LPTIM tick is exactly 1ms
		uint32_t timeoutTicks = static_cast<uint32_t>(delay.count());

		m_Lptim1Expired = false;
		HAL_LPTIM_TimeOut_Start_IT(&hlptim1, timeoutTicks);
		__HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_ARRM);
		while (!m_Lptim1Expired)
		{
			HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
		}
		m_Lptim1Expired = false;
		HAL_ResumeTick();
	}

}

extern "C"
{
	void AppMainRun(void *argument)
	{
		(void) argument;
		PiSubmarine::Chipset::AppMain app;
		app.Run();
	}

	int __io_putchar(int ch)
	{
		HAL_UART_Transmit(&huart1, (uint8_t*) &ch, 1, 0xFFFF);
		return ch;
	}

	extern "C"
	{
		void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
		{
			PiSubmarine::Chipset::AppMain *app = PiSubmarine::Chipset::AppMain::GetInstance();
			if (hi2c == app->GetRpiDriver().GetHandlePtr())
			{
				app->GetRpiDriver().OnErrorCallback(hi2c);
			}
			else if (hi2c == app->GetChipsetDriver().GetHandlePtr())
			{
				app->GetChipsetDriver().OnErrorCallback(hi2c);
			}
			else if (hi2c == app->GetChipsetDriver().GetHandlePtr())
			{
				app->GetChipsetDriver().OnErrorCallback(hi2c);
			}
		}

		void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
		{

			PiSubmarine::Chipset::AppMain *app = PiSubmarine::Chipset::AppMain::GetInstance();
			if (hi2c == app->GetRpiDriver().GetHandlePtr())
			{
				app->GetRpiDriver().OnMasterTxCplt(hi2c);
			}
			else if (hi2c == app->GetChipsetDriver().GetHandlePtr())
			{
				app->GetChipsetDriver().OnMasterTxCplt(hi2c);
			}
			else if (hi2c == app->GetChipsetDriver().GetHandlePtr())
			{
				app->GetChipsetDriver().OnMasterTxCplt(hi2c);
			}
		}

		void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
		{

			PiSubmarine::Chipset::AppMain *app = PiSubmarine::Chipset::AppMain::GetInstance();
			if (hi2c == app->GetRpiDriver().GetHandlePtr())
			{
				app->GetRpiDriver().OnMasterRxCplt(hi2c);
			}
			else if (hi2c == app->GetChipsetDriver().GetHandlePtr())
			{
				app->GetChipsetDriver().OnMasterRxCplt(hi2c);
			}
			else if (hi2c == app->GetChipsetDriver().GetHandlePtr())
			{
				app->GetChipsetDriver().OnMasterRxCplt(hi2c);
			}
		}

		void HAL_LPTIM_CompareMatchCallback(LPTIM_HandleTypeDef *hlptim)
		{
			PiSubmarine::Chipset::AppMain::GetInstance()->LpTimCallback(hlptim);
		}

	}
}
