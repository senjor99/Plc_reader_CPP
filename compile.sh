#!/bin/bash
rem Reinitialize the build and compile it
echo "Hello from my script!"

cls
cmake -S . -B build/linux -D WITH_TAO=ON -D WITH_SNAP7=ON -D BUILD_GUI=ON
cmake --build build/linux -j
bin\linux\plc_reader
pause
