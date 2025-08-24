@echo off
cls

echo All argument: %*

if "%~1"=="clean" (
    echo Cleaning build...
    rmdir /s /q build\win
    rmdir /s /q bin\win
    echo Pulizia completata. Compiling the build.
) else (
    echo  Compiling the build.
) 


cmake -S . -B build/win -D WITH_TAO=ON -D WITH_SNAP7=ON -D BUILD_GUI=ON
cmake --build build/win -j

if ERRORLEVEL == 0 (
    bin\win\plc_reader.exe
) else (
    echo Compilation error, please check the build and perform again.
)
