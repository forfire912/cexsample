# CTP 期货交易系统

基于 CTP API 的 Windows GUI 期货交易应用程序，支持交易和实时行情订阅功能。

## 快速开始

### 1. 编译项目
```powershell
.\build.ps1
```

### 2. 运行程序
```powershell
.\run.bat
```

### 3. 使用说明

#### 交易功能
1. 输入券商代码、用户名、密码和交易前置地址
2. 点击"连接并登录"
3. 登录成功后可以：
   - 查询合约列表
   - 查询持仓信息
   - 查询当日委托

#### 行情功能
1. 输入行情前置地址（通常与交易前置不同端口）
2. 点击"连接行情"
3. 从合约列表中选择合约（双击或选中自动填充）
4. 点击"订阅行情"查看实时行情

**⚠️ 注意**：行情数据仅在交易时段推送
- 白盘：09:00-15:00（周一至周五）
- 夜盘：21:00-02:30（部分品种）

## 项目结构

```
cexsample/
├── api/
│   └── allapi/              # CTP API 统一库（SE版本）
│       ├── *.h              # 头文件（TraderApi & MdApi）
│       ├── *.lib            # 静态库
│       └── *.dll            # 动态库
├── bin/                     # 可执行文件目录
│   ├── CTP_Trader.exe       # 主程序
│   └── *.dll                # 运行时依赖DLL
├── src/                     # 源代码
│   ├── main.c               # GUI界面和主逻辑
│   ├── ctp_trader.cpp       # CTP API封装
│   └── ctp_trader.h         # 接口定义
├── flow/                    # CTP流文件（自动生成）
├── obj/                     # 编译中间文件
├── build.ps1                # 编译脚本（推荐）
└── run.bat                  # 运行脚本
```

## 技术架构

### API 集成
- **TraderApi**：交易接口（认证、登录、查询、报单）
- **MdApi**：行情接口（连接、订阅、实时推送）

### 编译环境
- **编译器**：Visual Studio 2022 MSVC (x64)
- **语言**：C/C++ 混编（UNICODE模式）
- **UI框架**：Win32 API
- **库文件**：
  - `thosttraderapi_se.lib` (3.5MB)
  - `thostmduserapi_se.lib` (3.1MB)

### 编译过程
```
[1/4] 清理旧文件
[2/4] 编译 C++ 文件 (ctp_trader.cpp)
[3/4] 编译 C 文件 (main.c)
[4/4] 链接生成可执行文件
[5/5] 复制 DLL 到 bin 目录
```

## 常见问题

### 1. 杀毒软件误报
**问题**：Windows Defender 删除 `CTP_Trader.exe`

**解决方案**：
```powershell
# 以管理员身份运行
.\add_exclusion.ps1
```

或手动添加排除项：
- Windows 安全中心 → 病毒和威胁防护 → 管理设置 → 排除项
- 添加文件夹：`D:\projects\other\cexsample\bin`

### 2. 编译失败
**问题**：找不到 `vcvars64.bat`

**解决方案**：
- 确保安装了 Visual Studio 2022
- 检查路径：`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`
- 修改 `build.ps1` 中的路径

### 3. 行情无数据
**问题**：订阅行情后界面无反应

**原因**：不在交易时段，服务器不推送数据

**解决方案**：
- 在交易时段测试（09:00-15:00 或 21:00-02:30）
- 查看 `ctp_debug.log` 确认订阅成功
- 确保行情前置地址正确

### 4. 连接失败
**检查项**：
- 网络连接是否正常
- 前置地址格式：`tcp://IP:PORT`
- 券商代码、用户名、密码是否正确
- 查看 `flow/` 目录下的错误日志

## 调试信息

程序运行时会生成 `ctp_debug.log` 日志文件，记录：
- API 初始化过程
- 连接和登录状态
- 查询和订阅操作
- 回调函数执行情况

查看日志：
```powershell
Get-Content ctp_debug.log -Tail 50
```

## 开发说明

### 修改代码后重新编译
```powershell
# 强制终止旧进程并重新编译
taskkill /F /IM CTP_Trader.exe 2>$null
.\build.ps1
.\run.bat
```

### 添加新功能
1. 修改 `src/ctp_trader.h` 添加接口声明
2. 在 `src/ctp_trader.cpp` 实现功能
3. 在 `src/main.c` 添加UI控件和事件处理
4. 重新编译测试

### API 文档
参考：`api/doc/SFIT_CTP_Mini_API_V1.7.3-P2.pdf`

## 版本信息

- **CTP API 版本**：6.x.x（SE标准版）
- **编译器**：MSVC 19.x (Visual Studio 2022)
- **目标平台**：Windows x64
- **字符集**：UNICODE

## 许可证

本项目仅供学习和研究使用，请遵守相关法律法规和 CTP API 使用协议。

---

**最后更新**：2026年1月13日
