@echo off
rem Reinitialize the build and compile it
cls
cmake -S . -B build/win -D WITH_TAO=ON -D WITH_SNAP7=ON -D BUILD_GUI=ON
cmake --build build/win -j
bin\win\plc_reader.exe
pause
