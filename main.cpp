#include "LC3.h"

#include <Windows.h>
#include <conio.h>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
    HANDLE hStdin = INVALID_HANDLE_VALUE;

    DWORD oldMode;

    void DisableInputBuffering()
    {
        hStdin = GetStdHandle(STD_INPUT_HANDLE);

        // Save the old input mode.
        GetConsoleMode(hStdin, &oldMode); /* save old mode */

        // Disable echo and line input.
        DWORD newMode = oldMode ^ ENABLE_ECHO_INPUT ^ ENABLE_LINE_INPUT;
        SetConsoleMode(hStdin, newMode);

        // Clear the input buffer.
        FlushConsoleInputBuffer(hStdin);
    }

    void RestoreInputBuffering()
    {
        SetConsoleMode(hStdin, oldMode);
    }

    void HandleInterrupt(int signal)
    {
        RestoreInputBuffering();
        printf("\n");
        exit(-2);
    }
} // namespace

class Lc3Vm : public lc3::VmCore<Lc3Vm>
{
public:
    enum class Traps
    {
        TRAP_GETC = 0x20,  // Get character from keyboard, not echoed onto the terminal.
        TRAP_OUT = 0x21,   // Output a character.
        TRAP_PUTS = 0x22,  // Output a word string.
        TRAP_IN = 0x23,    // Get character from keyboard, echoed onto the terminal.
        TRAP_PUTSP = 0x24, // Output a byte string.
        TRAP_HALT = 0x25   // Halt the program.
    };

private:
    // Memory - 65536 x 16-bit locations (i.e., not bytes).
    uint16_t mem_[65536];

    static uint16_t Swap16(uint16_t x) { return (x << 8) | (x >> 8); }

public:
    // CRTP methods to be invoked from the base class.
    void WriteMem(uint16_t address, uint16_t val) { mem_[address] = val; }
    uint16_t ReadMem(uint16_t address);

public:
    void ReadImage(FILE* file);
    bool ReadImage(const char* image_path);
    lc3::State Trap(const uint16_t instr);

    bool HasKey() const { return key_ != 0; };

    uint16_t GetKey()
    {
        auto key = key_;
        key_ = 0;
        return key;
    }

    void SetKey(uint16_t key)
    {
        key_ = key;
    }

private:
    uint16_t key_{0};
};

uint16_t Lc3Vm::ReadMem(uint16_t address)
{
    // External mapped I/O ports.
    constexpr uint16_t MR_KBSR = 0xFE00; // Keyboard status register.
    constexpr uint16_t MR_KBDR = 0xFE02; // Keyboard data register.

    if (address == MR_KBSR)
    {
        // The VM is trying to read the keyboard.
        if (HasKey())
        {
            mem_[MR_KBSR] = (1 << 15);
            mem_[MR_KBDR] = GetKey();
        }
        else
        {
            mem_[MR_KBSR] = 0;
        }
    }
    return mem_[address];
}

lc3::State Lc3Vm::Trap(const uint16_t instr)
{
    // Default back to running.
    state_ = lc3::Running();

    switch (static_cast<Traps>(instr & 0xff))
    {
    case Traps::TRAP_GETC:
        // Trap GETC - read a single character.
        reg_[0] = GetKey();
        break;

    case Traps::TRAP_OUT:
        // Trap OUT - write a single character.
        putc((char)reg_[0], stdout);
        fflush(stdout);
        break;

    case Traps::TRAP_PUTS:
        // Trap PUTS - write a character string.
        {
            /* one char per word */
            uint16_t* c = mem_ + reg_[0];
            while (*c)
            {
                putc((char)*c, stdout);
                ++c;
            }
            fflush(stdout);
        }
        break;

    case Traps::TRAP_IN:
        // Trap IN - read a single character.
        {
            printf("Enter a character: ");
            char c = GetKey();
            putc(c, stdout);
            reg_[0] = static_cast<uint16_t>(c);
        }
        break;

    case Traps::TRAP_PUTSP:
        // Trap PUTSP - write a big-endian byte-packed character string.
        {
            uint16_t* c = mem_ + reg_[0];
            while (*c)
            {
                char char1 = (*c) & 0xFF;
                putc(char1, stdout);
                char char2 = (*c) >> 8;
                if (char2) putc(char2, stdout);
                ++c;
            }
            fflush(stdout);
        }
        break;

    case Traps::TRAP_HALT:
        // Trap HALT - and catch fire.
        puts("HALT");
        fflush(stdout);
        state_ = lc3::Stopped();
        break;
    }
    return state_;
}

void Lc3Vm::ReadImage(FILE* file)
{
    // The origin tells us where in memory to place the image.
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = Swap16(origin);

    // We know the maximum file size so we only need one fread.
    uint16_t max_read = UINT16_MAX - origin;
    uint16_t* p = mem_ + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    // Swap to little endian.
    while (read-- > 0)
    {
        *p = Swap16(*p);
        ++p;
    }
}

bool Lc3Vm::ReadImage(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return false; }
    ReadImage(file);
    fclose(file);
    return true;
}

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        printf("%s [image-file1] ...\n", argv[0]);
        exit(2);
    }

    struct VmState
    {
        Lc3Vm lc3;
        bool isBlockedOnInput;
        bool isBlockedOnOutput;
    };

    std::vector<VmState> vms;

    for (int i = 1; i < argc; ++i)
    {
        VmState vmState{
                Lc3Vm(),
                false,
                false};
        vmState.lc3.Reset();

        if (!vmState.lc3.ReadImage(argv[i]))
        {
            printf("failed to load image: %s\n", argv[i]);
            exit(1);
        }

        vms.push_back(vmState);
    }

    signal(SIGINT, HandleInterrupt);
    DisableInputBuffering();

    auto IsRunning = [](const lc3::State& state) { return std::holds_alternative<lc3::Running>(state); };
    auto IsStopped = [](const lc3::State& state) { return std::holds_alternative<lc3::Stopped>(state); };
    auto IsTrapped = [](const lc3::State& state) { return std::holds_alternative<lc3::Trapped>(state); };

    constexpr size_t maxTicks = 1000;

    size_t consoleOwner = 0;

    uint16_t key{0};
    int running = vms.size();
    while (running > 0)
    {
        if (key == 0 && _kbhit())
        {
            key = _getch();
            if (key == '\x1b')  // [Esc] cycles console ownership to the next VM.
            {
                consoleOwner = (consoleOwner + 1) % vms.size();
                key = 0;
            }
        }

        size_t index = 0;
        for (auto& vm : vms)
        {
            auto& lc3 = vm.lc3;
            if (lc3::State state = lc3.GetState(); !IsStopped(state))
            {
                // If a key has been pressed and the VM currently owns the console then tell it what the key was and consume it.
                uint16_t availableKey = key;
                if (consoleOwner == index)
                {
                    if (availableKey != 0)
                    {
                        lc3.SetKey(availableKey);
                        key = 0;
                    }
                }

                if (IsTrapped(state))
                {
                    if (consoleOwner == index)
                    {
                        if (vm.isBlockedOnInput && availableKey != 0)
                        {
                            vm.isBlockedOnInput = false;
                        }
                        if (vm.isBlockedOnOutput)
                        {
                            vm.isBlockedOnOutput = false;
                        }
                        state = lc3.Trap(std::get<lc3::Trapped>(state).trap);
                    }
                }

                if (IsRunning(state))
                {
                    state = lc3.Run(maxTicks);
                    if (IsTrapped(state))
                    {
                        // The VM has become trapped, so find out what it needs to fulfil the trap, e.g., input.
                        auto& trapped = std::get<lc3::Trapped>(state);
                        switch (static_cast<Lc3Vm::Traps>(trapped.trap & 0xff))
                        {
                        case Lc3Vm::Traps::TRAP_GETC:
                        case Lc3Vm::Traps::TRAP_IN:
                            vm.isBlockedOnInput = true;
                            break;

                        case Lc3Vm::Traps::TRAP_OUT:
                        case Lc3Vm::Traps::TRAP_PUTS:
                        case Lc3Vm::Traps::TRAP_PUTSP:
                            vm.isBlockedOnOutput = true;
                            break;

                        default:
                            break;
                        }
                    }
                }

                if (IsStopped(state))
                {
                    running--;
                }
            }
            index++;
        }
    }

    RestoreInputBuffering();
}
