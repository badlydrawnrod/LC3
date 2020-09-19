/// A header implementation of an LC3 virtual machine.

#pragma once

#include <cstdint>
#include <variant>

namespace lc3
{
    struct Running
    {
    };
    struct Stopped
    {
    };
    struct Trapped
    {
        int trap;
    };

    using State = std::variant<Stopped, Running, Trapped>;

    /// \brief The core of an LC3 virtual machine.
    /// \tparam External a CRTP derived class that provides external access, such as memory and traps.
    ///
    /// Use CRTP to supply the ReadMem, WriteMem and Trap methods in the derived class.
    template<typename External>
    class VmCore
    {
    protected:
        // VM state. Note that memory access is implemented externally.
        const uint16_t PC_START = 0x3000;

        State state_;

        uint16_t reg_[8];       // General registers.
        uint16_t pc_{PC_START}; // Program counter.
        uint16_t cond_{0};      // Condition flags.

    public:
        /// \brief Performs a warm reset of the VM by resetting PC to the start of user memory and zeroing registers and flags.
        void Reset()
        {
            pc_ = PC_START;
            cond_ = 0;
            for (auto& reg : reg_)
            {
                reg = 0;
            }
            state_ = lc3::Running();
        }

        /// \brief Runs the VM for the given number of ticks.
        /// \param ticks the number of ticks to run for. Runs forever if negative.
        ///
        /// Ticks and instructions are currently synonymous.
        State Run(int ticks = -1)
        {
            while (std::holds_alternative<Running>(state_) && ticks != 0)
            {
                if (ticks > 0)
                {
                    --ticks;
                }

                const uint16_t instr = ReadMem(pc_++);
                const uint16_t op = instr >> 12;

                switch (op)
                {
                case OP_ADD:
                    OpAdd(instr);
                    break;

                case OP_AND:
                    OpAnd(instr);
                    break;

                case OP_NOT:
                    OpNot(instr);
                    break;

                case OP_BR:
                    OpBr(instr);
                    break;

                case OP_JMP:
                    OpJmp(instr);
                    break;

                case OP_JSR:
                    OpJsr(instr);
                    break;

                case OP_LD:
                    OpLd(instr);
                    break;

                case OP_LDI:
                    OpLdi(instr);
                    break;

                case OP_LDR:
                    OpLdr(instr);
                    break;

                case OP_LEA:
                    OpLea(instr);
                    break;

                case OP_ST:
                    OpSt(instr);
                    break;

                case OP_STI:
                    OpSti(instr);
                    break;

                case OP_STR:
                    OpStr(instr);
                    break;

                case OP_TRAP:
                    state_ = Trapped{instr};
                    break;

                case OP_RES:
                case OP_RTI:
                default:
                    state_ = Stopped();
                    break;
                }
            }

            return state_;
        }

    private:
        enum class Flags
        {
            POS = 1 << 0,  // P
            ZERO = 1 << 1, // Z
            NEG = 1 << 2,  // N
        };

        enum Opcodes
        {
            OP_BR = 0, // Branch.
            OP_ADD,    // Add.
            OP_LD,     // Load.
            OP_ST,     // Store.
            OP_JSR,    // Jump to subroutine.
            OP_AND,    // Bitwise and.
            OP_LDR,    // Load register.
            OP_STR,    // Store register.
            OP_RTI,    // Return from interrupt (not currently implemented).
            OP_NOT,    // Bitwise not.
            OP_LDI,    // Load indirect.
            OP_STI,    // Store indirect.
            OP_JMP,    // Jump.
            OP_RES,    // Reserved (unused).
            OP_LEA,    // Load effective address.
            OP_TRAP    // Invoke a trap.
        };

        // Helpers to invoke "underlying" methods.

        /// \brief Casts to the underlying External type.
        External& AsExternal() { return static_cast<External&>(*this); }

        /// \brief Reads from memory at the given address.
        /// \param address the address to read from.
        /// \return the word at the given address.
        uint16_t ReadMem(uint16_t address)
        {
            return AsExternal().ReadMem(address);
        }

        /// \brief Writes to memory at the given address.
        /// \param address the address to write to.
        /// \param val the value to write to the addres.
        void WriteMem(uint16_t address, uint16_t val)
        {
            AsExternal().WriteMem(address, val);
        }

        // Ancillary methods.

        static uint16_t SignExtend(uint16_t x, int bitCount)
        {
            if ((x >> (bitCount - 1)) & 1)
            {
                x |= (0xffff << bitCount);
            }
            return x;
        }

        void UpdateFlags(uint16_t r)
        {
            if (reg_[r] == 0)
            {
                cond_ = static_cast<uint16_t>(Flags::ZERO);
            }
            else if (reg_[r] >> 15)
            {
                cond_ = static_cast<uint16_t>(Flags::NEG);
            }
            else
            {
                cond_ = static_cast<uint16_t>(Flags::POS);
            }
        }

        // Instruction decoding.

        static bool IsImmediate(const uint16_t instr) { return (instr >> 5) & 0x1; }
        static bool IsLong(const uint16_t instr) { return (instr >> 11) & 1; }
        static uint16_t Cond(const uint16_t instr) { return (instr >> 9) & 0x7; }
        static uint16_t Dr(const uint16_t instr) { return (instr >> 9) & 0x7; }
        static uint16_t Sr1(const uint16_t instr) { return (instr >> 6) & 0x7; }
        static uint16_t Sr2(const uint16_t instr) { return instr & 0x7; }
        static uint16_t Sr(const uint16_t instr) { return (instr >> 9) & 0x7; }
        static uint16_t BaseR(const uint16_t instr) { return (instr >> 6) & 0x7; }
        static uint16_t Imm5(const uint16_t instr) { return SignExtend(instr & 0x1F, 5); }
        static uint16_t Offset6(const uint16_t instr) { return SignExtend(instr & 0x3F, 6); }
        static uint16_t PcOffset9(const uint16_t instr) { return SignExtend(instr & 0x1FF, 9); }
        static uint16_t PcOffset11(const uint16_t instr) { return SignExtend(instr & 0x7FF, 11); }

        // Opcodes.

        void OpAdd(const uint16_t instr)
        {
            const uint16_t dr = Dr(instr);
            const uint16_t sr1 = Sr1(instr);
            if (IsImmediate(instr))
            {
                const uint16_t imm5 = Imm5(instr);
                reg_[dr] = reg_[sr1] + imm5;
            }
            else
            {
                const uint16_t sr2 = Sr2(instr);
                reg_[dr] = reg_[sr1] + reg_[sr2];
            }
            UpdateFlags(dr);
        }

        void OpAnd(const uint16_t instr)
        {
            const uint16_t dr = Dr(instr);
            const uint16_t sr1 = Sr1(instr);
            if (IsImmediate(instr))
            {
                const uint16_t imm5 = Imm5(instr);
                reg_[dr] = reg_[sr1] & imm5;
            }
            else
            {
                const uint16_t sr2 = Sr2(instr);
                reg_[dr] = reg_[sr1] & reg_[sr2];
            }
            UpdateFlags(dr);
        }

        void OpBr(const uint16_t instr)
        {
            const uint16_t pcOffset9 = PcOffset9(instr);
            const uint16_t cond = Cond(instr);
            if (cond == 0 || (cond & cond_))
            {
                pc_ += pcOffset9;
            }
        }

        void OpJmp(const uint16_t instr)
        {
            const uint16_t baser = BaseR(instr);
            pc_ = reg_[baser];
        }

        void OpJsr(const uint16_t instr)
        {
            reg_[7] = pc_;
            if (IsLong(instr))
            {
                const uint16_t pcOffset11 = PcOffset11(instr);
                pc_ += pcOffset11; // JSR.
            }
            else
            {
                const uint16_t baser = BaseR(instr);
                pc_ = reg_[baser]; // JSRR.
            }
        }

        void OpLd(const uint16_t instr)
        {
            const uint16_t dr = Dr(instr);
            const uint16_t pcOffset9 = PcOffset9(instr);
            reg_[dr] = ReadMem(pc_ + pcOffset9);
            UpdateFlags(dr);
        }

        void OpLdi(const uint16_t instr)
        {
            const uint16_t dr = Dr(instr);
            const uint16_t pcOffset9 = PcOffset9(instr);
            reg_[dr] = ReadMem(ReadMem(pc_ + pcOffset9));
            UpdateFlags(dr);
        }

        void OpLdr(const uint16_t instr)
        {
            const uint16_t dr = Dr(instr);
            const uint16_t baser = BaseR(instr);
            const uint16_t offset6 = Offset6(instr);
            reg_[dr] = ReadMem(reg_[baser] + offset6);
            UpdateFlags(dr);
        }

        void OpLea(const uint16_t instr)
        {
            const uint16_t dr = Dr(instr);
            const uint16_t pcOffset9 = PcOffset9(instr);
            reg_[dr] = pc_ + pcOffset9;
            UpdateFlags(dr);
        }

        void OpNot(const uint16_t instr)
        {
            const uint16_t dr = Dr(instr);
            const uint16_t sr1 = Sr1(instr);
            reg_[dr] = ~reg_[sr1];
            UpdateFlags(dr);
        }

        void OpSt(const uint16_t instr)
        {
            const uint16_t sr = Sr(instr);
            const uint16_t pcOffset9 = PcOffset9(instr);
            WriteMem(pc_ + pcOffset9, reg_[sr]);
        }

        void OpSti(const uint16_t instr)
        {
            const uint16_t sr = Sr(instr);
            const uint16_t pcOffset9 = PcOffset9(instr);
            WriteMem(ReadMem(pc_ + pcOffset9), reg_[sr]);
        }

        void OpStr(const uint16_t instr)
        {
            const uint16_t sr = Sr(instr);
            const uint16_t baser = BaseR(instr);
            const uint16_t offset6 = Offset6(instr);
            WriteMem(reg_[baser] + offset6, reg_[sr]);
        }
    };
} // namespace lc3
