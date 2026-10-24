// Add ../SDK/ProcessTrace.cpp and ../SDK to a Windows x64 C++17 project.
// Link setupapi.lib. Requires the signed driver and administrator rights.
#include "ProcessTrace.h"
#include <iostream>
int main() {
    try {
        ProcessTrace::Client trace;
        trace.Start(false); // process create/exit only; names, not full paths
        std::cout << "Read-only process telemetry for ten one-second waits.\n";
        for (int n=0; n<10; ++n) {
            auto events=trace.Receive(1000);
            for (unsigned long i=0; i<events.count; ++i)
                std::wcout << events.events[i].pid << L" "
                           << ProcessTrace::Name(events.events[i]) << L"\n";
        }
        trace.Stop();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
