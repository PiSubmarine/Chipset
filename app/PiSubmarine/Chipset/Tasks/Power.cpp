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
            Delay(10ms);
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

        SetBatLedEnabled(true);
        SetRegPiLedEnabled(true);
        SetReg5LedEnabled(true);
        SetReg12LedEnabled(true);

        while (!InitCharger())
        {
            Delay(1000ms);
        }

        while (GetVsys().Get() < VsysThreshold.Get())
        {
            Delay(100ms);
        }

        SetBatLedEnabled(false);

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

            auto vsys = GetVsys();
            SetBatLedEnabled(vsys.Get() < VsysThreshold.Get());

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

            auto isChargerOverheating = m_Charger.IsInThermalRegulation();
            if (!isChargerOverheating.has_value())
            {
                Delay(100ms);
                continue;
            }


            auto ibus = m_Charger.GetIbusCurrent();
            if (!ibus.has_value())
            {
                Delay(100ms);
                continue;
            }

            auto ibat = m_Charger.GetIbatCurrent();
            if (!ibat.has_value())
            {
                Delay(100ms);
                continue;
            }

            auto vbus = m_Charger.GetVbusVoltage();
            if (!vbus.has_value())
            {
                SetBatLedEnabled(true);
                Delay(100ms);
                continue;
            }

            auto vbat = m_Charger.GetVbatVoltage();
            if (!vbat.has_value())
            {
                Delay(100ms);
                continue;
            }

            // TODO Battery Temperature

            auto chargerTemperature = m_Charger.GetDieTemperature();
            if (!chargerTemperature.has_value())
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
            if (isChargerOverheating.value())
            {
                powerStatus.StatusFlags = powerStatus.StatusFlags | PowerStatus::Flags::ChargerOverheat;
            }
            powerStatus.ChargerBusCurrent = Units::MicroAmperes(ibus.value().Value * 1000);
            powerStatus.BatteryCurrent = Units::MicroAmperes(ibat.value().Value * 1000);
            powerStatus.ChargerBusVoltage = Units::MicroVolts(vbus.value().Value * 1000);
            powerStatus.BatteryVoltage = Units::MicroVolts(vbat.value().Value * 1000);
            powerStatus.ChargerSystemVoltage = vsys;
            // TODO Write Battery Temperature
            powerStatus.ChargerTemperature = Units::MicroKelvins(chargerTemperature.value().Halves * 500000 + 273150000);

            // TODO Charger Overcurrent
            powerStatus.Timestamp = GetUptime();

            m_PowerStatus.Swap();

            volatile auto stack = osThreadGetStackSpace(osThreadGetId());
            (void)stack;

            Delay(100ms);
        }
    }

    Power::PowerStatus Power::GetStatus() const
    {
        return m_PowerStatus.Read();
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

    void Power::SetBatLedEnabled(bool enabled)
    {
        HAL_GPIO_WritePin(LED_BAT_GPIO_Port, LED_BAT_Pin, static_cast<GPIO_PinState>(enabled));
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
        if (!m_Charger.SetChargeCurrentLimit(bqCurrent).has_value())
        {
            return false;
        }

        return SetChargerMinimalSystemVoltage() && DisableChargerTemperatureSensor() && DisableChargerWatchdog() &&
            EnableChargerDischargeOvercurrentProtection() && EnableChargerDischargeCurrentSensing() &&
            DisableChargerPresetCurrentLimit() &&
            DisableChargerUsbLines() && EnableChargerAdc();
    }

    bool Power::SetChargerMinimalSystemVoltage() const
    {
        using namespace PiSubmarine::Bq25792;
        auto minimalSystemVoltage = 12000_mV;

        if (!m_Charger.SetMinimalSystemVoltage(minimalSystemVoltage).has_value())
        {
            return false;
        }

        auto read = m_Charger.GetMinimalSystemVoltage();
        return read.has_value() && read.value() == minimalSystemVoltage;
    }

    bool Power::DisableChargerTemperatureSensor() const
    {
        return m_Charger.SetTsIgnore(true).has_value();
    }

    bool Power::DisableChargerWatchdog() const
    {
        return m_Charger.SetWatchdog(Bq25792::Watchdog::Disable).has_value();
    }

    bool Power::EnableChargerDischargeOvercurrentProtection() const
    {
        return m_Charger.SetDischargeOcpEnabled(true).has_value();
    }

    bool Power::EnableChargerDischargeCurrentSensing() const
    {
        return m_Charger.SetDischargeCurrentSensingEnabled(true).has_value();
    }

    bool Power::DisableChargerPresetCurrentLimit() const
    {
        return m_Charger.SetIlimHizCurrentLimitEnabled(false).has_value();
    }

    bool Power::DisableChargerUsbLines() const
    {
        return
            m_Charger.SetAutomaticDpDmDetectionEnabled(false).has_value() &&
            m_Charger.SetDpDac(Bq25792::DpDac::HiZ).has_value() &&
            m_Charger.SetDmDac(Bq25792::DmDac::HiZ).has_value();
    }

    bool Power::EnableChargerAdc() const
    {
        return m_Charger.SetAdcEnabled(true).has_value() &&
            m_Charger.SetAdcSampleSpeed(Bq25792::AdcSpeed::Resolution15bits).has_value();
    }

    Units::MicroVolts Power::GetVsys() const
    {
        while (true)
        {
            auto vsys = m_Charger.GetVsysVoltage();
            if (!vsys.has_value())
            {
               return Units::MicroVolts(0);
            }
            return Units::MicroVolts(vsys.value().Value * 1000);
        }
    }
}
