Process-Trace-Studio - native SDK source package, version 0.1.0

Contains the existing public user-mode C++ SDK, its local header dependencies and licenses.
This is the Process-Trace-Studio SDK, not an application installer or a kernel driver package.
Requires Windows x64, a Windows SDK and a C++17 compiler. Runtime driver/access
requirements and demo restrictions remain those of the original product.
No driver installation, activation, service creation or administrator action is performed.
Source repository: https://github.com/titanfyre/Process-Trace-Studio

Build: cmake -S . -B build -A x64
       cmake --build build --config Release
       cmake --install build --config Release --prefix install
Consume: find_package(ProcessTraceStudioSDK CONFIG REQUIRED)
         target_link_libraries(your_target PRIVATE ProcessTraceStudioSDK::sdk)
Headers preserve the original SDK layout; API signatures and license checks are unchanged.

Publication requires registry/repository ownership and a verified downloadable source archive.
Preparation does not mean acceptance into vcpkg, Conan Center or publication on NuGet.org.
