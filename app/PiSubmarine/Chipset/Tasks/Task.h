#pragma once
#include <chrono>
#include "cmsis_os2.h"
#include "PiSubmarine/Chipset/AppMain.h"
#include "main.h"
#include "rtc.h"

namespace PiSubmarine::Chipset::Tasks
{
    class Task
    {
    public:
        virtual ~Task() = default;
        static std::chrono::milliseconds GetUptime();

        template <typename Rep, typename Period>
        static size_t ToTicks(std::chrono::duration<Rep, Period> duration)
        {
            const uint64_t freq = osKernelGetTickFreq();
            // Convert input duration to nanoseconds to preserve precision
            const uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

            // ticks = duration * freq
            // duration in seconds = ns / 1e9
            // ticks = ns * freq / 1e9
            uint64_t ticks = (ns * freq + 1'000'000'000ULL - 1) / 1'000'000'000ULL; // round up

            if (ticks == 0)
                ticks = 1;

            return static_cast<size_t>(ticks);
        }

        static std::chrono::milliseconds GetRtcTime();
        static void ToRtc(std::chrono::milliseconds Timestamp, RTC_TimeTypeDef &OutTime, RTC_DateTypeDef &OutDate);
        static void SetRtc(RTC_TimeTypeDef &Time, RTC_DateTypeDef &Date);

        static void Yield();
        static void Delay(std::chrono::milliseconds);
        static void DelayUntil(std::chrono::milliseconds);

        [[noreturn]] virtual void Run() = 0;

    };
}
