// HostProtocol_SeqDMA.cpp
#include "HostProtocol.h"

#include <PiSubmarine/Chipset/Api/Status.h>

#include "Adc.h"
#include "Power.h"

using namespace std::chrono_literals;

namespace
{
    void I2CAbortAndReset(I2C_HandleTypeDef* hi2c)
    {
        // Abort TX DMA if active
        if (hi2c->hdmatx != nullptr)
        {
            // HAL_DMA_Abort_IT will call the DMA abort callback and stop the stream
            HAL_DMA_Abort_IT(hi2c->hdmatx);
        }

        // Abort RX DMA if active
        if (hi2c->hdmarx != nullptr)
        {
            HAL_DMA_Abort_IT(hi2c->hdmarx);
        }

        HAL_I2C_DisableListen_IT(hi2c);
    }
}

namespace PiSubmarine::Chipset::Tasks
{
    HostProtocol* HostProtocol::Instance = nullptr;

    HostProtocol& HostProtocol::GetInstance()
    {
        while (!Instance)
        {
            Delay(10ms);
        }
        return *Instance;
    }

    HostProtocol* HostProtocol::GetInstanceISR()
    {
        return Instance;
    }

    HostProtocol::HostProtocol(I2C_HandleTypeDef* hi2c) : m_I2c(hi2c)
    {
        m_TaskHandle = osThreadGetId();

        m_RegisterStorage[static_cast<Api::Register>(0)] = 0xABABABAB;
        m_RegisterStorage[static_cast<Api::Register>(1)] = 0xABABABAB;
        m_RegisterStorage[static_cast<Api::Register>(2)] = 0xABABABAB;
        m_RegisterStorage[static_cast<Api::Register>(3)] = 0xABABABAB;
        m_RegisterStorage[static_cast<Api::Register>(4)] = 0xABABABAB;
        m_RegisterStorage[static_cast<Api::Register>(5)] = 0xABABABAB;
        m_RegisterStorage[static_cast<Api::Register>(6)] = 0xABABABAB;
        m_RegisterStorage[static_cast<Api::Register>(7)] = 0xABABABAB;
        m_RegisterStorage[static_cast<Api::Register>(8)] = 0xABABABAB;

        // Default pointer
        m_Register = Api::Register::Status;

        Instance = this;
    }

    void HostProtocol::Run()
    {
        using namespace RegUtils;
        SetActivityLed(true);

        // Ensure I2C Listen mode is enabled initially
        HAL_I2C_EnableListen_IT(m_I2c);

        while (true)
        {
            uint32_t flagsAll = ToInt(WaitFlags::Address | WaitFlags::Listen | WaitFlags::Transmit | WaitFlags::Receive | WaitFlags::Error);
            auto waitTicks = ToTicks(500ms);
            uint32_t flagsInt = osThreadFlagsWait(flagsAll, osFlagsWaitAny, waitTicks);
            SetActivityLed(!GetActivityLed());

            if (flagsInt & osFlagsError)
            {
                continue;
            }

            // auto flags = static_cast<WaitFlags>(flagsInt);

        }
    }

    I2C_HandleTypeDef* HostProtocol::GetI2CHandle() const
    {
        return m_I2c;
    }

    osThreadId_t HostProtocol::GetTaskHandle() const
    {
        return m_TaskHandle;
    }

    void HostProtocol::HalAddrCallbackISR(uint8_t transferDirection, uint16_t addrMatchCode)
    {
        m_TransferDirection = transferDirection;
        m_AddrMatchCode = addrMatchCode;

        if (transferDirection == I2C_DIRECTION_RECEIVE) // master reads
        {
            uint8_t* txPtr = m_RegisterStorage.Data(m_Register);
            uint16_t txLen = m_RegisterStorage.Size(m_Register);

            HAL_I2C_Slave_Seq_Transmit_DMA(m_I2c, txPtr, txLen, I2C_FIRST_FRAME);
        }
        else
        {
            m_RegisterReceived = false;
            m_ExpectedPayloadBytes = 0;

            HAL_I2C_Slave_Seq_Receive_DMA(m_I2c,
                reinterpret_cast<uint8_t*>(&m_Register),
                1,
                I2C_FIRST_FRAME);
        }

        // optional: still notify task for bookkeeping
        osThreadFlagsSet(m_TaskHandle, RegUtils::ToInt(WaitFlags::Address));
    }

    void HostProtocol::HalListenCompleteCallbackISR()
    {
        using namespace RegUtils;
        if (!m_TaskHandle)
        {
            return;
        }
        osThreadFlagsSet(m_TaskHandle, ToInt(WaitFlags::Listen));

        // Clean up any pending DMA and re-arm listen.
        // Abort any ongoing I2C operations to ensure peripheral returns to idle.
        // HAL_I2C_Abort_IT will trigger the HAL callbacks to stop DMA safely.
        I2CAbortAndReset(m_I2c);

        // Small delay is not required; HAL_Abort will stop DMA and clear state.
        // Re-enable listen so we can detect the next address match.
        HAL_I2C_EnableListen_IT(m_I2c);

        // Reset transient state
        m_RegisterReceived = false;
        m_ExpectedPayloadBytes = 0;
    }

    void HostProtocol::HalTransmitCompleteCallbackISR()
    {
        using namespace RegUtils;
        if (!m_TaskHandle)
        {
            return;
        }
        osThreadFlagsSet(m_TaskHandle, ToInt(WaitFlags::Transmit));
    }

    void HostProtocol::HalReceiveCompleteCallbackISR()
    {
        using namespace RegUtils;
        if (!m_TaskHandle)
        {
            return;
        }
        osThreadFlagsSet(m_TaskHandle, ToInt(WaitFlags::Receive));

        if (!m_RegisterReceived)
        {
            // We just received the 1-byte pointer.
            m_RegisterReceived = true;

            // Decide how many payload bytes we will accept for this register
            m_ExpectedPayloadBytes = m_RegisterStorage.Size(m_Register);

            // If master continues clocking, prepare to receive up to the register size.
            // Use NEXT_FRAME to indicate continuation of the same sequence.
            // If expectedPayloadBytes == 0, we don't start another DMA.
            if (m_ExpectedPayloadBytes > 0)
            {
                HAL_I2C_Slave_Seq_Receive_DMA(m_I2c, m_RegisterStorage.Data(m_Register), static_cast<uint16_t>(m_ExpectedPayloadBytes), I2C_NEXT_FRAME);
            }
        }
        else
        {
            // Payload receive completed (or master sent fewer bytes and ended)
            // pointerReceived remains true until STOP (ListenCplt) to allow partial writes
            // Optionally: validate or notify other tasks about updated register
            m_RegisterReceived = true; // keep true until STOP
        }
    }

    void HostProtocol::HalErrorCallbackISR()
    {
        using namespace RegUtils;
        if (!m_TaskHandle)
        {
            return;
        }
        osThreadFlagsSet(m_TaskHandle, ToInt(WaitFlags::Error));

        m_RegisterReceived = false;
        m_ExpectedPayloadBytes = 0;
    }

    bool HostProtocol::GetActivityLed()
    {
        return HAL_GPIO_ReadPin(LED_ACTIVITY_GPIO_Port, LED_ACTIVITY_Pin);
    }

    void HostProtocol::SetActivityLed(bool enabled)
    {
        HAL_GPIO_WritePin(LED_ACTIVITY_GPIO_Port, LED_ACTIVITY_Pin, static_cast<GPIO_PinState>(enabled));
    }

    void HostProtocol::FillRegisterStorage()
    {
        using namespace RegUtils;
        Power& powerTask = Power::GetInstance();
        Adc& adcTask = Adc::GetInstance();
        auto powerStatus = powerTask.GetStatus();
        auto adcMeasurements = adcTask.GetMeasurements();

        Api::Status status{0};

        status = status | Api::Status::TestBit;
        if (HasAnyFlag(powerStatus.StatusFlags, Power::PowerStatus::Flags::VbusPresent))
        {
            status = status | Api::Status::VBusConnected;
        }
        if (HasAnyFlag(powerStatus.StatusFlags, Power::PowerStatus::Flags::ChargingOngoing))
        {
            status = status | Api::Status::ChargingInProgress;
        }
        if (HasAnyFlag(powerStatus.StatusFlags, Power::PowerStatus::Flags::ChargerOvercurrent))
        {
            status = status | Api::Status::ChargerOvercurrent;
        }
        if (HasAnyFlag(powerStatus.StatusFlags, Power::PowerStatus::Flags::ChargerOverheat))
        {
            status = status | Api::Status::ChargerOverheat;
        }
        if (HasAnyFlag(powerStatus.StatusFlags, Power::PowerStatus::Flags::Domain12vGood))
        {
            status = status | Api::Status::Reg12PowerGood;
        }
        m_RegisterStorage[Api::Register::Status] = ToInt(status);

        auto rtc = GetRtcTime();
        auto rtcHigh = static_cast<uint32_t>(rtc.count() >> 32);
        auto rtcLow = static_cast<uint32_t>(rtc.count());
        m_RegisterStorage[Api::Register::RtcHigh] = rtcHigh;
        m_RegisterStorage[Api::Register::RtcLow] = rtcLow;
        m_RegisterStorage[Api::Register::Reg5Voltage] = adcMeasurements.Reg5Voltage.Get();
        m_RegisterStorage[Api::Register::RegPiVoltage] = adcMeasurements.RegPiVoltage.Get();
        m_RegisterStorage[Api::Register::BallastPosition] = adcMeasurements.BallastPosition.Get();
        m_RegisterStorage[Api::Register::ChipsetTemperature] = adcMeasurements.ChipsetTemperature.Get();
        m_RegisterStorage[Api::Register::ChargerTemperature] = powerStatus.ChargerTemperature.Get();
    }
}

// HAL callback wrappers (C linkage)
extern "C" {

void HAL_I2C_AddrCallback(I2C_HandleTypeDef* hi2c, uint8_t transferDirection, uint16_t addrMatchCode)
{
    auto* instance = PiSubmarine::Chipset::Tasks::HostProtocol::GetInstanceISR();
    if (!instance)
    {
        return;
    }
    if (instance->GetI2CHandle() != hi2c)
    {
        return;
    }
    instance->HalAddrCallbackISR(transferDirection, addrMatchCode);
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef* hi2c)
{
    auto* instance = PiSubmarine::Chipset::Tasks::HostProtocol::GetInstanceISR();
    if (!instance)
    {
        return;
    }
    if (instance->GetI2CHandle() != hi2c)
    {
        return;
    }
    instance->HalListenCompleteCallbackISR();
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    auto* instance = PiSubmarine::Chipset::Tasks::HostProtocol::GetInstanceISR();
    if (!instance)
    {
        return;
    }
    if (instance->GetI2CHandle() != hi2c)
    {
        return;
    }
    instance->HalTransmitCompleteCallbackISR();
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    auto* instance = PiSubmarine::Chipset::Tasks::HostProtocol::GetInstanceISR();
    if (!instance)
    {
        return;
    }
    if (instance->GetI2CHandle() != hi2c)
    {
        return;
    }
    instance->HalReceiveCompleteCallbackISR();
}

} // extern "C"
