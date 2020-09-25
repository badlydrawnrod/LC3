#pragma once

#include "Lc3C.h"

#include <cstdint>
#include <cstdio>
#include <variant>

class VmState
{
public:
    enum : uint32_t
    {
        isBlockedOnInput = 0x01,
        isBlockedOnOutput = 0x02
    };

    bool Run();
    void SetKey(uint16_t key) { lc3_.SetKey(key); }

    bool IsBlocked() const { return blocked_ != 0; }
    void SetBlocked(uint32_t flags) { blocked_ |= flags; }
    void ClearBlocked(uint32_t flags) { blocked_ &= ~flags; }

    void ReadImage(FILE* file);
    bool ReadImage(const char* filename);

private:
    static uint16_t Swap16(uint16_t x) { return (x << 8) | (x >> 8); }

    static bool IsRunning(const lc3::State& state) { return std::holds_alternative<lc3::Running>(state); };
    static bool IsStopped(const lc3::State& state) { return std::holds_alternative<lc3::Stopped>(state); };
    static bool IsTrapped(const lc3::State& state) { return std::holds_alternative<lc3::Trapped>(state); };

    Lc3C lc3_;            // The VM itself.
    uint32_t blocked_{0}; // Bitfields that indicate why the VM is blocked.
};
