@echo off
chcp 936 >nul
echo ==========================================
echo   CTP Trader - Safe Launch
echo ==========================================
echo.

if not exist "bin\CTP_Trader.exe" (
    echo [ERROR] CTP_Trader.exe not found!
    echo.
    echo Possible reasons:
    echo 1. Program was deleted by antivirus
    echo 2. Program not compiled yet
    echo.
    echo Solutions:
    echo 1. Run: add_exclusion.ps1 as Administrator
    echo 2. Run: build.ps1 to recompile
    echo 3. See ANTIVIRUS_SOLUTION.md
    echo.
    pause
    exit /b 1
)

echo Starting CTP_Trader.exe...
powershell -NoProfile -WindowStyle Hidden -Command ^
  "Start-Process -FilePath 'bin\\CTP_Trader.exe' -WorkingDirectory 'bin'"
timeout /t 2 /nobreak >nul

if not exist "bin\CTP_Trader.exe" (
    echo.
    echo [WARNING] Program deleted after launch!
    echo Please add exclusion: D:\projects\other\cexsample\bin
    pause
) else (
    echo Program started successfully!
)
