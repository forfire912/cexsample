@echo off
REM This script automatically sets up VS2022 build environment and compiles the project

echo Setting up Visual Studio 2022 build environment...

REM Try common VS2022 installation paths
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALLDIR=%%i"
    )
)

if not defined VSINSTALLDIR (
    echo ERROR: Could not find Visual Studio 2022 installation
    echo Please install Visual Studio 2022 with C++ development tools
    pause
    exit /b 1
)

REM Setup x86 build environment
if exist "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars32.bat" (
    call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars32.bat"
) else (
    echo ERROR: Could not find vcvars32.bat
    echo VS Installation: %VSINSTALLDIR%
    pause
    exit /b 1
)

echo.
echo Environment setup complete. Starting build...
echo.

REM Now run the compile script
call compile.bat
