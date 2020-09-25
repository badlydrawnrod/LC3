#include "LC3.h"
#include "Lc3C.h"
#include "VmState.h"

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
