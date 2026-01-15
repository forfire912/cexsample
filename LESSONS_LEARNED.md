# 开发经验总结 (Lessons Learned)

本文档记录了在 CTP 交易系统开发过程中遇到的常见问题及解决方案。

---

## 📝 字符编码问题

### 问题现象
1. **状态栏显示乱码**：`ErrorID=42, CTP:?????? & ???`
2. **弹出窗口中的错误信息乱码**：`错误=查询错误: ErrorID=42, CTP:???????&???`
3. 所有来自 CTP API 的中文错误信息都显示为问号

### 根本原因
**CTP API 返回的字符串是 GBK/GB2312 编码，而不是 UTF-8 编码！**

- CTP API 的 `ErrorMsg`、`StatusMsg` 等字段直接返回 **GBK 编码** 的中文字符串
- 错误地将 GBK 字符串当作 UTF-8 进行转换会导致乱码
- 在 UNICODE 编译的 Windows 程序中，需要正确处理 GBK 到 UNICODE 的转换

### 错误做法 ❌
```cpp
// 错误：假设 CTP 返回的是 UTF-8，然后转换为 GBK
char* Utf8ToGbk(const char* utf8Str) {
    // 1. UTF-8 -> Unicode
    MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wideCharStr, wideCharLen);
    // 2. Unicode -> GBK
    WideCharToMultiByte(CP_ACP, 0, wideCharStr, -1, gbkStr, gbkLen, NULL, NULL);
    return gbkStr;
}
```

### 正确做法 ✅
```cpp
// 方案1：CTP 返回的就是 GBK，直接使用（如果目标也是 GBK/ANSI）
char* CopyGbkString(const char* gbkStr) {
    if (!gbkStr) return NULL;
    int len = strlen(gbkStr) + 1;
    char* result = new char[len];
    strcpy(result, gbkStr);
    return result;
}

// 方案2：在 UNICODE 程序中，将 GBK 转换为 UNICODE
void UpdateStatus(const char* msg) {
    if (g_hStatus && msg) {
        WCHAR wMsg[512];
        // 直接从 GBK (CP_ACP) 转换到 UNICODE
        MultiByteToWideChar(CP_ACP, 0, msg, -1, wMsg, 512);
        SetWindowText(g_hStatus, wMsg);
    }
}
```

### 关键点 🎯
1. **CTP API 字符串编码 = GBK (CP_ACP)**
2. **Windows ANSI API 使用 GBK**
3. **Windows UNICODE API 需要 UTF-16LE (WCHAR)**
4. **转换路径**：
   - CTP (GBK) → Windows ANSI API：**直接使用**
   - CTP (GBK) → Windows UNICODE API：**GBK → UNICODE**
5. **MessageBox 选择**：
   - ❌ `MessageBoxA` - ANSI 版本，在某些系统上可能显示不正确
   - ✅ `MessageBoxW` - UNICODE 版本，推荐使用

### 正确的 MessageBox 使用方式
```cpp
// ✅ 正确：将 GBK 字符串转换为 UNICODE 后使用 MessageBoxW
char msg[512];
sprintf(msg, "报单失败: ErrorID=%d, %s", errorID, errorMsg);

// 转换为 UNICODE
WCHAR wMsg[512], wTitle[64];
MultiByteToWideChar(CP_ACP, 0, msg, -1, wMsg, 512);
MultiByteToWideChar(CP_ACP, 0, "报单失败", -1, wTitle, 64);
MessageBoxW(NULL, wMsg, wTitle, MB_ICONERROR | MB_OK);
```

---

## 🔧 空函数实现问题

### 问题现象
- 点击"连接登录"按钮后，状态栏没有任何更新
- 明明调用了 `UpdateStatus("正在连接...")`，但界面无响应

### 根本原因
```cpp
// 空的函数实现 - 什么都不做！
void UpdateStatus(const char* msg) {
    // 更新状态栏或日志的实现
}
```

### 解决方案
```cpp
void UpdateStatus(const char* msg) {
    if (g_hStatus && msg) {
        // 将 ANSI/GBK 字符串转换为 UNICODE
        WCHAR wMsg[512];
        MultiByteToWideChar(CP_ACP, 0, msg, -1, wMsg, 512);
        SetWindowText(g_hStatus, wMsg);
    }
}
```

### 教训 📚
- **永远不要留空的函数实现**，即使是临时的
- 如果暂时不实现，至少加上日志：`printf("TODO: UpdateStatus - %s\n", msg);`
- 使用 `TODO`、`FIXME` 注释标记未完成的代码

---

## 🐛 未定义函数的链接错误

### 问题现象
```
main.obj : error LNK2019: 无法解析的外部符号 CloseAllPositions
bin\CTP_Trader.exe : fatal error LNK1120: 1 个无法解析的外部命令
```

### 根本原因
- 代码中调用了 `CloseAllPositions(g_pTrader)` 函数
- 但该函数在任何源文件中都没有实现
- 也没有在头文件中声明

### 解决方案
**方案1：实现该函数**
```cpp
// 在 ctp_trader.h 中声明
int CloseAllPositions(CTPTrader* trader);

// 在 ctp_trader.cpp 中实现
extern "C" int CloseAllPositions(CTPTrader* trader) {
    // 实现批量平仓逻辑
    return 0;
}
```

**方案2：移除调用或用提示代替**（如果功能复杂暂不实现）
```cpp
case IDC_BTN_CLOSE_ALL: {
    MessageBox(g_hMainWnd, 
        TEXT("平所有持仓功能说明：\n\n此功能需要先查询持仓，然后逐个发送平仓指令。\n\n建议：\n1. 先点击\"刷新持仓\"查看当前持仓\n2. 在持仓列表中选择要平仓的合约\n3. 手动在交易TAB中填写平仓单\n\n批量平仓功能将在后续版本中实现。"),
        TEXT("平所有持仓"),
        MB_ICONINFORMATION | MB_OK);
    break;
}
```

### 教训 📚
- **编译前检查所有函数调用都有对应实现**
- 使用 IDE 的"查找引用"功能，确认函数是否已定义
- 复杂功能可以先用 MessageBox 占位，说明功能规划

---

## 🎨 GUI 控件显示问题

### 问题现象
- 查询 TAB 界面不显示任何内容
- 持仓 TAB 中按钮点击没有反应

### 根本原因
1. **控件父窗口设置错误**
   - 控件直接以主窗口为父窗口，而不是 TAB 页的容器窗口
   
2. **WM_COMMAND 消息未转发**
   - 容器窗口未设置子类化过程，按钮消息无法传递到主窗口

3. **控件未显示**
   - 创建控件时缺少 `WS_VISIBLE` 标志
   - 或者 `SwitchTab` 函数未正确显示控件

### 解决方案
```cpp
// 1. 为每个 TAB 创建容器窗口
g_hQueryPanel = CreateWindow(TEXT("STATIC"), TEXT(""),
    WS_CHILD,  // 注意：容器窗口初始不可见，由 SwitchTab 控制
    rcTab.left + 10, rcTab.top + 10, 
    rcTab.right - rcTab.left - 20, rcTab.bottom - rcTab.top - 20,
    g_hTabControl, (HMENU)IDC_QUERY_PANEL, hInstance, NULL);

// 2. 设置子类化过程，转发 WM_COMMAND 消息
SetWindowLongPtr(g_hQueryPanel, GWLP_WNDPROC, (LONG_PTR)PanelProc);

// 3. 控件以容器窗口为父窗口
CreateWindow(TEXT("BUTTON"), TEXT("查询委托"), 
    WS_CHILD | BS_PUSHBUTTON,  // 不加 WS_VISIBLE，由 SwitchTab 控制
    x, y, 100, 30, 
    g_hQueryPanel,  // 父窗口是容器，不是主窗口
    (HMENU)IDC_BTN_QUERY_ORDER, hInstance, NULL);

// 4. 转发 WM_COMMAND 的窗口过程
LRESULT CALLBACK PanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND) {
        return SendMessage(g_hMainWnd, WM_COMMAND, wParam, lParam);
    }
    return CallWindowProc(g_oldPanelProc, hWnd, message, wParam, lParam);
}
```

### 教训 📚
- **TAB 控件的子控件应该放在容器窗口中**，不要直接放在主窗口
- **容器窗口需要设置子类化过程**，转发消息到主窗口
- **使用 `SwitchTab` 统一管理控件显示/隐藏**，不要依赖 `WS_VISIBLE`

---

## 🛠️ 开发最佳实践

### 1. 字符编码处理
```cpp
// ✅ 明确标注字符串编码
// CTP API 返回: GBK
// Windows ANSI: GBK (CP_ACP)
// Windows UNICODE: UTF-16LE (WCHAR)

// ✅ 统一的编码转换函数
WCHAR* GbkToUnicode(const char* gbkStr) {
    if (!gbkStr) return NULL;
    int len = MultiByteToWideChar(CP_ACP, 0, gbkStr, -1, NULL, 0);
    WCHAR* result = new WCHAR[len];
    MultiByteToWideChar(CP_ACP, 0, gbkStr, -1, result, len);
    return result;
}
```

### 2. 日志记录
```cpp
// ✅ 关键位置添加日志
void LogMessage(const char* msg) {
    FILE* f = fopen("ctp_debug.log", "a");
    if (f) {
        time_t now = time(NULL);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(f, "[%s] %s\n", timeStr, msg);
        fclose(f);
    }
}

// ✅ 记录重要操作
LogMessage("ConnectAndLogin called");
LogMessage(msg);
```

### 3. 错误处理
```cpp
// ✅ 完整的错误信息
if (pRspInfo && pRspInfo->ErrorID != 0) {
    char msg[512];
    sprintf(msg, "报单失败: ErrorID=%d, %s", 
            pRspInfo->ErrorID, 
            pRspInfo->ErrorMsg);  // CTP 的 ErrorMsg 是 GBK 编码
    UpdateStatus(msg);
    LogMessage(msg);
    MessageBoxA(NULL, msg, "报单失败", MB_ICONERROR | MB_OK);
}
```

### 4. 编译前检查清单
- [ ] 所有调用的函数都有实现或声明
- [ ] 字符编码转换正确（GBK ↔ UNICODE）
- [ ] 空函数实现已补全
- [ ] GUI 控件父窗口设置正确
- [ ] 消息处理和转发逻辑完整
- [ ] 添加了必要的日志记录

---

## 📋 常见错误速查表

| 问题现象 | 可能原因 | 检查项 |
|---------|---------|--------|
| 状态栏显示乱码 | 字符编码转换错误 | CTP 返回的是 GBK，不是 UTF-8 |
| 按钮点击无反应 | WM_COMMAND 未处理 | 检查容器窗口子类化、消息转发 |
| 链接错误 LNK2019 | 函数未定义 | 查找函数实现，或移除调用 |
| TAB 界面空白 | 控件父窗口错误 | 使用容器窗口，设置 SwitchTab |
| 界面留白过多 | 坐标计算错误 | 调整 x, y 起始值 |
| 编译后无变化 | 旧文件未清理 | 删除 obj/ 和 bin/ 重新编译 |

---

## 🎓 关键经验

1. **字符编码是 Windows 中文开发的头号陷阱**
   - CTP API = GBK
   - 明确每个字符串的编码
   - 在边界处正确转换

2. **空函数实现会导致难以发现的 bug**
   - 编译通过，但运行无效果
   - 立即补全或添加 TODO 日志

3. **Win32 GUI 开发要注意消息机制**
   - 父子窗口关系
   - 消息路由和转发
   - 控件可见性管理

4. **日志是调试的最佳工具**
   - 记录关键操作
   - 记录错误信息
   - 记录状态转换

5. **遇到问题先总结，避免重复踩坑**
   - 文档化解决方案
   - 建立代码模板
   - 形成开发清单

---

**最后更新**: 2026-01-15
**维护者**: Development Team
