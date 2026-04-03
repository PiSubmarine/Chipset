#include "Task.h"

namespace PiSubmarine::Chipset::Tasks
{
    std::chrono::milliseconds Task::GetUptime()
    {
        uint64_t ticks = osKernelGetTickCount();
        uint64_t freq  = osKernelGetTickFreq();

        return std::chrono::milliseconds((ticks * 1000) / freq);
    }

    std::chrono::milliseconds Task::GetRtcTime()
    {
        if (!IsRtcCorrect())
        {
            return std::chrono::milliseconds(0);
        }

        RTC_TimeTypeDef currentTime;
        RTC_DateTypeDef currentDate;
        time_t timestamp{};
        tm currTime{};

        HAL_RTC_GetTime(&hrtc, &currentTime, RTC_FORMAT_BCD);
        HAL_RTC_GetDate(&hrtc, &currentDate, RTC_FORMAT_BCD);

        currTime.tm_year = RTC_Bcd2ToByte(currentDate.Year) + 100;  // In fact: 2000 + 18 - 1900
        currTime.tm_mday = RTC_Bcd2ToByte(currentDate.Date);
        currTime.tm_mon = RTC_Bcd2ToByte(currentDate.Month) - 1;

        currTime.tm_hour = RTC_Bcd2ToByte(currentTime.Hours);
        currTime.tm_min = RTC_Bcd2ToByte(currentTime.Minutes);
        currTime.tm_sec = RTC_Bcd2ToByte(currentTime.Seconds);

        timestamp = mktime(&currTime) * 1000;
        return std::chrono::milliseconds(timestamp);
    }

    void Task::ToRtc(std::chrono::milliseconds Timestamp, RTC_TimeTypeDef& OutTime, RTC_DateTypeDef& OutDate)
    {
        time_t RawTime = Timestamp.count() / 1000;
        // uint32_t Milliseconds = Timestamp % 1000;
        tm ptm;
        gmtime_r(&RawTime, &ptm);

        OutDate.Year = RTC_ByteToBcd2(ptm.tm_year - 100);
        OutDate.Date = RTC_ByteToBcd2(ptm.tm_mday);
        OutDate.Month = RTC_ByteToBcd2(ptm.tm_mon + 1);
        OutDate.WeekDay = RTC_WEEKDAY_MONDAY;

        OutTime.Hours = RTC_ByteToBcd2(ptm.tm_hour);
        OutTime.Minutes = RTC_ByteToBcd2(ptm.tm_min);
        OutTime.Seconds = RTC_ByteToBcd2(ptm.tm_sec);
        OutTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
        OutTime.SecondFraction = 0;
        OutTime.StoreOperation = 0;
        OutTime.SubSeconds = 0;
        OutTime.TimeFormat = RTC_HOURFORMAT_24;
    }

    void Task::SetRtc(RTC_TimeTypeDef& Time, RTC_DateTypeDef& Date)
    {
        HAL_RTC_SetTime(&hrtc, &Time, RTC_FORMAT_BCD);
        HAL_RTC_SetDate(&hrtc, &Date, RTC_FORMAT_BCD);
    }

    void Task::Yield()
    {
        osThreadYield();
    }

    void Task::Delay(std::chrono::milliseconds delay) const
    {
        const uint32_t freq = osKernelGetTickFreq();

        // Convert ms → ticks (round up to avoid undersleep)
        uint32_t ticks = (delay.count() * freq + 999) / 1000;

        osDelay(ticks);
    }

    void Task::DelayUntil(std::chrono::milliseconds delayUntil) const
    {
        const uint32_t freq = osKernelGetTickFreq();

        // Convert ms → ticks (round up to avoid undersleep)
        uint32_t ticks = (delayUntil.count() * freq + 999) / 1000;

        osDelayUntil(ticks);
    }
}
