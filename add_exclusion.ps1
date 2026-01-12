# ==========================================
# 添加 Windows Defender 排除项脚本
# 需要以管理员权限运行
# ==========================================

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Windows Defender 排除项添加工具" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$binPath = "D:\projects\other\cexsample\bin"
$exePath = "D:\projects\other\cexsample\bin\CTP_Trader.exe"

# 检查是否以管理员身份运行
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "❌ 错误：此脚本需要管理员权限！" -ForegroundColor Red
    Write-Host ""
    Write-Host "请按以下步骤操作：" -ForegroundColor Yellow
    Write-Host "1. 右键点击 PowerShell 图标" -ForegroundColor White
    Write-Host "2. 选择 '以管理员身份运行'" -ForegroundColor White
    Write-Host "3. 在管理员窗口中运行此脚本" -ForegroundColor White
    Write-Host ""
    Write-Host "或者手动添加排除项：" -ForegroundColor Yellow
    Write-Host "1. 打开 Windows 安全中心" -ForegroundColor White
    Write-Host "2. 病毒和威胁防护 > 管理设置" -ForegroundColor White
    Write-Host "3. 排除项 > 添加或删除排除项" -ForegroundColor White
    Write-Host "4. 添加文件夹：$binPath" -ForegroundColor Green
    Write-Host ""
    pause
    exit 1
}

Write-Host "✅ 正在以管理员权限运行..." -ForegroundColor Green
Write-Host ""

try {
    # 添加整个 bin 文件夹到排除项
    Write-Host "正在添加文件夹排除项：$binPath" -ForegroundColor Yellow
    Add-MpPreference -ExclusionPath $binPath
    Write-Host "✅ 文件夹排除项添加成功！" -ForegroundColor Green
    Write-Host ""
    
    # 验证排除项
    Write-Host "正在验证排除项..." -ForegroundColor Yellow
    $exclusions = Get-MpPreference | Select-Object -ExpandProperty ExclusionPath
    if ($exclusions -contains $binPath) {
        Write-Host "✅ 验证成功！排除项已生效。" -ForegroundColor Green
    } else {
        Write-Host "⚠️  警告：排除项可能未生效，请手动检查。" -ForegroundColor Yellow
    }
    
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host " 配置完成！现在可以安全运行程序了。" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
} catch {
    Write-Host "❌ 错误：添加排除项失败！" -ForegroundColor Red
    Write-Host "错误信息：$($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    Write-Host "请手动添加排除项：" -ForegroundColor Yellow
    Write-Host "1. 打开 Windows 安全中心" -ForegroundColor White
    Write-Host "2. 病毒和威胁防护 > 管理设置" -ForegroundColor White
    Write-Host "3. 排除项 > 添加或删除排除项" -ForegroundColor White
    Write-Host "4. 添加文件夹：$binPath" -ForegroundColor Green
    Write-Host ""
}

pause
