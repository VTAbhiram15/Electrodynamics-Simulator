@echo off
cls

for /F "delims=" %%A in ('echo prompt $E^|cmd') do set "ESC=%%A"
set "GREEN=%ESC%[92m"
set "RED=%ESC%[91m"
set "YELLOW=%ESC%[93m" 
set "RESET=%ESC%[0m"

echo ===================================================
echo   %YELLOW%Compiling Electrosim... Please wait...%RESET%
echo ===================================================

:: --- CONFIGURATION (Put your real paths here. DO NOT include trailing slashes) ---
set SFML_INC=C:\SFML\include
set SFML_LIB=C:\SFML\lib

:: Run g++ directly. Standard outputs and errors will print naturally.
g++ -O3 -std=c++20 -flto=auto -fopenmp -I"%SFML_INC%" -L"%SFML_LIB%" src/main.cpp -o bin/Electrosim.exe -lsfml-graphics -lsfml-window -lsfml-system

:: Check the exact exit code of the compiler
if %ERRORLEVEL% equ 0 (
    cls
    echo ===================================================
    echo   %GREEN%[SUCCESS] Build complete! Launching simulation...%RESET%
    echo ===================================================
    echo.
    .\bin\Electrosim.exe
) else (
    echo.
    echo ===================================================
    echo   %RED%[ERROR] Compilation failed. Fix the errors above.%RESET%
    echo ===================================================
)