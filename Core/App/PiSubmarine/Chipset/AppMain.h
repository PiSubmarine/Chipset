#pragma once

#ifdef __cplusplus

#include <chrono>
#include "PiSubmarine/Chipset/I2CDriver.h"
#include "PiSubmarine/Bq25792/Bq25792.h"
#include "PiSubmarine/Chipset/Api/MicroVolts.h"
#include "PiSubmarine/Chipset/Api/MicroKelvins.h"
#include "PiSubmarine/Chipset/Api/PacketOut.h"
#include "main.h"
#include "i2c.h"
#include <array>
#include "rtc.h"

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

		static AppMain* GetInstance(){return Instance;}
		AppMain();

		virtual ~AppMain();
		void Run();

		void LpTimCallback(LPTIM_HandleTypeDef *hlptim);
		void AdcConvertionCompletedCallback(ADC_HandleTypeDef *hadc);
		void I2CAddressCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode);
		void I2CListenCompleteCallback(I2C_HandleTypeDef *hi2c);
		void I2CSlaveRxCompleteCallback(I2C_HandleTypeDef *hi2c);
		void I2CSlaveTxCompleteCallback(I2C_HandleTypeDef *hi2c);
		void I2CErrorCallback(I2C_HandleTypeDef *hi2c);

		I2CDriver& GetRpiDriver();
		I2CDriver& GetChipsetDriver();
		I2CDriver& GetBatchgDriver();

	private:
		static AppMain* Instance;
		I2CDriver m_RpiI2CDriver{hi2c1};
		I2CDriver m_ChipsetI2CDriver{hi2c2};
		I2CDriver m_BatchgI2CDriver{hi2c3};
		PiSubmarine::Bq25792::Device<I2CDriver> m_Batchg{m_BatchgI2CDriver};
		bool m_Lptim1Expired = false;
		bool m_AdcComplete = false;
		PowerState m_PowerState = PowerState::FullReset;
		std::array<uint16_t, 4> m_AdcBuffer{0};
		std::array<uint8_t, 255> m_RpiReceiveBuffer{0};
		std::array<uint8_t, Api::PacketOut::Size> m_PacketOutSerialized;
		Api::PacketOut m_PacketOut;
		bool m_I2CCommandHeaderReceived = false;
		std::chrono::milliseconds m_ShutdownDelay;

		bool InitBatteryManagers();
		void SleepWait(std::chrono::milliseconds delay, bool interruptable = false);

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
		Api::Percentage<12> GetPercentageBallast(uint16_t ballastAdc) const;
		Api::MicroVolts GetVoltageReg5(uint16_t reg5Adc) const;
		Api::MicroVolts GetVoltageRegPi(uint16_t regPiAdc) const;
		Api::MicroKelvins GetTemperature(uint16_t tempAdc) const;

		std::chrono::milliseconds GetTimestamp() const;
		void ToRtc(std::chrono::milliseconds Timestamp, RTC_TimeTypeDef &OutTime, RTC_DateTypeDef &OutDate) const;
		void SetRtc(RTC_TimeTypeDef &Time, RTC_DateTypeDef &Date);
		uint32_t Crc32(const uint8_t* data, size_t size);
		void StartAdcOneShot();

		void OnSetTimeCommand();
		void OnShutdownCommand();
	};
}

extern "C"
{
#endif
void AppMainRun(void *argument);

int IsRtcCorrect();

#ifdef __cplusplus
}
#endif
