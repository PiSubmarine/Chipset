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

        /*
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
        */

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
            auto waitTicks = ToTicks(10ms);
            uint32_t flagsInt = osThreadFlagsWait(flagsAll, osFlagsWaitAny, waitTicks);

            bool noFlagSet = flagsInt & osFlagsError;
            auto flags = static_cast<WaitFlags>(flagsInt);

            if ((noFlagSet && HAL_I2C_GetState(m_I2c) == HAL_I2C_STATE_LISTEN) || HasAnyFlag(flags, WaitFlags::Listen))
            {
                // I2C is idle for a long time
                // OR
                // STOP condition, I2C will be idle in the near future
                m_RegisterStorage.Swap();
            }

            if (m_NextRegisterUpdateTime <= GetUptime())
            {
                FillRegisterStorage();
                m_NextRegisterUpdateTime = GetUptime() + UpdateInterval;
            }

            if (!noFlagSet)
            {
                SetActivityLed(false);
                m_LedEnableTime = GetUptime() + LedInterval;
            }
            else if (m_LedEnableTime <= GetUptime())
            {
                SetActivityLed(true);
            }
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
            // Abort the RX DMA that was speculatively armed in HalReceiveCompleteCallbackISR
            if (m_I2c->hdmarx != nullptr)
            {
                HAL_DMA_Abort_IT(m_I2c->hdmarx);
            }

            auto& regStorage = m_RegisterStorage.GetReadBuffer();
            // auto regStorage = m_RegisterStorage.Read();
            uint8_t* txPtr = regStorage.Data(m_Register);
            uint16_t txLen = regStorage.Size(m_Register);

            // FIX: The first byte was already preloaded into TXDR.
            // We only configure the TX DMA for the REMAINING bytes (if any).
            if (txLen > 1)
            {
                // Offset the pointer by 1 and decrease length by 1
                HAL_I2C_Slave_Seq_Transmit_DMA(m_I2c, txPtr + 1, txLen - 1, I2C_FIRST_FRAME);
            }
        }
        else // master writes
        {
            m_RegisterReceived = false;
            m_ExpectedPayloadBytes = 0;

            // Optional safety measure: Flush TXDR by setting the TXE bit in ISR
            // in case it holds stale preloaded data from an aborted read
            m_I2c->Instance->ISR |= I2C_ISR_TXE;

            HAL_I2C_Slave_Seq_Receive_DMA(m_I2c,
                reinterpret_cast<uint8_t*>(&m_Register),
                1,
                I2C_FIRST_FRAME);
        }

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
            auto& regStorage = m_RegisterStorage.GetReadBuffer();
            // auto regStorage = m_RegisterStorage.Read();
            m_ExpectedPayloadBytes = regStorage.Size(m_Register);

            // FIX: Preload the first byte into the TX Data Register immediately.
            // Because clock stretching is disabled, the HAL DMA setup in AddrCallback
            // is too slow. We must queue the first byte now while the bus is idle.
            if (m_ExpectedPayloadBytes > 0)
            {
                m_I2c->Instance->TXDR = regStorage.Data(m_Register)[0];

                // Note: If your protocol ONLY reads (Write Reg -> Repeated Start -> Read),
                // you do not need to arm the Receive DMA here.
                // However, if your protocol also supports master WRITING payload data, keep this:
                HAL_I2C_Slave_Seq_Receive_DMA(m_I2c, regStorage.Data(m_Register), static_cast<uint16_t>(m_ExpectedPayloadBytes), I2C_NEXT_FRAME);
            }
        }
        else
        {
            // Payload receive completed
            m_RegisterReceived = true;
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

    void HostProtocol::FillRegisterStorage() const
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
        m_RegisterStorage.GetWriteBuffer()[Api::Register::Status] = ToInt(status);

        auto rtc = GetRtcTime();
        auto rtcHigh = static_cast<uint32_t>(rtc.count() >> 32);
        auto rtcLow = static_cast<uint32_t>(rtc.count());
        m_RegisterStorage.GetWriteBuffer()[Api::Register::RtcHigh] = rtcHigh;
        m_RegisterStorage.GetWriteBuffer()[Api::Register::RtcLow] = rtcLow;
        m_RegisterStorage.GetWriteBuffer()[Api::Register::ChipsetVoltage] = adcMeasurements.ChipsetVoltage.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::Reg5Voltage] = adcMeasurements.Reg5Voltage.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::RegPiVoltage] = adcMeasurements.RegPiVoltage.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::BallastPosition] = adcMeasurements.BallastPosition.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::ChipsetTemperature] = adcMeasurements.ChipsetTemperature.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::ChargerTemperature] = powerStatus.ChargerTemperature.Get();

        m_RegisterStorage.GetWriteBuffer()[Api::Register::ChargerBusCurrent] = powerStatus.ChargerBusCurrent.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::BatteryCurrent] = powerStatus.BatteryCurrent.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::ChargerBusVoltage] = powerStatus.ChargerBusVoltage.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::BatteryVoltage] = powerStatus.BatteryVoltage.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::ChargerSystemVoltage] = powerStatus.ChargerSystemVoltage.Get();
        m_RegisterStorage.GetWriteBuffer()[Api::Register::BatteryTemperature] = powerStatus.BatteryTemperature.Get();
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
