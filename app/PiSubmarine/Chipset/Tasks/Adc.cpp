#include "Adc.h"
#include <adc.h>
#include "Power.h"

using namespace std::chrono_literals;

namespace PiSubmarine::Chipset::Tasks
{
    Adc* Adc::Instance = nullptr;

    Adc& Adc::GetInstance()
    {
        while (!Instance)
        {
            Delay(10ms);
        }

        return *Instance;
    }

    Adc::Adc(SharedState& sharedState) : m_SharedState(sharedState)
    {
        Instance = this;
        m_TaskHandle = osThreadGetId();
    }

    void Adc::Run()
    {
        ADC_Calibrate(&hadc1);

        auto next = GetUptime();

        while (true)
        {
            next += ScanInterval;

            HAL_ADC_Stop_DMA(&hadc1);
            HAL_ADC_Start_DMA(&hadc1, reinterpret_cast<uint32_t*>(m_AdcBuffer.data()), m_AdcBuffer.size());
            __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);
            uint32_t flags = osThreadFlagsWait(0x01, osFlagsWaitAny, ToTicks(ScanInterval));
            if (flags & osFlagsError)
            {
                // DelayUntil(next);
                continue;
            }

            m_Measurements.GetWriteBuffer().BallastPosition = GetBallastAdc();
            m_Measurements.GetWriteBuffer().ChipsetVoltage = GetAdcReferenceVoltage();
            m_Measurements.GetWriteBuffer().Reg5Voltage = GetReg5Voltage();
            m_Measurements.GetWriteBuffer().RegPiVoltage = GetRegPiVoltage();
            m_Measurements.GetWriteBuffer().ChipsetTemperature = GetMcuTemperature();
            m_Measurements.GetWriteBuffer().Timestamp = GetUptime();

            m_Measurements.Swap();

            volatile auto stack = osThreadGetStackSpace(osThreadGetId());
            (void)stack;

            DelayUntil(next);
        }
    }

    void Adc::AdcCompleteCallback() const
    {
        osThreadFlagsSet(m_TaskHandle, 0x01);
    }

    osThreadId_t Adc::GetTaskHandle() const
    {
        return m_TaskHandle;
    }

    NormalizedIntFraction<12> Adc::GetBallastAdc() const
    {
        uint16_t adc = m_AdcBuffer[0];
        return NormalizedIntFraction<12>(adc);
    }

    Units::MicroVolts Adc::GetReg5Voltage() const
    {
        auto adcRef = GetAdcReferenceVoltage();
        auto adcRef64 = static_cast<uint64_t>(adcRef.Get());
        uint64_t adcRefMult = adcRef64 * m_AdcBuffer[1];
        uint64_t adcScaled = adcRefMult / ((1ULL << 12) - 1) * 2;
        uint32_t adc32 = adcScaled;
        return Units::MicroVolts(adc32);
    }

    Units::MicroVolts Adc::GetRegPiVoltage() const
    {
        auto adcRef = GetAdcReferenceVoltage();
        auto adcRef64 = static_cast<uint64_t>(adcRef.Get());
        uint64_t adcRefMult = adcRef64 * m_AdcBuffer[2];
        uint64_t adcScaled = adcRefMult / ((1ULL << 12) - 1) * 2;
        uint32_t adc32 = adcScaled;
        return Units::MicroVolts(adc32);
    }

    Units::MicroKelvins Adc::GetMcuTemperature() const
    {
        auto vdd = GetAdcReferenceVoltage();
        volatile uint32_t vddInt = vdd.Get();
        volatile uint16_t tempAdc = m_AdcBuffer[3];
        constexpr uint16_t tsCal1Temp = 30;
        constexpr uint16_t tsCal2Temp = 130;
        constexpr uint16_t tsCalTempDelta = tsCal2Temp - tsCal1Temp;
        volatile uint16_t tsCal1 = *reinterpret_cast<uint16_t*>(0x1FFF6E68);
        volatile uint16_t tsCal2 = *reinterpret_cast<uint16_t*>(0x1FFF6E8A);
        auto tsCal1Scaled = tsCal1;
        auto tsCal2Scaled = tsCal2;
        uint16_t tsCalDelta = tsCal2Scaled - tsCal1Scaled;
        int64_t tsData = tempAdc * vddInt / 3000000;
        int64_t tsDataDelta = tsData - static_cast<int64_t>(tsCal1Scaled);
        volatile int64_t celsius = tsCalTempDelta * tsDataDelta * 1000000 / tsCalDelta + tsCal1Temp * 1000000;
        uint64_t kelvin = celsius + 273150000;
        return Units::MicroKelvins(kelvin);
    }

    Units::MicroVolts Adc::GetAdcReferenceVoltage() const
    {
        volatile uint16_t vRefInt  = m_AdcBuffer[4];

        volatile uint32_t vdda = __HAL_ADC_CALC_VREFANALOG_VOLTAGE(vRefInt, ADC_RESOLUTION_12B);
        return Units::MicroVolts(vdda * 1000);
    }

    void Adc::ADC_Calibrate(ADC_HandleTypeDef* hadc)
    {
        HAL_ADC_Stop(hadc);

        HAL_ADCEx_Calibration_Start(hadc);
    }


    Adc::Measurements Adc::GetMeasurements() const
    {
        return m_Measurements.Read();
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    PiSubmarine::Chipset::Tasks::Adc::GetInstance().AdcCompleteCallback();
}