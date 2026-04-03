#include "SharedState.h"

namespace PiSubmarine::Chipset
{
    SharedState::SharedState(osMutexId_t& mutex) : m_Mutex(mutex)
    {
    }

    void SharedState::SetBallastAdc(const NormalizedIntFraction<12>& fraction)
    {
        osMutexAcquire(m_Mutex, osWaitForever);
        m_Registers[Api::Register::BallastPosition] = fraction.Get();
        osMutexRelease(m_Mutex);
    }

    NormalizedIntFraction<12> SharedState::GetBallastAdc() const
    {
        osMutexAcquire(m_Mutex, osWaitForever);
        const auto& temp = m_Registers[Api::Register::BallastPosition];
        osMutexRelease(m_Mutex);

        return NormalizedIntFraction<12>(temp);
    }

    void SharedState::SetReg5Voltage(const Units::MicroVolts& voltage)
    {
        osMutexAcquire(m_Mutex, osWaitForever);
        m_Registers[Api::Register::Reg5Voltage] = voltage.Get();
        osMutexRelease(m_Mutex);
    }

    Units::MicroVolts SharedState::GetReg5Voltage() const
    {
        osMutexAcquire(m_Mutex, osWaitForever);
        auto temp = Units::MicroVolts(m_Registers[Api::Register::Reg5Voltage]);
        osMutexRelease(m_Mutex);

        return temp;
    }

    void SharedState::SetRegPiVoltage(const Units::MicroVolts& voltage)
    {
        osMutexAcquire(m_Mutex, osWaitForever);
        m_Registers[Api::Register::RegPiVoltage] = voltage.Get();
        osMutexRelease(m_Mutex);
    }

    Units::MicroVolts SharedState::GetRegPiVoltage() const
    {
        osMutexAcquire(m_Mutex, osWaitForever);
        auto temp = Units::MicroVolts(m_Registers[Api::Register::Reg5Voltage]);
        osMutexRelease(m_Mutex);
        return temp;
    }

    void SharedState::SetChipsetTemperature(const Units::MicroKelvins& temperature)
    {
        osMutexAcquire(m_Mutex, osWaitForever);
        m_Registers[Api::Register::ChipsetTemperature] = temperature.Get();
        osMutexRelease(m_Mutex);
    }

    Units::MicroKelvins SharedState::GetChipsetTemperature() const
    {
        osMutexAcquire(m_Mutex, osWaitForever);
        auto temp = Units::MicroKelvins(m_Registers[Api::Register::ChipsetTemperature]);
        osMutexRelease(m_Mutex);
        return temp;
    }
}
