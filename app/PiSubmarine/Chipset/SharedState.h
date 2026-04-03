#pragma once

#include "cmsis_os2.h"
#include "PiSubmarine/Chipset/RegisterStorage.h"
#include "PiSubmarine/NormalizedIntFraction.h"
#include "PiSubmarine/Chipset/Units/MicroVolts.h"
#include "Units/MicroKelvins.h"

namespace PiSubmarine::Chipset
{
    class SharedState
    {
    public:
        explicit SharedState(osMutexId_t& mutex);

        void SetBallastAdc(const NormalizedIntFraction<12>& fraction);
        [[nodiscard]] NormalizedIntFraction<12> GetBallastAdc() const;

        void SetReg5Voltage(const Units::MicroVolts& voltage);
        [[nodiscard]] Units::MicroVolts GetReg5Voltage() const;

        void SetRegPiVoltage(const Units::MicroVolts& voltage);
        [[nodiscard]] Units::MicroVolts GetRegPiVoltage() const;

        void SetChipsetTemperature(const Units::MicroKelvins& temperature);
        [[nodiscard]] Units::MicroKelvins GetChipsetTemperature() const;

    private:
        osMutexId_t& m_Mutex;
        RegisterStorage m_Registers;

    };
}
