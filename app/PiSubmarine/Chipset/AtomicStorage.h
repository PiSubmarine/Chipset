#pragma once

#include "cmsis_os2.h"
#include "cmsis_gcc.h"

namespace PiSubmarine::Chipset
{
    template<typename T>
    class AtomicStorage
    {
    public:
        T Read() const
        {
            return *m_BufferRead;
        }

        T& GetWriteBuffer() const
        {
            return *m_BufferWrite;
        }

        void Swap()
        {
            uint32_t priorityMask = __get_PRIMASK();
            __disable_irq();

            auto tmp = m_BufferRead;
            m_BufferRead = m_BufferWrite;
            m_BufferWrite = tmp;

            __set_PRIMASK(priorityMask);
        }

    private:
        T m_BufferA;
        T m_BufferB;
        T* m_BufferRead = &m_BufferA;
        T* m_BufferWrite = &m_BufferB;
    };
}