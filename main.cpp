#include "LC3.h"
#include "Lc3C.h"

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

    bool ReadImage(const char* filename) { return lc3_.ReadImage(filename); }

private:
    static bool IsRunning(const lc3::State& state) { return std::holds_alternative<lc3::Running>(state); };
    static bool IsStopped(const lc3::State& state) { return std::holds_alternative<lc3::Stopped>(state); };
    static bool IsTrapped(const lc3::State& state) { return std::holds_alternative<lc3::Trapped>(state); };

    Lc3C lc3_;            // The VM itself.
    uint32_t blocked_{0}; // Bitfields that indicate why the VM is blocked.
};

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

void Run(std::vector<VmState>&& vms)
{
    size_t consoleOwner = 0;
    size_t running = vms.size();
    while (running > 0)
    {
        if (_kbhit())
        {
            uint16_t key = _getch();
            // If the user pressed [Esc] then cycle console ownership to the next VM.
            if (key == '\x1b')
            {
                consoleOwner = (consoleOwner + 1) % vms.size();
                fprintf(stderr, "\nConsole owner: %zd\n", consoleOwner);
            }
            // Otherwise pass the key to the console owner and unblock it.
            else
            {
                vms[consoleOwner].SetKey(key);
                vms[consoleOwner].ClearBlocked(VmState::isBlockedOnInput);
            }
        }

        // The console owner VM can't be blocked on output.
        vms[consoleOwner].ClearBlocked(VmState::isBlockedOnOutput);

        for (auto& vm : vms)
        {
            if (!vm.Run())
            {
                // The VM just stopped.
                running--;
            }
        }
    }
}

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        printf("%s [image-file1] ...\n", argv[0]);
        exit(2);
    }

    std::vector<VmState> vms;

    for (int i = 1; i < argc; ++i)
    {
        VmState vmState;

        if (!vmState.ReadImage(argv[i]))
        {
            printf("failed to load image: %s\n", argv[i]);
            exit(1);
        }

        vms.push_back(vmState);
    }

    signal(SIGINT, HandleInterrupt);
    DisableInputBuffering();

    Run(std::move(vms));

    RestoreInputBuffering();
}
