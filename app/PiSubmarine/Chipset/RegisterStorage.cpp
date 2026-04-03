#include "RegisterStorage.h"

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

}
