#pragma once

#include "PiSubmarine/Chipset/Tasks/Task.h"
#include "PiSubmarine/I2C/Stm32/Driver.h"
#include "PiSubmarine/Bq25792/Device.h"
#include "PiSubmarine/Chipset/AtomicStorage.h"
#include "PiSubmarine/Chipset/SharedState.h"
#include "PiSubmarine/Chipset/Units/MicroAmperes.h"

namespace PiSubmarine::Chipset::Tasks
{
    class Power : public Task
    {
    public:
        struct PowerStatus
        {
            enum class Flags : uint8_t
            {
                Domain3v3Good = 1 << 0,
                Domain5vGood = 1 << 1,
                Domain12vGood = 1 << 2,
                VbusPresent = 1 << 3,
                ChargingOngoing = 1 << 4,
                ChargingFinished = 1 << 5,
                ChargerOvercurrent = 1 << 6,
                ChargerOverheat = 1 << 7,
            };

            std::chrono::milliseconds Timestamp{0};
            Flags StatusFlags{0};
            Units::MicroKelvins ChargerTemperature{0};
        };

        static Power& GetInstance();
        constexpr static auto Reg5Threshold = Units::MicroVolts(4900000);
        constexpr static auto RegPiThreshold = Units::MicroVolts(3200000);
        constexpr static auto MaxChargingCurrent = Units::MicroAmperes(1'000'000);

        explicit Power(SharedState& sharedState, I2C::Stm32::Driver& chargerI2CDriver);

        [[noreturn]] void Run() override;

        [[nodiscard]] PowerStatus GetStatus() const;

    private:
        static Power* Instance;

        static bool IsReg12Enabled();
        static void SetReg12Enabled(bool enabled);
        static bool IsReg5Enabled();
        static void SetReg5Enabled(bool enabled);
        static void SetReg12LedEnabled(bool enabled);
        static void SetReg5LedEnabled(bool enabled);
        static void SetRegPiLedEnabled(bool enabled);
        static bool IsReg12PowerGood();

        SharedState& m_SharedState;
        I2C::Stm32::Driver& m_ChargerI2CDriver;
        Bq25792::Device m_Charger;

        AtomicStorage<PowerStatus> m_PowerStatus;

        bool InitCharger() const;

        [[nodiscard]] bool SetChargerMinimalSystemVoltage() const;
        [[nodiscard]] bool DisableChargerTemperatureSensor() const;
        [[nodiscard]] bool DisableChargerWatchdog() const;
        [[nodiscard]] bool EnableChargerDischargeOvercurrentProtection() const;
        [[nodiscard]] bool DisableChargerPresetCurrentLimit() const;
        [[nodiscard]] bool DisableChargerUsbLines() const;
        [[nodiscard]] bool EnableChargerAdc() const;
    };
}
