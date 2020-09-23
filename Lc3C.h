#pragma once

#include "LC3.h"

#include <cstdint>

/// \brief An LC3 VM with a console.
class Lc3C : public lc3::Lc3Core<Lc3C>
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

    /// \brief Invoked by the CRTP base class to write to VM memory.
    void WriteMem(uint16_t address, uint16_t val) { mem_[address] = val; }

    /// \brief Invoked by the CRTP base class to read from VM memory.
    uint16_t ReadMem(uint16_t address);

    /// \brief Loads the given program image into VM memory.
    /// \param file the file to load.
    void ReadImage(FILE* file);

    /// \brief Loads the given program image file into the VM.
    /// \brief Loads the given program image into VM memory.
    /// \param filename the name of the file to load.
    bool ReadImage(const char* filename);

    /// \brief Notifies the VM that a trap can be fulfilled.
    /// \param instr the trap instruction to fulfil.
    /// \return the state of the VM after fulfilling the trap.
    lc3::State Trap(const uint16_t instr);

    /// \brief Notifies the VM that a key is available for reading.
    void SetKey(uint16_t key) { key_ = key; }

private:
    static uint16_t Swap16(uint16_t x) { return (x << 8) | (x >> 8); }

    /// \brief Called by the VM to detect if the execution environment has made a key available.
    /// Does not consume the key.
    bool HasKey() const { return key_ != 0; };

    /// \brief Called by the VM to read and consume a key set by the execution environment.
    uint16_t GetKey()
    {
        auto key = key_;
        key_ = 0;
        return key;
    }

    uint16_t mem_[65536]; // VM memory - 65536 x 16-bit locations (i.e., not bytes).
    uint16_t key_{0};     // The current key being input to the VM, or 0 if no key is available.
};
