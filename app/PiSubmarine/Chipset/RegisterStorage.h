#pragma once

#include "PiSubmarine/Chipset/Api/Register.h"
#include <array>

namespace PiSubmarine::Chipset
{
    class RegisterStorage
    {
    public:
        uint32_t& At(Api::Register reg);

        uint32_t& operator[](Api::Register reg);
        const uint32_t& operator[](Api::Register reg) const;

    private:
        std::array<uint32_t, 9> m_Registers{};
    };
}
