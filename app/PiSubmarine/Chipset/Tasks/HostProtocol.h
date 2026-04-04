#pragma once

#include "PiSubmarine/Chipset/RegisterStorage.h"
#include "PiSubmarine/Chipset/Tasks/Task.h"

namespace PiSubmarine::Chipset::Tasks
{
    class HostProtocol : public Tasks::Task
    {
    public:
        static HostProtocol& GetInstance();
        static HostProtocol* GetInstanceISR();

        explicit HostProtocol(I2C_HandleTypeDef *hi2c);

        [[noreturn]] void Run() override;

        [[nodiscard]] I2C_HandleTypeDef* GetI2CHandle() const;
        [[nodiscard]] osThreadId_t GetTaskHandle() const;
        void HalAddrCallbackISR(uint8_t transferDirection, uint16_t addrMatchCode);
        void HalListenCompleteCallbackISR();
        void HalTransmitCompleteCallbackISR();
        void HalReceiveCompleteCallbackISR();
        void HalErrorCallbackISR();

    private:
        enum class WaitFlags : uint32_t
        {
            Address = (1 << 0),
            Listen = (1 << 1),
            Transmit = (1 << 2),
            Receive = (1 << 3),
            Error = (1 << 4)
        };

        static HostProtocol* Instance;

        static bool GetActivityLed();
        static void SetActivityLed(bool enabled);

        osThreadId_t m_TaskHandle = nullptr;
        I2C_HandleTypeDef *m_I2c = nullptr;
        uint8_t m_TransferDirection = 0;
        uint16_t m_AddrMatchCode = 0;
        Api::Register m_Register = Api::Register::Status;
        RegisterStorage m_RegisterStorage;

    };
}
