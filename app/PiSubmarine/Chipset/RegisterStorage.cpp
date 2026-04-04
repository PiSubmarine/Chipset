#include "RegisterStorage.h"

#include <PiSubmarine/RegUtils.h>

namespace PiSubmarine::Chipset
{
    uint32_t& RegisterStorage::At(Api::Register reg)
    {
        return m_Registers[static_cast<size_t>(reg)];
    }

    uint32_t& RegisterStorage::operator[](Api::Register reg)
    {
        return m_Registers[static_cast<size_t>(reg)];
    }

    const uint32_t& RegisterStorage::operator[](Api::Register reg) const
    {
        return m_Registers[static_cast<size_t>(reg)];
    }

    uint8_t* RegisterStorage::Data(Api::Register reg)
    {
        return reinterpret_cast<uint8_t*>(m_Registers.data()) + RegUtils::ToInt(reg) * sizeof(RegisterType);
    }

    size_t RegisterStorage::Size(Api::Register reg) const
    {
        auto regsLeft = m_Registers.size() - RegUtils::ToInt(reg);
        return regsLeft * sizeof(RegisterType);
    }
}
