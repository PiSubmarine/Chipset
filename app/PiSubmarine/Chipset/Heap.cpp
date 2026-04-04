#include <cstdlib>
#include "FreeRTOS.h"
#include "portable.h"

// Redirect C++ new to FreeRTOS pvPortMalloc
void* operator new(std::size_t size) {
    return pvPortMalloc(size);
}

void* operator new[](std::size_t size) {
    return pvPortMalloc(size);
}

// Redirect C++ delete to FreeRTOS vPortFree
void operator delete(void* ptr) noexcept {
    vPortFree(ptr);
}

void operator delete[](void* ptr) noexcept {
    vPortFree(ptr);
}

// Add this to handle sized array deallocation (C++14/17)
void operator delete[](void* ptr, std::size_t size) noexcept {
    (void)size; // FreeRTOS doesn't need the size, so we silence the unused param warning
    vPortFree(ptr);
}

// Optional: for C++14/17 sized deallocation
void operator delete(void* ptr, std::size_t size) noexcept {
    (void)size;
    vPortFree(ptr);
}