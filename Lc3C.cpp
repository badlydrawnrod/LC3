#include "Lc3C.h"

#include <cstdint>

uint16_t Lc3C::ReadMem(uint16_t address)
{
    // Externally mapped I/O ports.
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

lc3::State Lc3C::Trap(const uint16_t instr)
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
            for (uint16_t* c = mem_ + reg_[0]; *c; c++)
            {
                char char1 = (*c) & 0xFF;
                putc(char1, stdout);
                char char2 = (*c) >> 8;
                if (char2)
                {
                    putc(char2, stdout);
                }
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

void Lc3C::ReadImage(FILE* file)
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

bool Lc3C::ReadImage(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) { return false; }
    ReadImage(file);
    fclose(file);
    return true;
}
