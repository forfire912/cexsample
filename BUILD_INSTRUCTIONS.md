# CTP Trading System - Build Instructions

## Prerequisites
- Visual Studio 2022 with C++ development tools
- Windows SDK

## How to Build

### Method 1: Using VS2022 Developer Command Prompt (Recommended)

1. Open Start Menu and search for "x86 Native Tools Command Prompt for VS 2022"
2. Navigate to project directory:
   ```
   cd /d d:\projects\other\cexsample
   ```
3. Run the compile script:
   ```
   compile.bat
   ```

### Method 2: Using PowerShell with vcvars32.bat

Run the `setup_and_build.bat` script which will automatically setup the environment and build.

## Build Output

If successful, the executable will be located at:
```
bin\CTP_Trader.exe
```

## Troubleshooting

### Error: "Please run this script in VS2022 Developer Command Prompt"
- You need to run the script in a VS2022 developer command prompt, not a regular cmd/powershell
- OR use the `setup_and_build.bat` script which sets up the environment automatically

### Encoding Issues
- All source files are in UTF-8 format
- The compile script uses `/utf-8` flag to tell MSVC to parse files as UTF-8
- This prevents encoding errors like C4819 or C2146

### Link Errors
- Make sure all DLL files in `api\traderapi\` directory are present
- Check that you're using x86 (32-bit) build tools, not x64

## Notes

- The project uses MSVC compiler with `/MD` flag (multithreaded DLL runtime)
- GUI subsystem requires: user32.lib, gdi32.lib, comctl32.lib
- CTP API requires: thosttraderapi_se.lib, thostmduserapi_se.lib, ws2_32.lib
