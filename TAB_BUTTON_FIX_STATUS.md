# TAB按钮修复状态

## 问题原因
1. **消息转发问题**：所有TAB面板使用STATIC控件作为容器，而STATIC控件不会转发子控件的WM_COMMAND消息到主窗口，导致所有按钮点击无响应。
2. **坐标计算问题**：查询面板控件使用相对于TAB控件的坐标而非主窗口绝对坐标，导致控件位置错误。
3. **可见性控制问题**：查询面板控件创建时设置WS_VISIBLE，但需要由SwitchTab统一控制显示/隐藏。

## 解决方案

### 方案A：查询TAB - 直接使用主窗口
1. 将所有控件的父窗口从`g_hQueryPanel`改为`g_hMainWnd`
2. 使用主窗口绝对坐标定位控件 (20, 80) 起始
3. 创建`g_hQueryControls[]`数组跟踪所有控件
4. 控件创建时不设置WS_VISIBLE标志
5. 在`SwitchTab()`中遍历数组显示/隐藏控件

### 方案B：其他TAB - 窗口子类化
1. 保持面板容器结构不变
2. 创建`PanelProc()`窗口过程，拦截WM_COMMAND并转发到主窗口
3. 使用`SetWindowLongPtr()`为三个面板设置自定义窗口过程
4. 所有子控件的按钮点击消息自动转发到主窗口处理

## 修复进度

### ✅ 查询TAB（已完成）
- ✅ 创建了 `g_hQueryControls[]` 数组跟踪所有控件
- ✅ 所有控件父窗口改为 `g_hMainWnd`
- ✅ 修复了坐标计算为绝对坐标 (20, 80)
- ✅ 移除控件创建时的WS_VISIBLE标志
- ✅ 更新了 `SwitchTab()` 函数来显示/隐藏控件数组
- **测试结果**: 查询按钮现在可以正常响应！

### ✅ 交易TAB（已完成）
- ✅ 使用窗口子类化技术转发WM_COMMAND消息
- ✅ 交易按钮可以正常工作

### ✅ 持仓管理TAB（已完成）
- ✅ 使用窗口子类化技术
- ✅ 管理按钮可以正常工作

### ✅ 系统设置TAB（已完成）
- ✅ 使用窗口子类化技术
- ✅ 设置控件可以正常工作

## 技术亮点

**窗口子类化代码**：
```c
LRESULT CALLBACK PanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND) {
        return SendMessage(g_hMainWnd, WM_COMMAND, wParam, lParam);
    }
    return CallWindowProc(g_oldPanelProc, hWnd, message, wParam, lParam);
}
```

## 最终状态
✅ **所有TAB的按钮均已修复，可以正常工作！**

测试要点：
1. 查询TAB：所有控件正常显示和响应
2. 交易TAB：报单按钮正常工作
3. 持仓管理TAB：管理按钮正常工作
4. 系统设置TAB：设置选项正常工作
5. TAB切换正常，各面板互不干扰
