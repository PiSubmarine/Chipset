// HostProtocol_SeqDMA.cpp
#include "HostProtocol.h"

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
            // hdmatx pointer may be left intact by HAL; clear it to be safe
            hi2c->hdmatx = nullptr;
        }

        // Abort RX DMA if active
        if (hi2c->hdmarx != nullptr)
        {
            HAL_DMA_Abort_IT(hi2c->hdmarx);
            hi2c->hdmarx = nullptr;
        }

        // Reset HAL I2C state and error code so the peripheral can be re-used
        hi2c->State = HAL_I2C_STATE_READY;
        hi2c->ErrorCode = HAL_I2C_ERROR_NONE;

        // Clear peripheral flags that may indicate STOP/AF/ARLO etc.
        // Use the Instance pointer and the HAL macro for your device.
#if defined(I2C_FLAG_STOPF)
        __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);
#endif
#if defined(I2C_FLAG_AF)
        __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF);
#endif
#if defined(I2C_FLAG_ARLO)
        __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_ARLO);
#endif

        // Re-enable Listen mode (caller should do this after cleanup)
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

        // Initialize register storage with sample values (4 bytes each)
        m_RegisterStorage[static_cast<Api::Register>(0)] = 0x01;
        m_RegisterStorage[static_cast<Api::Register>(1)] = 0x23;
        m_RegisterStorage[static_cast<Api::Register>(2)] = 0x45;
        m_RegisterStorage[static_cast<Api::Register>(3)] = 0x67;
        m_RegisterStorage[static_cast<Api::Register>(4)] = 0x89;
        m_RegisterStorage[static_cast<Api::Register>(5)] = 0xAB;
        m_RegisterStorage[static_cast<Api::Register>(6)] = 0xCD;
        m_RegisterStorage[static_cast<Api::Register>(7)] = 0xEF;
        m_RegisterStorage[static_cast<Api::Register>(8)] = 0x01;

        // Default pointer
        m_Register = Api::Register::Status;

        Instance = this;
    }

    void HostProtocol::Run()
    {
        using namespace RegUtils;
        SetActivityLed(true);

        // Power& powerTask = Power::GetInstance();
        // Adc& adcTask = Adc::GetInstance();

        // Ensure I2C Listen mode is enabled initially
        HAL_I2C_EnableListen_IT(m_I2c);

        // State flags
        bool pointerReceived = false; // true after we've received the 1-byte pointer in the current transaction
        size_t expectedPayloadBytes = 0;

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

            auto flags = static_cast<WaitFlags>(flagsInt);

            // 1) Address match: master started a transfer
            if (HasAnyFlag(flags, WaitFlags::Address))
            {
                // m_TransferDirection set in ISR
                pointerReceived = false;
                expectedPayloadBytes = 0;

                if (m_TransferDirection == I2C_DIRECTION_TRANSMIT) // Master will write to us
                {
                    // Start sequence receive: first frame is the 1-byte register pointer
                    // Use I2C_FIRST_FRAME so HAL knows this is the beginning of a sequence
                    HAL_I2C_Slave_Seq_Receive_DMA(m_I2c, reinterpret_cast<uint8_t*>(&m_Register), 1, I2C_FIRST_FRAME);
                }
                else // Master is reading from us
                {
                    // Transmit starting at the current register pointer
                    uint8_t* txPtr = m_RegisterStorage.Data(m_Register);
                    uint16_t txLen = static_cast<uint16_t>(m_RegisterStorage.Size(m_Register));
                    // Use FIRST_FRAME for the transmit sequence
                    HAL_I2C_Slave_Seq_Transmit_DMA(m_I2c, txPtr, txLen, I2C_FIRST_FRAME);
                }
            }

            // 2) Receive complete: either pointer or payload finished
            if (HasAnyFlag(flags, WaitFlags::Receive))
            {
                if (!pointerReceived)
                {
                    // We just received the 1-byte pointer.
                    pointerReceived = true;

                    // Decide how many payload bytes we will accept for this register
                    expectedPayloadBytes = m_RegisterStorage.Size(m_Register);

                    // If master continues clocking, prepare to receive up to the register size.
                    // Use NEXT_FRAME to indicate continuation of the same sequence.
                    // If expectedPayloadBytes == 0, we don't start another DMA.
                    if (expectedPayloadBytes > 0)
                    {
                        HAL_I2C_Slave_Seq_Receive_DMA(m_I2c, m_RegisterStorage.Data(m_Register), static_cast<uint16_t>(expectedPayloadBytes), I2C_NEXT_FRAME);
                    }
                }
                else
                {
                    // Payload receive completed (or master sent fewer bytes and ended)
                    // pointerReceived remains true until STOP (ListenCplt) to allow partial writes
                    // Optionally: validate or notify other tasks about updated register
                    pointerReceived = true; // keep true until STOP
                }
            }

            // 3) Transmit complete: master finished reading (or NACK ended transfer)
            if (HasAnyFlag(flags, WaitFlags::Transmit))
            {
                // Nothing to do here; wait for STOP (ListenCplt) to re-arm listen.
            }

            // 4) Listen complete: STOP or NACK detected; transaction ended
            if (HasAnyFlag(flags, WaitFlags::Listen))
            {
                // Clean up any pending DMA and re-arm listen.
                // Abort any ongoing I2C operations to ensure peripheral returns to idle.
                // HAL_I2C_Abort_IT will trigger the HAL callbacks to stop DMA safely.
                I2CAbortAndReset(m_I2c);

                // Small delay is not required; HAL_Abort will stop DMA and clear state.
                // Re-enable listen so we can detect the next address match.
                HAL_I2C_EnableListen_IT(m_I2c);

                // Reset transient state
                pointerReceived = false;
                expectedPayloadBytes = 0;
            }

            // 5) Error handling: bus error, AF, etc.
            if (HasAnyFlag(flags, WaitFlags::Error))
            {
                // Abort any DMA and re-arm listen after cleanup
                I2CAbortAndReset(m_I2c);
                HAL_I2C_EnableListen_IT(m_I2c);

                // Reset transient state
                pointerReceived = false;
                expectedPayloadBytes = 0;
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
        using namespace RegUtils;
        m_TransferDirection = transferDirection;
        m_AddrMatchCode = addrMatchCode;
        if (!m_TaskHandle)
        {
            return;
        }
        osThreadFlagsSet(m_TaskHandle, ToInt(WaitFlags::Address));
    }

    void HostProtocol::HalListenCompleteCallbackISR()
    {
        using namespace RegUtils;
        if (!m_TaskHandle)
        {
            return;
        }
        osThreadFlagsSet(m_TaskHandle, ToInt(WaitFlags::Listen));
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
    }

    void HostProtocol::HalErrorCallbackISR()
    {
        using namespace RegUtils;
        if (!m_TaskHandle)
        {
            return;
        }
        osThreadFlagsSet(m_TaskHandle, ToInt(WaitFlags::Error));
    }

    bool HostProtocol::GetActivityLed()
    {
        return HAL_GPIO_ReadPin(LED_ACTIVITY_GPIO_Port, LED_ACTIVITY_Pin);
    }

    void HostProtocol::SetActivityLed(bool enabled)
    {
        HAL_GPIO_WritePin(LED_ACTIVITY_GPIO_Port, LED_ACTIVITY_Pin, static_cast<GPIO_PinState>(enabled));
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
