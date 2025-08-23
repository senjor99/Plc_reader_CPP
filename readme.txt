# Profinet DCP Scanner & DB Viewer

⚡ Educational project in C++ to explore:
- **Profinet DCP** for device discovery over Ethernet.
- **TIA Portal DB/UDT parsing** with PEGTL.
- **GUI** for browsing and filtering DB structures (Dear ImGui).

---

## Features
- Discover PLC devices via Profinet DCP (using libpcap/WinPcap).
- Parse `.db` files exported from Siemens TIA Portal.
- Display DB structures (arrays, UDTs, structs) in a tree view.
- Filter values by **Name**, **Value**, or both.
- Cross-platform (Linux/Windows).

---

## Build
Requirements:
- C++20
- CMake
- libpcap / npcap
- PEGTL, Dear ImGui
all the library used can be downloaded from github.

## Status

This project is work in progress and for educational purposes only.
Expect incomplete features and debugging.

## How-to

into bin folder you can find both linux and windows compiled
file, if the code get modified for a simpler build you can find
both .bat and .sh file for launching cmake, depending on the device
you'll find the compiled executable in the following path: 

-bin/win
-bin/linux
