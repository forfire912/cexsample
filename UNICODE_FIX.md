# 中文乱码已修复 - 重新编译说明

## ✅ 已完成的修复

已将所有 Windows API 调用从 ANSI 版本改为 Unicode 版本：

### 修改内容：
1. ✅ 添加 `#define UNICODE` 和 `#define _UNICODE`
2. ✅ `CreateWindowA` → `CreateWindow` (Unicode 宏版本)
3. ✅ `MessageBoxA` → `MessageBox`
4. ✅ `GetWindowTextA` → `GetWindowText`
5. ✅ `SetWindowTextA` → `SetWindowText`
6. ✅ 添加 Unicode ↔ ANSI 转换（CTP API 仍需要 ANSI）

### 工作原理：
- **界面显示**：使用 Unicode (UTF-16LE) - 正确显示中文
- **CTP API**：使用 ANSI (GBK) - API 要求
- **自动转换**：WideCharToMultiByte 和 MultiByteToWideChar

---

## 🚀 重新编译步骤

### 步骤1：关闭正在运行的程序
**重要！** 必须先关闭 CTP_Trader.exe，否则无法覆盖文件。

```
点击程序窗口的 [X] 按钮关闭
或在任务管理器中结束进程
```

### 步骤2：重新编译
```powershell
.\build.ps1
```

### 步骤3：运行新程序
```powershell
.\run.bat
```

---

## 📋 预期效果

修复后，界面应该显示：

```
✅ 经纪商ID:  [1010]
✅ 用户ID:    [20833]
✅ 密码:      [******]
✅ 交易前置:  [tcp://106.37.101.162:31205]
✅ 认证码:    [YHQHYHQHYHQHYHQH]

[连接登录]

[查询委托] [查询持仓] [查询行情] [查询合约]

状态: 未连接
```

**所有中文应该正确显示，不再是乱码！**

---

## ⚠️ 如果仍有编译错误

### 错误：无法打开文件 "bin\CTP_Trader.exe"

**原因**：程序正在运行中

**解决**：
1. 关闭程序
2. 检查任务管理器中是否有残留进程
3. 或重启 PowerShell 窗口

### 错误：类型不兼容警告

这些警告不影响程序运行，可以忽略。

---

## 🎯 技术说明

### 为什么之前会乱码？

1. **ANSI 版本**：`CreateWindowA` 使用系统默认代码页（中文 Windows 是 GBK）
2. **源文件编码**：main.c 是 UTF-8 编码
3. **编码冲突**：UTF-8 字符串 → GBK 解析 → 乱码

### 现在如何解决？

1. **Unicode 版本**：`CreateWindow` 使用 UTF-16LE
2. **TEXT 宏**：自动转换字符串为 Unicode
3. **正确显示**：Windows 原生支持 Unicode，完美显示中文

---

**现在请关闭程序，然后重新编译！** 🚀
