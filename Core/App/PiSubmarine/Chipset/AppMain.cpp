/*
 * AppMain.cpp
 *
 *  Created on: Jan 13, 2025
 *      Author: DmitriyPC
 */

#include "PiSubmarine/Chipset/AppMain.h"
#include "PiSubmarine/Chipset/Api/Command.h"
#include "PiSubmarine/Chipset/Api/PacketShutdown.h"
#include "PiSubmarine/Chipset/Api/PacketSetTime.h"
#include "usart.h"
#include "i2c.h"
#include "lptim.h"
#include <vector>
#include <stdio.h>
#include "adc.h"
#include "crc.h"

using namespace std::chrono_literals;
using namespace PiSubmarine::Bq25792;
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

		// RPI_SDA_GPIO_Port->PUPDR |= (0b11ULL << (7 * 2));
		// RPI_SCL_GPIO_Port->PUPDR |= (0b11ULL << (6 * 2));
		// Gives time for weak I2C pull-ups to pressurize the lines.
		// SleepWait(1000ms);

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

		// Disable RPI_I2C pull-ups. After REG5 boots, RegRpi will drive RPI_I2C
		// RPI_SDA_GPIO_Port->PUPDR &= ~(0b11ULL << (7 * 2));
		// RPI_SCL_GPIO_Port->PUPDR &= ~(0b11ULL << (6 * 2));

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

	void AppMain::AdcConvertionCompletedCallback(ADC_HandleTypeDef *hadc)
	{
		(void) hadc;
		m_AdcComplete = true;

		uint16_t ballastAdc = GetAdcBallast();
		m_PacketOut.BallastAdc = Api::Percentage<12>(ballastAdc);

		uint16_t reg5Adc = GetAdcReg5();
		m_PacketOut.Reg5Voltage = GetVoltageReg5(reg5Adc);

		uint16_t regPiAdc = GetAdcRegPi();
		m_PacketOut.RegPiVoltage = GetVoltageRegPi(regPiAdc);

		uint16_t tempAdc = GetAdcTemp();
		m_PacketOut.ChipsetTemperature = GetTemperature(tempAdc);

		m_PacketOut.Status = m_PacketOut.Status | Api::StatusFlags::AdcValid;
	}

	void AppMain::I2CAddressCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
	{
		(void) AddrMatchCode;
		if (m_PowerState != PowerState::Running)
		{
			return;
		}

		if (hi2c != &hi2c1)
		{
			return;
		}

		HAL_I2C_DisableListen_IT(&hi2c1);

		if (TransferDirection == I2C_DIRECTION_TRANSMIT)
		{
			HAL_I2C_Slave_Receive_IT(hi2c, m_RpiReceiveBuffer.data(), m_RpiReceiveBuffer.size());
		}
		else
		{
			PiSubmarine::Chipset::Api::Crc32Func crcFunc = [this](const uint8_t *data, size_t size)
			{	return Crc32(data, size);};
			m_PacketOut.ChipsetTime = GetTimestamp();
			m_PacketOut.Serialize(m_PacketOutSerialized.data(), m_PacketOutSerialized.size(), crcFunc);

			// Clear packet data
			m_PacketOut.ChipsetTime = std::chrono::milliseconds(0);
			m_PacketOut.Status = Api::StatusFlags { 0 };

			HAL_I2C_Slave_Transmit_DMA(&hi2c1, m_PacketOutSerialized.data(), m_PacketOutSerialized.size());
		}

	}

	void AppMain::I2CListenCompleteCallback(I2C_HandleTypeDef *hi2c)
	{
		if (m_PowerState != PowerState::Running)
		{
			return;
		}

		if (hi2c != &hi2c1)
		{
			return;
		}

		HAL_I2C_EnableListen_IT(hi2c);
		printf("L\n");
	}

	void AppMain::I2CSlaveRxCompleteCallback(I2C_HandleTypeDef *hi2c)
	{
		if (m_PowerState != PowerState::Running)
		{
			return;
		}

		if (hi2c != &hi2c1)
		{
			return;
		}

		printf("C: 0x%X\n", m_RpiReceiveBuffer[0]);
		Api::Command command = static_cast<Api::Command>(m_RpiReceiveBuffer[0]);
		switch (command)
		{
		case Api::Command::SetTime:
			OnSetTimeCommand();
			break;
		case Api::Command::Shutdown:
			OnShutdownCommand();
			break;
		default:
			break;
		}

		HAL_I2C_EnableListen_IT(hi2c);

	}

	void AppMain::I2CSlaveTxCompleteCallback(I2C_HandleTypeDef *hi2c)
	{
		if (m_PowerState != PowerState::Running)
		{
			return;
		}

		if (hi2c != &hi2c1)
		{
			return;
		}
		HAL_I2C_EnableListen_IT(hi2c);
		printf("T\n");
	}

	void AppMain::I2CErrorCallback(I2C_HandleTypeDef *hi2c)
	{
		if (m_PowerState != PowerState::Running)
		{
			return;
		}

		if (hi2c != &hi2c1)
		{
			return;
		}
		HAL_I2C_EnableListen_IT(hi2c);
		printf("E\n");
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

		WaitFunc delayFunc = [this](std::chrono::milliseconds delay)
		{	SleepWait(delay);};

		// Init BATCHG
		if (!m_Batchg.ReadAndWait(delayFunc))
		{
			return false;
		}

		m_Batchg.SetChargeCurrentLimit(PiSubmarine::Bq25792::MilliAmperes(3000));
		m_Batchg.SetTsIgnore(true);
		m_Batchg.SetWatchdog(PiSubmarine::Bq25792::Watchdog::Disable);
		// m_Batchg.SetAdcEnabled(true);
		m_Batchg.SetDischargeOcpEnabled(true);
		// m_Batchg.SetDischargeCurrentSensingEnabled(true);
		m_Batchg.SetIlimHizCurrentLimitEnabled(false);
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

	void AppMain::SleepWait(std::chrono::milliseconds delay, bool interruptable)
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

		if (interruptable)
		{
			HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
			HAL_LPTIM_TimeOut_Stop_IT(&hlptim1);
		}
		else
		{
			while (!m_Lptim1Expired)
			{
				HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
			}
			m_Lptim1Expired = false;
		}

		HAL_ResumeTick();
	}

	void AppMain::TickFullReset()
	{
		m_PowerState = PowerState::WaitForReg12;
	}

	void AppMain::EnterStandby(PowerState oldState)
	{
		(void) oldState;
		HAL_I2C_MspDeInit(&hi2c1);
		HAL_I2C_MspDeInit(&hi2c2);
		HAL_I2C_MspDeInit(&hi2c3);

		HAL_I2C_DisableListen_IT(&hi2c1);
		HAL_ADC_Stop_DMA(&hadc1);

		SleepWait(m_ShutdownDelay);

		HAL_LPTIM_PWM_Stop(&hlptim2, LPTIM_CHANNEL_1);
		HAL_LPTIM_PWM_Start(&hlptim2, LPTIM_CHANNEL_1);
		hlptim2.Instance->ARR = 5000;

		HAL_GPIO_WritePin(REG5_EN_GPIO_Port, REG5_EN_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(REG12_EN_GPIO_Port, REG12_EN_Pin, GPIO_PIN_RESET);

		HAL_SuspendTick();
		HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
		HAL_ResumeTick();

		HAL_LPTIM_PWM_Stop(&hlptim2, LPTIM_CHANNEL_1);

		MX_I2C1_Init();
		MX_I2C2_Init();
		MX_I2C3_Init();
	}

	void AppMain::TickStandby()
	{
		m_PowerState = PowerState::WaitForReg12;
	}

	void AppMain::EnterWaitForReg12(PowerState oldState)
	{
		(void) oldState;
		HAL_GPIO_WritePin(CHIPSET_INT_GPIO_Port, CHIPSET_INT_Pin, GPIO_PIN_RESET);
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
			SleepWait(1ms);
			return;
		}
		HAL_GPIO_WritePin(LED_REG12_GPIO_Port, LED_REG12_Pin, GPIO_PIN_RESET);
		m_PowerState = PowerState::WaitForReg5;
	}

	void AppMain::EnterWaitForReg5(PowerState oldState)
	{
		(void) oldState;
		HAL_GPIO_WritePin(REG5_EN_GPIO_Port, REG5_EN_Pin, GPIO_PIN_SET);

		StartAdcOneShot();
	}

	void AppMain::TickWaitForReg5()
	{
		if (!m_AdcComplete)
		{
			SleepWait(10ms);
			return;
		}

		if (m_PacketOut.Reg5Voltage.Get() < Api::MicroVolts(4900000).Get())
		{
			StartAdcOneShot();
			return;
		}

		HAL_GPIO_WritePin(LED_REG5_GPIO_Port, LED_REG5_Pin, GPIO_PIN_RESET);
		m_PowerState = PowerState::WaitForRegPi;
	}

	void AppMain::EnterWaitForRegPi(PowerState oldState)
	{
		(void) oldState;
		StartAdcOneShot();
	}

	void AppMain::TickWaitForRegPi()
	{
		if (!m_AdcComplete)
		{
			SleepWait(10ms);
			return;
		}

		Api::MicroVolts regPiVoltage = m_PacketOut.RegPiVoltage;
		printf("RegPi Voltage: %lu\n", static_cast<uint32_t>(regPiVoltage.Get() / 1000));

		if (regPiVoltage.Get() < Api::MicroVolts(3200000).Get())
		{
			StartAdcOneShot();
			return;
		}

		HAL_GPIO_WritePin(LED_REGPI_GPIO_Port, LED_REGPI_Pin, GPIO_PIN_RESET);
		m_PowerState = PowerState::Running;
	}

	void AppMain::EnterRunning(PowerState oldState)
	{
		(void) oldState;
		StartAdcOneShot();
		HAL_I2C_EnableListen_IT(&hi2c1);
	}

	void AppMain::TickRunning()
	{
		WaitFunc delayFunc = [this](std::chrono::milliseconds delay)
		{	SleepWait(delay);};

		if (m_AdcComplete)
		{
			StartAdcOneShot();
		}

		bool batchgOk = m_Batchg.Read();
		batchgOk &= m_Batchg.WaitForTransaction(delayFunc);

		if (batchgOk)
		{
			m_PacketOut.Status = m_PacketOut.Status | Api::StatusFlags::BatchgValid;
			auto status0 = m_Batchg.GetChargerStatus0();
			auto chargeStatus = m_Batchg.GetChargeStatus();
			bool vbusPresent = RegUtils::HasAnyFlag(status0, ChargerStatus0Flags::VbusPresentStat);
			bool isCharging = chargeStatus != ChargeStatus::NotCharging && chargeStatus != ChargeStatus::ChargingTerminationDone;

			if (vbusPresent)
			{
				m_PacketOut.Status = m_PacketOut.Status | Api::StatusFlags::VbusConnected;
			}
			if (isCharging)
			{
				m_PacketOut.Status = m_PacketOut.Status | Api::StatusFlags::ChargingInProgress;
			}
		}

		SleepWait(100ms, true);
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

	Api::MicroVolts AppMain::GetVoltageReg5(uint16_t reg5Adc) const
	{
		constexpr auto adcRef = Api::MicroVolts(3300000);

		auto adcScaled = adcRef * reg5Adc / ((1ULL << 12) - 1) * 2;
		return adcScaled;
	}

	Api::MicroVolts AppMain::GetVoltageRegPi(uint16_t regPiAdc) const
	{
		constexpr auto adcRef = Api::MicroVolts(3300000);
		auto adcScaled = adcRef * regPiAdc / ((1ULL << 12) - 1) * 2;
		return adcScaled;
	}

	Api::MicroKelvins AppMain::GetTemperature(uint16_t tempAdc) const
	{
		constexpr uint16_t tsCal1Temp = 30;
		constexpr uint16_t tsCal2Temp = 130;
		uint16_t tsCal1 = *(reinterpret_cast<uint16_t*>(0x1FFF6E68)) * 33 / 30;
		uint16_t tsCal2 = *(reinterpret_cast<uint16_t*>(0x1FFF6E8A)) * 33 / 30;
		int32_t tsData = tempAdc;
		int32_t tsCalTempDelta = tsCal2Temp - tsCal1Temp;
		int32_t tsCalDelta = tsCal2 - tsCal1;
		int32_t tsDataDelta = tsData - (int32_t) tsCal1;
		int64_t celcius = tsCalTempDelta * tsDataDelta * 1000000 / tsCalDelta + tsCal1Temp * 1000000;
		uint64_t kelvin = celcius + 273150000;
		return Api::MicroKelvins(kelvin);
	}

	std::chrono::milliseconds AppMain::GetTimestamp() const
	{
		if (!IsRtcCorrect())
		{
			return std::chrono::milliseconds(0);
		}

		RTC_TimeTypeDef currentTime;
		RTC_DateTypeDef currentDate;
		time_t timestamp;
		tm currTime;

		HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BCD);
		HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BCD);

		currTime.tm_year = RTC_Bcd2ToByte(currentDate.Year) + 100;  // In fact: 2000 + 18 - 1900
		currTime.tm_mday = RTC_Bcd2ToByte(currentDate.Date);
		currTime.tm_mon = RTC_Bcd2ToByte(currentDate.Month) - 1;

		currTime.tm_hour = RTC_Bcd2ToByte(currentTime.Hours);
		currTime.tm_min = RTC_Bcd2ToByte(currentTime.Minutes);
		currTime.tm_sec = RTC_Bcd2ToByte(currentTime.Seconds);

		timestamp = mktime(&currTime) * 1000;
		return std::chrono::milliseconds(timestamp);
	}

	void AppMain::ToRtc(std::chrono::milliseconds Timestamp, RTC_TimeTypeDef &OutTime, RTC_DateTypeDef &OutDate) const
	{
		time_t RawTime = Timestamp.count() / 1000;
		// uint32_t Milliseconds = Timestamp % 1000;
		tm ptm;
		gmtime_r(&RawTime, &ptm);

		OutDate.Year = RTC_ByteToBcd2(ptm.tm_year - 100);
		OutDate.Date = RTC_ByteToBcd2(ptm.tm_mday);
		OutDate.Month = RTC_ByteToBcd2(ptm.tm_mon + 1);
		OutDate.WeekDay = RTC_WEEKDAY_MONDAY;

		OutTime.Hours = RTC_ByteToBcd2(ptm.tm_hour);
		OutTime.Minutes = RTC_ByteToBcd2(ptm.tm_min);
		OutTime.Seconds = RTC_ByteToBcd2(ptm.tm_sec);
		OutTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
		OutTime.SecondFraction = 0;
		OutTime.StoreOperation = 0;
		OutTime.SubSeconds = 0;
		OutTime.TimeFormat = RTC_HOURFORMAT_24;
	}

	void AppMain::SetRtc(RTC_TimeTypeDef &Time, RTC_DateTypeDef &Date)
	{
		HAL_RTC_SetTime(&hrtc, &Time, RTC_FORMAT_BCD);
		HAL_RTC_SetDate(&hrtc, &Date, RTC_FORMAT_BCD);
	}

	uint32_t AppMain::Crc32(const uint8_t *data, size_t size)
	{
		uint8_t *dataNc = const_cast<uint8_t*>(data);
		return HAL_CRC_Calculate(&hcrc, reinterpret_cast<uint32_t*>(dataNc), size);
	}

	void AppMain::StartAdcOneShot()
	{
		m_AdcComplete = false;
		HAL_ADC_Start_DMA(&hadc1, reinterpret_cast<uint32_t*>(m_AdcBuffer.data()), m_AdcBuffer.size());
		__HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);
	}

	void AppMain::OnSetTimeCommand()
	{
		PiSubmarine::Chipset::Api::Crc32Func crcFunc = [this](const uint8_t *data, size_t size)
		{	return Crc32(data, size);};

		Api::PacketSetTime setTime;
		if (!setTime.Deserialize(m_RpiReceiveBuffer.data(), Api::PacketSetTime::Size, crcFunc))
		{
			printf("Cf\n");
			return;
		}

		RTC_TimeTypeDef time;
		RTC_DateTypeDef date;
		ToRtc(setTime.RtcTime, time, date);
		SetRtc(time, date);
	}

	void AppMain::OnShutdownCommand()
	{
		PiSubmarine::Chipset::Api::Crc32Func crcFunc = [this](const uint8_t *data, size_t size)
		{	return Crc32(data, size);};

		Api::PacketShutdown shutdown;
		if (!shutdown.Deserialize(m_RpiReceiveBuffer.data(), Api::PacketShutdown::Size, crcFunc))
		{
			printf("Cf\n");
			return;
		}

		printf("Shutdown in %u\n", static_cast<uint32_t>(shutdown.Delay.count()));

		m_ShutdownDelay = shutdown.Delay;
		m_PowerState = PowerState::Standby;
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

	int IsRtcCorrect()
	{
		return (RTC->ICSR & RTC_ICSR_INITS) != 0;
	}

	void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
	{
		PiSubmarine::Chipset::AppMain *app = PiSubmarine::Chipset::AppMain::GetInstance();
		/*
		 auto error = HAL_I2C_GetError(hi2c);
		 if(error == HAL_I2C_ERROR_NONE)
		 {

		 }
		 */
		if (hi2c == &hi2c1)
		{
			app->I2CErrorCallback(hi2c);
		}
		else
		{
			/*
			 if (hi2c == app->GetRpiDriver().GetHandlePtr())
			 {
			 app->GetRpiDriver().OnErrorCallback(hi2c);
			 }
			 */
			if (hi2c == app->GetChipsetDriver().GetHandlePtr())
			{
				app->GetChipsetDriver().OnErrorCallback(hi2c);
			}
			else if (hi2c == app->GetBatchgDriver().GetHandlePtr())
			{
				app->GetBatchgDriver().OnErrorCallback(hi2c);
			}
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

		app->AdcConvertionCompletedCallback(hadc);
	}

	void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
	{
		auto *app = PiSubmarine::Chipset::AppMain::GetInstance();
		if (!app)
		{
			return;
		}

		app->I2CAddressCallback(hi2c, TransferDirection, AddrMatchCode);
	}

	void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
	{
		auto *app = PiSubmarine::Chipset::AppMain::GetInstance();
		if (!app)
		{
			return;
		}

		app->I2CListenCompleteCallback(hi2c);
	}

	void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
	{
		auto *app = PiSubmarine::Chipset::AppMain::GetInstance();
		if (!app)
		{
			return;
		}
		app->I2CSlaveTxCompleteCallback(hi2c);
	}

	void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
	{
		auto *app = PiSubmarine::Chipset::AppMain::GetInstance();
		if (!app)
		{
			return;
		}
		app->I2CSlaveRxCompleteCallback(hi2c);
	}

}
