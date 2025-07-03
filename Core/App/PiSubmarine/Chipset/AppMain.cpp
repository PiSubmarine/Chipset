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
#include "adc.h"

using namespace std::chrono_literals;
using namespace PiSubmarine::Max1726;
using namespace PiSubmarine::RegUtils;

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
		HAL_LPTIM_PWM_Start(&hlptim2, LPTIM_CHANNEL_1);
		while (true)
		{
			bool initOk = InitBatteryManagers();
			if (initOk)
			{
				break;
			}
			SleepWait(3000ms);
		}
		HAL_LPTIM_PWM_Stop(&hlptim2, LPTIM_CHANNEL_1);

		while (true)
		{
			PowerState powerStateOld = m_PowerState;

			switch (powerStateOld)
			{
			case PowerState::FullReset:
				TickFullReset();
				break;
			case PowerState::WaitForReg12:
				TickWaitForReg12();
				break;
			case PowerState::WaitForReg5:
				TickWaitForReg5();
				break;
			case PowerState::WaitForRegPi:
				TickWaitForRegPi();
				break;
			case PowerState::Running:
				TickRunning();
				break;
			case PowerState::Standby:
				TickStandby();
				break;
			}

			if (m_PowerState != powerStateOld)
			{
				switch (m_PowerState)
				{
				case PowerState::FullReset:
					Error_Handler();
					break;
				case PowerState::WaitForReg12:
					EnterWaitForReg12(powerStateOld);
					break;
				case PowerState::WaitForReg5:
					EnterWaitForReg5(powerStateOld);
					break;
				case PowerState::WaitForRegPi:
					EnterWaitForRegPi(powerStateOld);
					break;
				case PowerState::Running:
					EnterRunning(powerStateOld);
					break;
				case PowerState::Standby:
					EnterStandby(powerStateOld);
					break;
				}
			}
		}
	}

	void AppMain::LpTimCallback(LPTIM_HandleTypeDef *hlptim)
	{
		if (hlptim == &hlptim1)
		{
			m_Lptim1Expired = true;
			HAL_LPTIM_TimeOut_Stop_IT(&hlptim1);
		}
	}

	void AppMain::OnAdcConvertionCompletedHandler(ADC_HandleTypeDef *hadc)
	{
		m_AdcComplete = true;
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

	bool AppMain::InitBatteryManagers()
	{
		// Force-disable regulators
		HAL_GPIO_WritePin(REG12_EN_GPIO_Port, REG12_EN_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(REG5_EN_GPIO_Port, REG5_EN_Pin, GPIO_PIN_RESET);

		// Init BATMON
		WaitFunc delayFunc = [this](std::chrono::milliseconds delay)
		{	SleepWait(delay);};

		if (!m_Batmon.InitBlocking(delayFunc, BatmonCapacity, BatmonTerminationCurrent, BatmonEmptyVoltage))
		{
			return false;
		}
		if (!m_Batmon.ReadAndWait(RegOffset::Config, delayFunc))
		{
			return false;
		}
		auto config = m_Batmon.GetConfig();
		config = config & ~(ConfigFlags::EnableTemperatureChannel | ConfigFlags::EnableThermistor | ConfigFlags::TemperatureAlertSticky);
		m_Batmon.SetConfig(config);
		if (!m_Batmon.WriteAndWait(RegOffset::Config, delayFunc))
		{
			return false;
		}

		// Init BATCHG
		if (!m_Batchg.ReadAndWait(delayFunc))
		{
			return false;
		}

		m_Batchg.SetChargeCurrentLimit(PiSubmarine::Bq25792::MilliAmperes(3000));
		m_Batchg.SetTsIgnore(true);
		m_Batchg.SetWatchdog(PiSubmarine::Bq25792::Watchdog::Disable);
		// m_Batchg.SetAdcEnabled(true);
		// m_Batchg.SetDischargeOcpEnabled(true);
		// m_Batchg.SetDischargeCurrentSensingEnabled(true);
		// m_Batchg.SetIlimHizCurrentLimitEnabled(false);
		if (!m_Batchg.WriteDirty())
		{
			return false;
		}

		if (!m_Batchg.WaitForTransaction(delayFunc))
		{
			return false;
		}

		return true;
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

	void AppMain::UpdateLeds()
	{
		HAL_GPIO_WritePin(LED_BAT_GPIO_Port, LED_BAT_Pin, GPIO_PIN_SET);
	}

	void AppMain::TickFullReset()
	{
		m_PowerState = PowerState::WaitForReg12;
	}

	void AppMain::EnterStandby(PowerState oldState)
	{
		hlptim2.Instance->ARR = 5000;
		HAL_LPTIM_PWM_Start(&hlptim2, LPTIM_CHANNEL_1);

		HAL_ADC_Stop_DMA(&hadc1);

		HAL_GPIO_WritePin(REG12_EN_GPIO_Port, REG12_EN_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(REG5_EN_GPIO_Port, REG5_EN_Pin, GPIO_PIN_RESET);

		HAL_SuspendTick();
		HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
		HAL_ResumeTick();

		HAL_LPTIM_PWM_Stop(&hlptim2, LPTIM_CHANNEL_1);
	}

	void AppMain::TickStandby()
	{
		m_PowerState = PowerState::WaitForReg12;
	}

	void AppMain::EnterWaitForReg12(PowerState oldState)
	{
		HAL_GPIO_WritePin(LED_REG5_GPIO_Port, LED_REG5_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_REG12_GPIO_Port, LED_REG12_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_REGPI_GPIO_Port, LED_REGPI_Pin, GPIO_PIN_SET);

		HAL_GPIO_WritePin(REG12_EN_GPIO_Port, REG12_EN_Pin, GPIO_PIN_SET);
	}

	void AppMain::TickWaitForReg12()
	{
		auto reg12Pg = HAL_GPIO_ReadPin(REG12_PG_GPIO_Port, REG12_PG_Pin);
		if (reg12Pg != GPIO_PIN_SET)
		{
			SleepWait(10ms);
			return;
		}
		HAL_GPIO_WritePin(LED_REG12_GPIO_Port, LED_REG12_Pin, GPIO_PIN_RESET);
		m_PowerState = PowerState::WaitForReg5;
	}

	void AppMain::EnterWaitForReg5(PowerState oldState)
	{
		HAL_GPIO_WritePin(REG5_EN_GPIO_Port, REG5_EN_Pin, GPIO_PIN_SET);

		m_AdcComplete = false;
		HAL_ADC_Start_DMA(&hadc1, reinterpret_cast<uint32_t*>(m_AdcBuffer.data()), m_AdcBuffer.size());
		__HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);
	}

	void AppMain::TickWaitForReg5()
	{
		if (!m_AdcComplete)
		{
			SleepWait(10ms);
			return;
		}
		m_AdcComplete = false;

		uint16_t reg5Adc = GetAdcReg5();
		Api::MicroVolts reg5Voltage = GetVoltageReg5(reg5Adc);
		if (reg5Voltage.Get() < Api::MicroVolts(490000).Get())
		{
			SleepWait(10ms);
			return;
		}

		HAL_GPIO_WritePin(LED_REG5_GPIO_Port, LED_REG5_Pin, GPIO_PIN_RESET);
		m_PowerState = PowerState::WaitForRegPi;
	}

	void AppMain::EnterWaitForRegPi(PowerState oldState)
	{
		m_AdcComplete = false;
	}

	void AppMain::TickWaitForRegPi()
	{
		if (!m_AdcComplete)
		{
			SleepWait(10ms);
			return;
		}
		m_AdcComplete = false;

		uint16_t regPiAdc = GetAdcRegPi();
		Api::MicroVolts regPiVoltage = GetVoltageRegPi(regPiAdc);
		uint32_t regPiMv = regPiVoltage.Get() / 1000;
		if (regPiVoltage.Get() < Api::MicroVolts(32000000).Get())
		{
			SleepWait(10ms);
			return;
		}

		HAL_GPIO_WritePin(LED_REGPI_GPIO_Port, LED_REGPI_Pin, GPIO_PIN_RESET);
		m_PowerState = PowerState::Running;
	}

	void AppMain::EnterRunning(PowerState oldState)
	{

	}

	void AppMain::TickRunning()
	{
		SleepWait(10ms);
	}

	uint16_t AppMain::GetAdcBallast() const
	{
		return m_AdcBuffer[0];
	}

	uint16_t AppMain::GetAdcReg5() const
	{
		return m_AdcBuffer[1];
	}

	uint16_t AppMain::GetAdcRegPi() const
	{
		return m_AdcBuffer[2];
	}

	uint16_t AppMain::GetAdcTemp() const
	{
		return m_AdcBuffer[3];
	}

	Api::MicroVolts AppMain::GetVoltageReg5(uint16_t reg5Adc)
	{
		constexpr auto adcRef = Api::MicroVolts(3300000);

		auto adcScaled = adcRef * reg5Adc / ((1ULL << 12) - 1) * 2;
		return adcScaled;
	}

	Api::MicroVolts AppMain::GetVoltageRegPi(uint16_t regPiAdc)
	{
		constexpr auto adcRef = Api::MicroVolts(3300000);
		auto adcScaled = adcRef * regPiAdc / ((1ULL << 12) - 1) * 2;
		return adcScaled;
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
			else if (hi2c == app->GetBatchgDriver().GetHandlePtr())
			{
				app->GetBatchgDriver().OnErrorCallback(hi2c);
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
			else if (hi2c == app->GetBatchgDriver().GetHandlePtr())
			{
				app->GetBatchgDriver().OnMasterTxCplt(hi2c);
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
			else if (hi2c == app->GetBatchgDriver().GetHandlePtr())
			{
				app->GetBatchgDriver().OnMasterRxCplt(hi2c);
			}
		}

		void HAL_LPTIM_CompareMatchCallback(LPTIM_HandleTypeDef *hlptim)
		{
			PiSubmarine::Chipset::AppMain::GetInstance()->LpTimCallback(hlptim);
		}

		void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
		{
			auto *app = PiSubmarine::Chipset::AppMain::GetInstance();
			if (!app)
			{
				return;
			}

			app->OnAdcConvertionCompletedHandler(hadc);
		}
	}
}
