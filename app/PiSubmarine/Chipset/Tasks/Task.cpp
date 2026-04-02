#include "Task.h"
#include "cmsis_os2.h"

namespace PiSubmarine::Chipset::Tasks
{
    void Task::Yield() const
    {
        osThreadYield();
    }

    void Task::Delay(std::chrono::milliseconds delay) const
    {
        osDelay(delay.count());
    }
}
