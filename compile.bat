@echo off
chcp 936

REM ==========================================
REM   CTP Trading System Build Script (Auto Clean + Compile)
REM ==========================================

REM Check MSVC build environment
where cl >nul 2>nul
if errorlevel 1 (
    echo [Error] Please run this script in the VS2022 Developer Command Prompt (x86)!
    pause
    exit /b 1
)

echo [1/4] Cleaning old files...
if exist obj rmdir /s /q obj
if not exist obj mkdir obj
if not exist bin mkdir bin
if exist bin\CTP_Trader.exe del /q bin\CTP_Trader.exe

echo [2/4] Compiling ctp_trader.cpp...
cl /nologo /c /EHsc /MD /O2 /utf-8 /wd4828 /wd4477 /I. src\ctp_trader.cpp /Fo:obj\ctp_trader.obj
if errorlevel 1 goto ERROR

echo [3/4] Compiling main.c...
cl /nologo /c /MD /O2 /utf-8 /I. src\main.c /Fo:obj\main.obj
if errorlevel 1 goto ERROR

echo [4/4] Linking to create CTP_Trader.exe...
link /nologo /SUBSYSTEM:WINDOWS /OUT:bin\CTP_Trader.exe obj\main.obj obj\ctp_trader.obj api\traderapi\thosttraderapi_se.lib api\traderapi\thostmduserapi_se.lib user32.lib gdi32.lib comctl32.lib ws2_32.lib
if errorlevel 1 goto ERROR

echo.
echo ==========================================
echo    Build Successful! Executable: bin\CTP_Trader.exe
echo ==========================================
pause
exit /b 0

:ERROR
echo.
echo [Error] Build failed. Please check the error messages above.
pause
exit /b 1
