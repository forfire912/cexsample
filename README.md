# CTP Trading System

A Windows GUI application for CTP (Comprehensive Transaction Platform) trading operations.

## ⚠️ Important: Antivirus False Positive Warning

**Windows Defender or other antivirus software may delete the compiled `CTP_Trader.exe` as a false positive.**

### Quick Fix:
1. **Add exclusion** to Windows Defender:
   - Run as Administrator: `.\add_exclusion.ps1`
   - Or manually add folder: `D:\projects\other\cexsample\bin`
   
2. **Recompile**: `.\build.ps1`

3. **Run**: `.\run.bat`

📖 **See [ANTIVIRUS_SOLUTION.md](ANTIVIRUS_SOLUTION.md) for detailed solutions.**

---

## Project Structure

```
cexsample/
├── api/                      # CTP C API libraries and headers
│   ├── doc/                 # API documentation (PDF)
│   └── traderapi/           # Header files and static libraries
│       ├── *.h              # C/C++ header files
│       └── *.lib            # Static link libraries
├── bin/                      # Executable and runtime DLLs
│   ├── CTP_Trader.exe       # Main executable
│   └── *.dll                # Required runtime DLLs
├── src/                      # Source code
│   ├── main.c               # GUI application entry point
│   ├── ctp_trader.cpp       # CTP API wrapper
│   └── ctp_trader.h         # Header file
├── req/                      # Requirements and documentation
│   ├── 当日持仓.csv          # Sample position data
│   ├── 当日委托.csv          # Sample order data
│   ├── 商品参数.xlsx         # Instrument parameters
│   └── 商品行情.xlsx         # Market data
├── build.ps1                 # PowerShell build script (recommended)
├── compile.bat               # Batch build script
├── BUILD_SUCCESS.md          # Build report
└── README.md                 # This file
```

## Features

- **Windows GUI**: Native Win32 API-based interface
- **CTP Trading API**: Connect to CTP trading servers
- **Query Functions**:
  - Order queries (当日委托)
  - Position queries (当日持仓)
  - Market data queries (商品行情)
  - Instrument queries (商品参数)
- **Real-time Updates**: Status bar with connection and query status

## Requirements

- **OS**: Windows 10/11 (64-bit)
- **Compiler**: Visual Studio 2022 with C++ development tools
- **CTP API**: Version 1.7.3-P2 (included in `api/` directory)

## Build Instructions

### Method 1: PowerShell Script (Recommended)

Simply run the PowerShell build script:

```powershell
.\build.ps1
```

This script will:
- Automatically detect VS2022 installation
- Set up x64 build environment
- Clean old build files
- Compile and link the project

### Method 2: Batch File

1. Open **"x64 Native Tools Command Prompt for VS 2022"** from Start Menu
2. Navigate to project directory:
   ```cmd
   cd /d d:\projects\other\cexsample
   ```
3. Run the build script:
   ```cmd
   compile.bat
   ```

## Running the Application

After successful build, the executable is located at `bin\CTP_Trader.exe`.

All required DLLs are already copied to the `bin\` directory, so you can run it directly:

```cmd
.\bin\CTP_Trader.exe
```

Or simply double-click `bin\CTP_Trader.exe` in Windows Explorer.

## Configuration

Default connection parameters are in `src/main.c`:

```c
const char* BROKER_ID = "1010";
const char* USER_ID = "20833";
const char* PASSWORD = "******";
const char* FRONT_ADDR = "tcp://106.37.101.162:31213";
const char* AUTH_CODE = "YHQHYHQHYHQHYHQH";
```

Modify these values according to your CTP account settings.

## Technical Details

- **Language**: C/C++ hybrid
- **GUI Framework**: Win32 API
- **Architecture**: x64 (64-bit)
- **Compiler**: MSVC v18.1.1+ 
- **Encoding**: UTF-8 with `/utf-8` flag
- **Runtime**: Multithreaded DLL (`/MD`)

## Build Configuration

### Compiler Flags
- **C++**: `/EHsc /MD /O2 /utf-8`
- **C**: `/MD /O2 /utf-8`

### Linker Settings
- Subsystem: Windows GUI
- Libraries: `user32.lib gdi32.lib comctl32.lib ws2_32.lib`
- CTP Libraries: `thosttraderapi_se.lib thostmduserapi_se.lib`

## Troubleshooting

### Build Errors

If you encounter encoding errors (C4819, C2146), ensure:
1. All source files are UTF-8 encoded
2. Using the `/utf-8` compiler flag
3. Running in VS2022 x64 Developer Command Prompt

### Runtime Errors

If the program fails to start:
1. Ensure all DLL files are in `bin\` directory
2. Check that you're using the x64 version
3. Verify CTP server address and credentials

## License

This is a sample project for educational purposes.

## References

- [CTP API Documentation](api/doc/SFIT_CTP_Mini_API_V1.7.3-P2.pdf)
- Build report: [BUILD_SUCCESS.md](BUILD_SUCCESS.md)
