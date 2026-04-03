#include "PiSubmarine/Chipset/Tasks/Power.h"
#include <cstdlib>

#include "Adc.h"
#include "main.h"
#include "stm32u0xx_hal_gpio.h"
#include "PiSubmarine/Bq25792/Units.h"

using namespace std::chrono_literals;

namespace PiSubmarine::Chipset::Tasks
{
    Power* Power::Instance = nullptr;

    Power& Power::GetInstance()
    {
        while (!Instance)
        {
            Yield();
        }

        return *Instance;
    }

    Power::Power(SharedState& sharedState, I2C::Stm32::Driver& chargerI2CDriver) :
        m_SharedState(sharedState),
        m_ChargerI2CDriver(chargerI2CDriver),
        m_Charger(chargerI2CDriver)
    {
        Instance = this;
    }

    void Power::Run()
    {
        using namespace RegUtils;

        Adc& adcTask = Adc::GetInstance();

        SetReg5Enabled(false);
        SetReg12Enabled(false);

        SetRegPiLedEnabled(true);
        SetReg5LedEnabled(true);
        SetReg12LedEnabled(true);

        while (!InitCharger())
        {
            Delay(1000ms);
        }

        SetReg12Enabled(true);
        while (!IsReg12PowerGood())
        {
            Delay(10ms);
        }
        SetReg12LedEnabled(false);

        SetReg5Enabled(true);
        while (adcTask.GetMeasurements().Reg5Voltage.Get() < Reg5Threshold.Get())
        {
            Delay(10ms);
        }
        SetReg5LedEnabled(false);

        while (true)
        {
            auto measurements = adcTask.GetMeasurements();
            if (measurements.Timestamp == 0ms)
            {
                Delay(10ms);
                continue;
            }

            bool domain12vGood = IsReg12PowerGood();
            bool domain5vGood = measurements.Reg5Voltage.Get() > Reg5Threshold.Get();
            bool domain3v3Good = measurements.RegPiVoltage.Get() > RegPiThreshold.Get();

            SetReg12LedEnabled(!domain12vGood);
            SetReg5LedEnabled(!domain5vGood);
            SetRegPiLedEnabled(!domain3v3Good);

            auto status0 = m_Charger.GetChargerStatus0();
            if (!status0.has_value())
            {
                Delay(100ms);
                continue;
            }

            auto chargeStatus = m_Charger.GetChargeStatus();
            if (!chargeStatus.has_value())
            {
                Delay(100ms);
                continue;
            }

            bool vbusPresent = RegUtils::HasAnyFlag(status0.value(), Bq25792::ChargerStatus0Flags::VbusPresentStat);
            bool isCharging = chargeStatus != Bq25792::ChargeStatus::NotCharging && chargeStatus != Bq25792::ChargeStatus::ChargingTerminationDone;
            bool isChargingFinished = chargeStatus == Bq25792::ChargeStatus::ChargingTerminationDone;

            PowerStatus& powerStatus = m_PowerStatus.GetWriteBuffer();
            powerStatus.StatusFlags = PowerStatus::Flags{0};
            if (domain3v3Good)
            {
                powerStatus.StatusFlags = powerStatus.StatusFlags | PowerStatus::Flags::Domain3v3Good;
            }
            if (domain5vGood)
            {
                powerStatus.StatusFlags = powerStatus.StatusFlags | PowerStatus::Flags::Domain5vGood;
            }
            if (domain12vGood)
            {
                powerStatus.StatusFlags = powerStatus.StatusFlags | PowerStatus::Flags::Domain12vGood;
            }
            if (vbusPresent)
            {
                powerStatus.StatusFlags = powerStatus.StatusFlags | PowerStatus::Flags::VbusPresent;
            }
            if (isCharging)
            {
                powerStatus.StatusFlags = powerStatus.StatusFlags | PowerStatus::Flags::ChargingOngoing;
            }
            if (isChargingFinished)
            {
                powerStatus.StatusFlags = powerStatus.StatusFlags | PowerStatus::Flags::ChargingFinished;
            }
            powerStatus.Timestamp = GetUptime();

            m_PowerStatus.Swap();

            volatile auto stack = osThreadGetStackSpace(osThreadGetId());
            (void)stack;

            Delay(100ms);
        }
    }

    bool Power::IsReg12Enabled()
    {
        return HAL_GPIO_ReadPin(REG12_EN_GPIO_Port, REG12_EN_Pin);
    }

    void Power::SetReg12Enabled(bool enabled)
    {
        HAL_GPIO_WritePin(REG12_EN_GPIO_Port, REG12_EN_Pin, static_cast<GPIO_PinState>(enabled));
    }

    bool Power::IsReg5Enabled()
    {
        return HAL_GPIO_ReadPin(REG5_EN_GPIO_Port, REG5_EN_Pin);
    }

    void Power::SetReg5Enabled(bool enabled)
    {
        HAL_GPIO_WritePin(REG5_EN_GPIO_Port, REG5_EN_Pin, static_cast<GPIO_PinState>(enabled));
    }

    void Power::SetReg12LedEnabled(bool enabled)
    {
        HAL_GPIO_WritePin(LED_REG12_GPIO_Port, LED_REG12_Pin, static_cast<GPIO_PinState>(enabled));
    }

    void Power::SetReg5LedEnabled(bool enabled)
    {
        HAL_GPIO_WritePin(LED_REG5_GPIO_Port, LED_REG5_Pin, static_cast<GPIO_PinState>(enabled));
    }

    void Power::SetRegPiLedEnabled(bool enabled)
    {
        HAL_GPIO_WritePin(LED_REGPI_GPIO_Port, LED_REGPI_Pin, static_cast<GPIO_PinState>(enabled));
    }

    bool Power::IsReg12PowerGood()
    {
        return HAL_GPIO_ReadPin(REG12_PG_GPIO_Port, REG12_PG_Pin);
    }

    bool Power::InitCharger() const
    {
        using namespace Bq25792;
        constexpr auto bqCurrent = MilliAmperes(MaxChargingCurrent.Get() / 1000);
        if (m_Charger.SetChargeCurrentLimit(bqCurrent) != ProtocolError::Ok)
        {
            return false;
        }

        return SetChargerMinimalSystemVoltage() && DisableChargerTemperatureSensor() && DisableChargerWatchdog() &&
            EnableChargerDischargeOvercurrentProtection() && DisableChargerPresetCurrentLimit() &&
            DisableChargerUsbLines();
    }

    bool Power::SetChargerMinimalSystemVoltage() const
    {
        using namespace PiSubmarine::Bq25792;
        auto minimalSystemVoltage = 12000_mV;

        if (m_Charger.SetMinimalSystemVoltage(minimalSystemVoltage) != ProtocolError::Ok)
        {
            return false;
        }

        auto read = m_Charger.GetMinimalSystemVoltage();
        return read.has_value() && read.value() == minimalSystemVoltage;
    }

    bool Power::DisableChargerTemperatureSensor() const
    {
        return m_Charger.SetTsIgnore(true) == Bq25792::ProtocolError::Ok;
    }

    bool Power::DisableChargerWatchdog() const
    {
        return m_Charger.SetWatchdog(Bq25792::Watchdog::Disable) == Bq25792::ProtocolError::Ok;
    }

    bool Power::EnableChargerDischargeOvercurrentProtection() const
    {
        return m_Charger.SetDischargeOcpEnabled(true) == Bq25792::ProtocolError::Ok;
    }

    bool Power::DisableChargerPresetCurrentLimit() const
    {
        return m_Charger.SetIlimHizCurrentLimitEnabled(false) == Bq25792::ProtocolError::Ok;
    }

    bool Power::DisableChargerUsbLines() const
    {
        return
            m_Charger.SetAutomaticDpDmDetectionEnabled(false) == Bq25792::ProtocolError::Ok &&
            m_Charger.SetDpDac(Bq25792::DpDac::HiZ) == Bq25792::ProtocolError::Ok &&
            m_Charger.SetDmDac(Bq25792::DmDac::HiZ) == Bq25792::ProtocolError::Ok;
    }
}
