#include "VmState.h"

void VmState::ReadImage(FILE* file)
{
    // The origin tells us where in memory to place the image.
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = Swap16(origin);

    // We know the maximum file size so we only need one fread.
    uint16_t max_read = UINT16_MAX - origin;

    lc3_.Load(origin, max_read, [&file](uint16_t* p, uint16_t count) {
        size_t read = fread(p, sizeof(uint16_t), count, file);
        // Swap to little endian.
        while (read-- > 0)
        {
            *p = Swap16(*p);
            ++p;
        }
    });
}

bool VmState::ReadImage(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) { return false; }
    ReadImage(file);
    fclose(file);
    return true;
}

bool VmState::Run()
{
    if (lc3::State state = lc3_.GetState(); !IsStopped(state))
    {
        // If the VM is trapped and it isn't blocked then execute the trap.
        if (IsTrapped(state) && !IsBlocked())
        {
            state = lc3_.Trap(std::get<lc3::Trapped>(state).trap);
        }

        // If the VM can run then run it.
        if (IsRunning(state))
        {
            constexpr size_t maxTicks = 1000;
            state = lc3_.Run(maxTicks);
            if (IsTrapped(state))
            {
                // The VM has become trapped, so find out what it needs to fulfil the trap, e.g., input, and block it
                // until that condition is fulfilled.
                auto& trapped = std::get<lc3::Trapped>(state);
                switch (static_cast<Lc3C::Traps>(trapped.trap & 0xff))
                {
                case Lc3C::Traps::TRAP_GETC:
                case Lc3C::Traps::TRAP_IN:
                    SetBlocked(isBlockedOnInput);
                    break;

                case Lc3C::Traps::TRAP_OUT:
                case Lc3C::Traps::TRAP_PUTS:
                case Lc3C::Traps::TRAP_PUTSP:
                    SetBlocked(isBlockedOnOutput);
                    break;

                default:
                    break;
                }
            }
        }

        return !IsStopped(state);
    }

    return true;
}
