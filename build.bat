@echo off
where cl >nul 2>nul
if errorlevel 1 (
    echo Error: Please run this script in VS2022 Developer Command Prompt x86
    pause
    exit /b 1
)

echo Step 1/4: Cleaning old files...
if exist obj rmdir /s /q obj
if exist bin rmdir /s /q bin
mkdir obj
mkdir bin

echo Step 2/4: Compiling ctp_trader.cpp...
cl /nologo /c /EHsc /MD /O2 /utf-8 /I. src\ctp_trader.cpp /Fo:obj\ctp_trader.obj
if errorlevel 1 goto ERROR

echo Step 3/4: Compiling main.c...
cl /nologo /c /MD /O2 /utf-8 /I. src\main.c /Fo:obj\main.obj
if errorlevel 1 goto ERROR

echo Step 4/4: Linking CTP_Trader.exe...
link /nologo /SUBSYSTEM:WINDOWS /OUT:bin\CTP_Trader.exe obj\main.obj obj\ctp_trader.obj api\traderapi\thosttraderapi_se.lib api\traderapi\thostmduserapi_se.lib user32.lib gdi32.lib comctl32.lib ws2_32.lib
if errorlevel 1 goto ERROR

echo.
echo Build completed successfully
echo Executable: bin\CTP_Trader.exe
echo.
pause
exit /b 0

:ERROR
echo.
echo Build failed - please check error messages above
echo.
pause
exit /b 1
