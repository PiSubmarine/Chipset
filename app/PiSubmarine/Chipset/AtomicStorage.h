#pragma once

#include "PiSubmarine/Chipset/CriticalSection.h"

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

        T& GetReadBuffer() const
        {
            return *m_BufferRead;
        }

        T& GetWriteBuffer() const
        {
            return *m_BufferWrite;
        }

        void Swap()
        {
            CriticalSection::EnterCriticalSection();

            auto tmp = m_BufferRead;
            m_BufferRead = m_BufferWrite;
            m_BufferWrite = tmp;

            CriticalSection::ExitCriticalSection();
        }

    private:
        T m_BufferA;
        T m_BufferB;
        T* m_BufferRead = &m_BufferA;
        T* m_BufferWrite = &m_BufferB;
    };
}
