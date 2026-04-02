#pragma once
#include <chrono>

namespace PiSubmarine::Chipset::Tasks
{
    class Task
    {
    protected:
        void Yield() const;
        void Delay(std::chrono::milliseconds) const;
    };
}
