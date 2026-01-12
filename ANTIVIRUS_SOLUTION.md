# CTP_Trader.exe 被杀毒软件删除 - 解决方案

## 🔴 问题原因
Windows Defender 或其他杀毒软件将新编译的 `CTP_Trader.exe` 识别为可疑程序并自动删除。
这是**误报**（False Positive），是开发环境中的常见问题。

---

## ✅ 解决方案

### 方案1：使用自动脚本（推荐）⭐

1. **右键点击 PowerShell 图标**
2. 选择 **"以管理员身份运行"**
3. 在管理员 PowerShell 中执行：
   ```powershell
   cd D:\projects\other\cexsample
   .\add_exclusion.ps1
   ```

---

### 方案2：手动添加 Windows Defender 排除项

#### 步骤：
1. 按 `Win + I` 打开 **Windows 设置**
2. 点击 **"更新和安全"** → **"Windows 安全"**
3. 点击 **"病毒和威胁防护"**
4. 点击 **"管理设置"**
5. 滚动到 **"排除项"**，点击 **"添加或删除排除项"**
6. 点击 **"+ 添加排除项"** → 选择 **"文件夹"**
7. 浏览并选择：
   ```
   D:\projects\other\cexsample\bin
   ```
8. 点击 **"选择文件夹"**

#### 验证：
- 重新编译：`.\build.ps1`
- 运行程序：`.\bin\CTP_Trader.exe`
- 程序应该正常启动，不会被删除

---

### 方案3：使用管理员命令（快速）

在 **管理员 PowerShell** 中执行：

```powershell
Add-MpPreference -ExclusionPath "D:\projects\other\cexsample\bin"
```

验证排除项已添加：
```powershell
Get-MpPreference | Select-Object -ExpandProperty ExclusionPath
```

---

### 方案4：临时禁用实时保护（不推荐）

⚠️ **仅用于测试，不建议长期使用**

1. 打开 **Windows 安全中心**
2. **病毒和威胁防护** → **管理设置**
3. 关闭 **"实时保护"**（会在一段时间后自动重新启用）
4. 快速测试程序
5. 测试完成后记得重新启用

---

## 🔍 如何确认程序被杀毒软件删除？

### 检查 Windows Defender 日志：
```powershell
Get-MpThreatDetection | Select-Object -Last 5
```

### 检查文件是否存在：
```powershell
Test-Path "D:\projects\other\cexsample\bin\CTP_Trader.exe"
```

---

## 📋 常见问题

### Q1: 为什么新编译的程序会被误报？
**A:** 因为程序没有数字签名，且编译器生成的代码特征可能触发启发式检测。

### Q2: 这样做安全吗？
**A:** ✅ 是的，因为是你自己编译的程序。但请确保：
- 源代码是可信的
- 只排除特定的开发目录
- 不要排除整个系统目录

### Q3: 添加排除项后还是被删除怎么办？
**A:** 可能是其他杀毒软件（如360、火绒等）：
- 检查系统中安装的其他安全软件
- 在对应软件中添加信任/白名单

---

## 🎯 推荐做法

1. ✅ **添加 bin 目录到排除项**（方案1或2）
2. ✅ **保留源代码备份**
3. ✅ **使用版本控制（Git）**
4. ⚠️ **不要排除整个磁盘**
5. ⚠️ **定期更新杀毒软件规则**

---

## 📞 仍然有问题？

如果添加排除项后问题仍未解决：

1. 检查是否有多个杀毒软件冲突
2. 尝试使用 Visual Studio 开发者模式
3. 联系杀毒软件厂商报告误报

---

**最后更新时间**：2026-01-12
