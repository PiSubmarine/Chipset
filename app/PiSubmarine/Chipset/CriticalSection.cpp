#include "PiSubmarine/Chipset/CriticalSection.h"
#include "FreeRTOS.h"
#include <task.h>
#include "cmsis_os2.h"
#include "portmacro.h"


extern "C"
{
    void EnterCriticalSectionInternal()
    {
        taskENTER_CRITICAL();
    }

    void ExitCriticalSectionInternal()
    {
        taskEXIT_CRITICAL();
    }
}

namespace PiSubmarine::Chipset::CriticalSection
{
    void EnterCriticalSection()
    {
        EnterCriticalSectionInternal();
    }

    void ExitCriticalSection()
    {
        ExitCriticalSectionInternal();
    }
}

