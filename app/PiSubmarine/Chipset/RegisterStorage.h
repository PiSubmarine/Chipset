#pragma once

#include "PiSubmarine/Chipset/Api/Register.h"
#include <array>
#include "PiSubmarine/NormalizedIntFraction.h"

namespace PiSubmarine::Chipset
{
    class RegisterStorage
    {
    public:
        using RegisterType = uint32_t;

        RegisterType& At(Api::Register reg);

        RegisterType& operator[](Api::Register reg);
        const RegisterType& operator[](Api::Register reg) const;
        [[nodiscard]] uint8_t* Data(Api::Register reg = Api::Register::Status);
        [[nodiscard]] size_t Size(Api::Register reg = Api::Register::Status) const;

    private:
        std::array<RegisterType, static_cast<size_t>(Api::Register::Command)> m_Registers{};
    };
}
