#pragma once
#include <PiSubmarine/Chipset/Units/MicroKelvins.h>
#include "Task.h"
#include "PiSubmarine/Chipset/SharedState.h"
#include "PiSubmarine/NormalizedIntFraction.h"
#include "PiSubmarine/Chipset/AtomicStorage.h"

namespace PiSubmarine::Chipset::Tasks
{
    class Adc : public Task
    {
    public:
        constexpr static std::chrono::milliseconds ScanInterval = std::chrono::milliseconds(10);

        struct Measurements
        {
            std::chrono::milliseconds Timestamp;
            NormalizedIntFraction<12> BallastPosition;
            Units::MicroVolts Reg5Voltage;
            Units::MicroVolts RegPiVoltage;
            Units::MicroKelvins ChipsetTemperature;
        };

        static Adc& GetInstance();

        explicit Adc(SharedState& sharedState);

        [[noreturn]] void Run() override;

        void AdcCompleteCallback() const;
        [[nodiscard]] osThreadId_t GetTaskHandle() const;

        [[nodiscard]] Measurements GetMeasurements() const;

    private:
        static Adc* Instance;

        SharedState& m_SharedState;
        osThreadId_t m_TaskHandle{};
        std::array<uint16_t, 5> m_AdcBuffer{};
        std::array<uint32_t, 4> m_ConversionBuffer{};

        AtomicStorage<Measurements> m_Measurements;

        [[nodiscard]] NormalizedIntFraction<12> GetBallastAdc() const;
        [[nodiscard]] Units::MicroVolts GetReg5Voltage() const;
        [[nodiscard]] Units::MicroVolts GetRegPiVoltage() const;
        [[nodiscard]] Units::MicroKelvins GetMcuTemperature() const;
    };
}
