# CTP Trading System Build Script (PowerShell)
# This script automatically sets up MSVC environment and builds the project

Write-Host "Setting up Visual Studio build environment..." -ForegroundColor Cyan

# Find VS installation
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    
    if ($vsPath) {
        Write-Host "Found VS at: $vsPath" -ForegroundColor Green
        
        # Setup environment (using x64 instead of x86)
        $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
        
        if (Test-Path $vcvars) {
            # Clean old files
            Write-Host "`n[1/4] Cleaning old files..." -ForegroundColor Yellow
            if (Test-Path "obj") { Remove-Item "obj" -Recurse -Force }
            if (-not (Test-Path "obj")) { New-Item -ItemType Directory -Path "obj" -Force | Out-Null }
            if (-not (Test-Path "bin")) { New-Item -ItemType Directory -Path "bin" -Force | Out-Null }
            if (Test-Path "bin\CTP_Trader.exe") { Remove-Item "bin\CTP_Trader.exe" -Force }
            if (Test-Path "ctp_debug.log") { Remove-Item "ctp_debug.log" -Force }
            if (Test-Path "bin\ctp_debug.log") { Remove-Item "bin\ctp_debug.log" -Force }
            
            # Compile ctp_trader.cpp
            Write-Host "[2/4] Compiling ctp_trader.cpp..." -ForegroundColor Yellow
            $cmd1 = "call `"$vcvars`" && cl /nologo /c /EHsc /MD /O2 /utf-8 /wd4828 /wd4477 /I. src\ctp_trader.cpp /Fo:obj\ctp_trader.obj"
            $result1 = cmd /c $cmd1 2>&1
            Write-Host $result1
            
            if ($LASTEXITCODE -ne 0) {
                Write-Host "`nERROR: Failed to compile ctp_trader.cpp" -ForegroundColor Red
                pause
                exit 1
            }
            
            # Compile main.c
            Write-Host "[3/4] Compiling main.c..." -ForegroundColor Yellow
            $cmd2 = "call `"$vcvars`" && cl /nologo /c /MD /O2 /utf-8 /I. src\main.c /Fo:obj\main.obj"
            $result2 = cmd /c $cmd2 2>&1
            Write-Host $result2
            
            if ($LASTEXITCODE -ne 0) {
                Write-Host "`nERROR: Failed to compile main.c" -ForegroundColor Red
                pause
                exit 1
            }
            
            # Link
            Write-Host "[4/4] Linking..." -ForegroundColor Yellow
            $cmd3 = "call `"$vcvars`" && link /nologo /SUBSYSTEM:WINDOWS /OUT:bin\CTP_Trader.exe obj\main.obj obj\ctp_trader.obj api\allapi\thosttraderapi_se.lib api\allapi\thostmduserapi_se.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib ole32.lib oleaut32.lib ws2_32.lib"
            $result3 = cmd /c $cmd3 2>&1
            Write-Host $result3
            
            if ($LASTEXITCODE -ne 0) {
                Write-Host "`nERROR: Failed to link" -ForegroundColor Red
                pause
                exit 1
            }
            
            # Copy DLL files to bin directory
            Write-Host "`n[5/5] Copying DLL files..." -ForegroundColor Yellow
            Copy-Item "api\allapi\thosttraderapi_se.dll" "bin\" -Force
            Copy-Item "api\allapi\thostmduserapi_se.dll" "bin\" -Force
            Write-Host "DLL files copied successfully" -ForegroundColor Green
            
            Write-Host "`n==========================================`n" -ForegroundColor Green
            Write-Host "   Build Successful!`n" -ForegroundColor Green
            Write-Host "   Executable: bin\CTP_Trader.exe`n" -ForegroundColor Green
            Write-Host "==========================================`n" -ForegroundColor Green
            pause
        } else {
            Write-Host "ERROR: vcvars32.bat not found at $vcvars" -ForegroundColor Red
            pause
            exit 1
        }
    } else {
        Write-Host "ERROR: Visual Studio installation not found" -ForegroundColor Red
        pause
        exit 1
    }
} else {
    Write-Host "ERROR: vswhere.exe not found. Please install Visual Studio 2022" -ForegroundColor Red
    pause
    exit 1
}
