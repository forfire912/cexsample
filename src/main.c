// CTP Trading System - Windows GUI Application

// 源文件使用 UTF-8 编码
#pragma execution_character_set("utf-8")

// 必须在 include windows.h 之前定义 UNICODE
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include "ctp_trader.h"

// Enable Visual Styles and Common Controls 6.0
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#pragma comment(lib, "comctl32.lib")

// 主控件
#define IDC_TAB_CONTROL     1001
#define IDC_STATUS          1002

// 连接区域
#define IDC_BTN_CONNECT     1010
#define IDC_EDIT_BROKERID   1011
#define IDC_EDIT_USERID     1012
#define IDC_EDIT_PASSWORD   1013
#define IDC_EDIT_FRONTADDR  1014
#define IDC_EDIT_AUTHCODE   1015

// TAB 1: 查询功能
#define IDC_QUERY_PANEL     1100
#define IDC_BTN_QUERY_ORDER 1101
#define IDC_BTN_QUERY_POS   1102
#define IDC_BTN_QUERY_MARKET 1103
#define IDC_BTN_QUERY_INST  1104
#define IDC_EDIT_QUERY_INSTRUMENT 1105
#define IDC_LISTVIEW_QUERY  1106

// TAB 2: 交易功能
#define IDC_TRADE_PANEL     1200
#define IDC_EDIT_TRADE_INSTRUMENT 1201
#define IDC_EDIT_TRADE_PRICE 1202
#define IDC_EDIT_TRADE_VOLUME 1203
#define IDC_BTN_BUY_OPEN    1204
#define IDC_BTN_SELL_OPEN   1205
#define IDC_BTN_BUY_CLOSE   1206
#define IDC_BTN_SELL_CLOSE  1207
#define IDC_RADIO_BUY       1208
#define IDC_RADIO_SELL      1209
#define IDC_RADIO_OPEN      1210
#define IDC_RADIO_CLOSE     1211
#define IDC_BTN_SUBMIT_ORDER 1212
#define IDC_STATIC_MARGIN   1213

// TAB 3: 持仓管理
#define IDC_POSITION_PANEL  1300
#define IDC_LISTVIEW_POSITION 1301
#define IDC_LISTVIEW_ORDERS 1302
#define IDC_BTN_REFRESH_POS 1303
#define IDC_BTN_CLOSE_ALL   1304
#define IDC_BTN_REFRESH_ORDERS 1305
#define IDC_BTN_CANCEL_ALL  1306

// TAB 4: 系统设置
#define IDC_SETTINGS_PANEL  1400
#define IDC_CHECK_CONFIRM   1401
#define IDC_CHECK_NOTIFY    1402
#define IDC_CHECK_LOG       1403

HINSTANCE g_hInst;
HWND g_hMainWnd;
HWND g_hTabControl;
HWND g_hStatus;

// 连接区域控件
HWND g_hEditBrokerID;
HWND g_hEditUserID;
HWND g_hEditPassword;
HWND g_hEditFrontAddr;
HWND g_hEditAuthCode;

// TAB面板
HWND g_hQueryPanel;
HWND g_hTradePanel;
HWND g_hPositionPanel;
HWND g_hSettingsPanel;

// 查询面板控件
HWND g_hListViewQuery;
HWND g_hEditQueryInstrument;
HWND g_hQueryControls[15];  // 查询面板所有控件数组
int g_nQueryControlCount = 0;

// 交易面板控件
HWND g_hEditTradeInstrument;
HWND g_hEditTradePrice;
HWND g_hEditTradeVolume;
HWND g_hRadioBuy;
HWND g_hRadioSell;
HWND g_hRadioOpen;
HWND g_hRadioClose;
HWND g_hTradeControls[30];  // 交易面板所有控件数组
int g_nTradeControlCount = 0;

// 持仓管理面板控件
HWND g_hListViewPosition;
HWND g_hListViewOrders;
HWND g_hPositionControls[10];  // 持仓管理面板所有控件数组
int g_nPositionControlCount = 0;

// 系统设置面板控件
HWND g_hSettingsControls[15];  // 系统设置面板所有控件数组
int g_nSettingsControlCount = 0;

CTPTrader* g_pTrader = NULL;

const char* BROKER_ID = "1010";
const char* USER_ID = "20833";
const char* PASSWORD = "******";
const char* FRONT_ADDR = "tcp://106.37.101.162:31213";
const char* AUTH_CODE = "YHQHYHQHYHQHYHQH";
const char* APP_ID = "client_long_1.0.0";

void CreateMainWindow(HWND hWnd, HINSTANCE hInstance);
void CreateQueryPanel(HWND hParent, HINSTANCE hInstance);
void CreateTradePanel(HWND hParent, HINSTANCE hInstance);
void CreateSettingsPanel(HWND hParent, HINSTANCE hInstance);
void SwitchTab(int tabIndex);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void UpdateStatus(const char* msg);

LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* pExceptionInfo) {
    MessageBox(NULL, TEXT("发生了异常错误\n请联系管理员"), TEXT("错误"), MB_ICONERROR | MB_OK);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(ExceptionFilter);
    g_hInst = hInstance;
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = TEXT("CTPTraderClass");
    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, TEXT("窗口类注册失败"), TEXT("错误"), MB_ICONERROR | MB_OK);
        return 0;
    }
    g_hMainWnd = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("CTPTraderClass"), TEXT("CTP交易系统"),
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 700,
                                NULL, NULL, hInstance, NULL);
    if (g_hMainWnd == NULL) {
        MessageBox(NULL, TEXT("主窗口创建失败"), TEXT("错误"), MB_ICONERROR | MB_OK);
        return 0;
    }
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// 面板窗口过程 - 转发WM_COMMAND到主窗口
WNDPROC g_oldPanelProc = NULL;
LRESULT CALLBACK PanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND) {
        // 转发WM_COMMAND到主窗口
        return SendMessage(g_hMainWnd, WM_COMMAND, wParam, lParam);
    }
    return CallWindowProc(g_oldPanelProc, hWnd, message, wParam, lParam);
}

void CreateMainWindow(HWND hWnd, HINSTANCE hInstance) {
    // ========== 连接信息区域（固定显示） ==========
    int y = 10;
    CreateWindow(TEXT("STATIC"), TEXT("经纪商ID:"), WS_VISIBLE | WS_CHILD, 
                 10, y, 80, 24, hWnd, NULL, hInstance, NULL);
    g_hEditBrokerID = CreateWindow(TEXT("EDIT"), TEXT("1010"), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER, 
                 90, y, 80, 24, hWnd, (HMENU)IDC_EDIT_BROKERID, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("用户ID:"), WS_VISIBLE | WS_CHILD, 
                 180, y, 60, 24, hWnd, NULL, hInstance, NULL);
    g_hEditUserID = CreateWindow(TEXT("EDIT"), TEXT("20833"), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER, 
                 240, y, 80, 24, hWnd, (HMENU)IDC_EDIT_USERID, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("密码:"), WS_VISIBLE | WS_CHILD, 
                 330, y, 40, 24, hWnd, NULL, hInstance, NULL);
    g_hEditPassword = CreateWindow(TEXT("EDIT"), TEXT("826109"), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD, 
                 370, y, 80, 24, hWnd, (HMENU)IDC_EDIT_PASSWORD, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("交易前置:"), WS_VISIBLE | WS_CHILD, 
                 460, y, 70, 24, hWnd, NULL, hInstance, NULL);
    g_hEditFrontAddr = CreateWindow(TEXT("EDIT"), TEXT("tcp://106.37.101.162:31205"), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER, 
                 530, y, 250, 24, hWnd, (HMENU)IDC_EDIT_FRONTADDR, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("认证码:"), WS_VISIBLE | WS_CHILD, 
                 790, y, 60, 24, hWnd, NULL, hInstance, NULL);
    g_hEditAuthCode = CreateWindow(TEXT("EDIT"), TEXT("YHQHYHQHYHQHYHQH"), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER, 
                 850, y, 150, 24, hWnd, (HMENU)IDC_EDIT_AUTHCODE, hInstance, NULL);
    
    CreateWindow(TEXT("BUTTON"), TEXT("连接登录"), 
                 WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 1010, y-2, 120, 30, hWnd, (HMENU)IDC_BTN_CONNECT, hInstance, NULL);
    
    // ========== TAB控件 ==========
    g_hTabControl = CreateWindowEx(0, WC_TABCONTROL, TEXT(""),
                 WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
                 10, 50, 1160, 560, hWnd, (HMENU)IDC_TAB_CONTROL, hInstance, NULL);
    
    // 添加TAB项
    TCITEM tie;
    tie.mask = TCIF_TEXT;
    
    tie.pszText = TEXT("查 询");
    TabCtrl_InsertItem(g_hTabControl, 0, &tie);
    
    tie.pszText = TEXT("交 易");
    TabCtrl_InsertItem(g_hTabControl, 1, &tie);
    
    tie.pszText = TEXT("系统设置");
    TabCtrl_InsertItem(g_hTabControl, 2, &tie);
    
    // ========== 创建各个面板 ==========
    CreateQueryPanel(hWnd, hInstance);
    CreateTradePanel(hWnd, hInstance);
    CreateSettingsPanel(hWnd, hInstance);
    
    // 默认显示第一个TAB
    SwitchTab(0);
    
    // ========== 状态栏 ==========
    g_hStatus = CreateWindow(TEXT("STATIC"), TEXT("状态: 未连接"), 
                 WS_VISIBLE | WS_CHILD | SS_LEFT,
                 10, 620, 1160, 20, hWnd, (HMENU)IDC_STATUS, hInstance, NULL);
}

// ========== 创建查询面板 ==========
void CreateQueryPanel(HWND hParent, HINSTANCE hInstance) {
    // 获取TAB控件的显示区域
    RECT rcTab;
    GetClientRect(g_hTabControl, &rcTab);
    TabCtrl_AdjustRect(g_hTabControl, FALSE, &rcTab);
    
    // 创建面板容器（虽然控件会直接放在主窗口，但保留容器用于占位）
    g_hQueryPanel = CreateWindow(TEXT("STATIC"), TEXT(""),
                 WS_CHILD | WS_VISIBLE,
                 rcTab.left + 10, rcTab.top + 10, 
                 rcTab.right - rcTab.left - 20, rcTab.bottom - rcTab.top - 20,
                 g_hTabControl, (HMENU)IDC_QUERY_PANEL, hInstance, NULL);
    
    // 设置查询TAB容器窗口的子类化过程
    if (g_oldPanelProc == NULL) {
        g_oldPanelProc = (WNDPROC)GetWindowLongPtr(g_hQueryPanel, GWLP_WNDPROC);
    }
    SetWindowLongPtr(g_hQueryPanel, GWLP_WNDPROC, (LONG_PTR)PanelProc);
    
    // 优化后的紧凑布局
    int x = 15;  // 左侧边距
    int y = 10;  // 顶部边距
    
    // 快速查询按钮区
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("快速查询:"), WS_CHILD, 
                 x, y, 80, 20, g_hQueryPanel, NULL, hInstance, NULL);
    
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询委托"), WS_CHILD | BS_PUSHBUTTON,
                 x+85, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_ORDER, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询持仓"), WS_CHILD | BS_PUSHBUTTON,
                 x+185, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_POS, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询行情"), WS_CHILD | BS_PUSHBUTTON,
                 x+285, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_MARKET, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询合约"), WS_CHILD | BS_PUSHBUTTON,
                 x+385, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_INST, hInstance, NULL);
    y += 35;
    
    // 条件查询区
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("合约代码:"), WS_CHILD, 
                 x, y+3, 70, 20, g_hQueryPanel, NULL, hInstance, NULL);
    g_hEditQueryInstrument = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), 
                 WS_CHILD | WS_BORDER | ES_UPPERCASE, 
                 x+75, y, 120, 24, g_hQueryPanel, (HMENU)IDC_EDIT_QUERY_INSTRUMENT, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = g_hEditQueryInstrument;
    
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("(点击下方列表可自动填充)"), WS_CHILD, 
                 x+205, y+3, 250, 20, g_hQueryPanel, NULL, hInstance, NULL);
    y += 35;
    
    // 查询结果区
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("查询结果:"), WS_CHILD, 
                 x, y, 80, 20, g_hQueryPanel, NULL, hInstance, NULL);
    y += 25;
    
    // ListView 直接创建在主窗口下，这样 WM_NOTIFY 可以正确传递
    // 计算ListView的实际位置（相对于主窗口）
    POINT pt = {0, 0};
    ClientToScreen(g_hTabControl, &pt);
    ScreenToClient(hParent, &pt);
    
    int listViewX = pt.x + x;
    int listViewY = pt.y + y + 10;
    int listViewWidth = 1120;
    int listViewHeight = 390;
    
    g_hListViewQuery = CreateWindowEx(0, WC_LISTVIEW, TEXT(""),
                 WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                 listViewX, listViewY, listViewWidth, listViewHeight, 
                 hParent, (HMENU)IDC_LISTVIEW_QUERY, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = g_hListViewQuery;
    ListView_SetExtendedListViewStyle(g_hListViewQuery, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
}

// ========== 创建交易面板 ==========
void CreateTradePanel(HWND hParent, HINSTANCE hInstance) {
    RECT rcTab;
    GetClientRect(g_hTabControl, &rcTab);
    TabCtrl_AdjustRect(g_hTabControl, FALSE, &rcTab);
    
    g_hTradePanel = CreateWindow(TEXT("STATIC"), TEXT(""),
                 WS_CHILD,
                 rcTab.left + 10, rcTab.top + 10, 
                 rcTab.right - rcTab.left - 20, rcTab.bottom - rcTab.top - 20,
                 g_hTabControl, (HMENU)IDC_TRADE_PANEL, hInstance, NULL);
    
    // 设置窗口子类化以转发WM_COMMAND
    if (g_oldPanelProc == NULL) {
        g_oldPanelProc = (WNDPROC)GetWindowLongPtr(g_hTradePanel, GWLP_WNDPROC);
    }
    SetWindowLongPtr(g_hTradePanel, GWLP_WNDPROC, (LONG_PTR)PanelProc);
    
    int x = 10, y = 5;
    
    // ===== 报单输入区域 =====
    CreateWindow(TEXT("STATIC"), TEXT("报单输入"), 
                 WS_VISIBLE | WS_CHILD | SS_LEFT,
                 x, y, 80, 20, g_hTradePanel, NULL, hInstance, NULL);
    y += 25;
    
    // 第一行：合约代码、交易方向、开平仓
    CreateWindow(TEXT("STATIC"), TEXT("合约:"), WS_VISIBLE | WS_CHILD, 
                 x, y+5, 45, 20, g_hTradePanel, NULL, hInstance, NULL);
    g_hEditTradeInstrument = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER | ES_UPPERCASE, 
                 x+50, y+2, 120, 24, g_hTradePanel, (HMENU)IDC_EDIT_TRADE_INSTRUMENT, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("方向:"), WS_VISIBLE | WS_CHILD, 
                 x+185, y+5, 45, 20, g_hTradePanel, NULL, hInstance, NULL);
    g_hRadioBuy = CreateWindow(TEXT("BUTTON"), TEXT("买"), 
                 WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                 x+230, y+3, 50, 20, g_hTradePanel, (HMENU)IDC_RADIO_BUY, hInstance, NULL);
    g_hRadioSell = CreateWindow(TEXT("BUTTON"), TEXT("卖"), 
                 WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                 x+285, y+3, 50, 20, g_hTradePanel, (HMENU)IDC_RADIO_SELL, hInstance, NULL);
    SendMessage(g_hRadioBuy, BM_SETCHECK, BST_CHECKED, 0);
    
    CreateWindow(TEXT("STATIC"), TEXT("开平:"), WS_VISIBLE | WS_CHILD, 
                 x+350, y+5, 45, 20, g_hTradePanel, NULL, hInstance, NULL);
    g_hRadioOpen = CreateWindow(TEXT("BUTTON"), TEXT("开仓"), 
                 WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                 x+395, y+3, 60, 20, g_hTradePanel, (HMENU)IDC_RADIO_OPEN, hInstance, NULL);
    g_hRadioClose = CreateWindow(TEXT("BUTTON"), TEXT("平仓"), 
                 WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                 x+460, y+3, 60, 20, g_hTradePanel, (HMENU)IDC_RADIO_CLOSE, hInstance, NULL);
    SendMessage(g_hRadioOpen, BM_SETCHECK, BST_CHECKED, 0);
    y += 32;
    
    // 第二行：价格、手数、提交按钮
    CreateWindow(TEXT("STATIC"), TEXT("价格:"), WS_VISIBLE | WS_CHILD, 
                 x, y+5, 45, 20, g_hTradePanel, NULL, hInstance, NULL);
    g_hEditTradePrice = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER, 
                 x+50, y+2, 120, 24, g_hTradePanel, (HMENU)IDC_EDIT_TRADE_PRICE, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("手数:"), WS_VISIBLE | WS_CHILD, 
                 x+185, y+5, 45, 20, g_hTradePanel, NULL, hInstance, NULL);
    g_hEditTradeVolume = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("1"), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER, 
                 x+230, y+2, 80, 24, g_hTradePanel, (HMENU)IDC_EDIT_TRADE_VOLUME, hInstance, NULL);
    
    CreateWindow(TEXT("BUTTON"), TEXT("提交报单 (F9)"), 
                 WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 x+330, y, 190, 28, g_hTradePanel, (HMENU)IDC_BTN_SUBMIT_ORDER, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("(提示：先在查询TAB点击合约列表选择合约)"), WS_VISIBLE | WS_CHILD, 
                 x+535, y+7, 400, 20, g_hTradePanel, NULL, hInstance, NULL);
    y += 40;
    
    // 分隔线
    CreateWindow(TEXT("STATIC"), TEXT(""), 
                 WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
                 x, y, 1100, 2, g_hTradePanel, NULL, hInstance, NULL);
    y += 10;
    
    // ===== 持仓管理区域 =====
    CreateWindow(TEXT("STATIC"), TEXT("持仓管理"), WS_VISIBLE | WS_CHILD, 
                 x, y, 80, 20, g_hTradePanel, NULL, hInstance, NULL);
    CreateWindow(TEXT("BUTTON"), TEXT("刷新"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 x+950, y-2, 70, 24, g_hTradePanel, (HMENU)IDC_BTN_REFRESH_POS, hInstance, NULL);
    CreateWindow(TEXT("BUTTON"), TEXT("平所有"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 x+1030, y-2, 70, 24, g_hTradePanel, (HMENU)IDC_BTN_CLOSE_ALL, hInstance, NULL);
    y += 28;
    
    // 持仓列表
    g_hListViewPosition = CreateWindowEx(0, WC_LISTVIEW, TEXT(""),
                 WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                 x, y, 1100, 160, g_hTradePanel, (HMENU)IDC_LISTVIEW_POSITION, hInstance, NULL);
    ListView_SetExtendedListViewStyle(g_hListViewPosition, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    y += 170;
    
    // ===== 委托管理区域 =====
    CreateWindow(TEXT("STATIC"), TEXT("委托管理"), WS_VISIBLE | WS_CHILD, 
                 x, y, 80, 20, g_hTradePanel, NULL, hInstance, NULL);
    CreateWindow(TEXT("BUTTON"), TEXT("刷新"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 x+950, y-2, 70, 24, g_hTradePanel, (HMENU)IDC_BTN_REFRESH_ORDERS, hInstance, NULL);
    CreateWindow(TEXT("BUTTON"), TEXT("撤所有"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 x+1030, y-2, 70, 24, g_hTradePanel, (HMENU)IDC_BTN_CANCEL_ALL, hInstance, NULL);
    y += 28;
    
    // 委托列表
    g_hListViewOrders = CreateWindowEx(0, WC_LISTVIEW, TEXT(""),
                 WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                 x, y, 1100, 160, g_hTradePanel, (HMENU)IDC_LISTVIEW_ORDERS, hInstance, NULL);
    ListView_SetExtendedListViewStyle(g_hListViewOrders, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
}

// ========== 创建持仓管理面板 ==========
// ========== 创建系统设置面板 ==========
void CreateSettingsPanel(HWND hParent, HINSTANCE hInstance) {
    RECT rcTab;
    GetClientRect(g_hTabControl, &rcTab);
    TabCtrl_AdjustRect(g_hTabControl, FALSE, &rcTab);
    
    g_hSettingsPanel = CreateWindow(TEXT("STATIC"), TEXT(""),
                 WS_CHILD,
                 rcTab.left + 10, rcTab.top + 10, 
                 rcTab.right - rcTab.left - 20, rcTab.bottom - rcTab.top - 20,
                 g_hTabControl, (HMENU)IDC_SETTINGS_PANEL, hInstance, NULL);
    
    // 设置窗口子类化以转发WM_COMMAND
    if (g_oldPanelProc == NULL) {
        g_oldPanelProc = (WNDPROC)GetWindowLongPtr(g_hSettingsPanel, GWLP_WNDPROC);
    }
    SetWindowLongPtr(g_hSettingsPanel, GWLP_WNDPROC, (LONG_PTR)PanelProc);
    
    int x = 30, y = 15;
    
    // 交易设置
    CreateWindow(TEXT("STATIC"), TEXT("【 交易设置 】"), 
                 WS_VISIBLE | WS_CHILD,
                 x, y, 200, 30, g_hSettingsPanel, NULL, hInstance, NULL);
    y += 40;
    
    CreateWindow(TEXT("BUTTON"), TEXT("☑ 下单前确认"), 
                 WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_LEFTTEXT,
                 x, y, 300, 25, g_hSettingsPanel, (HMENU)IDC_CHECK_CONFIRM, hInstance, NULL);
    SendMessage(GetDlgItem(g_hSettingsPanel, IDC_CHECK_CONFIRM), BM_SETCHECK, BST_CHECKED, 0);
    y += 35;
    
    CreateWindow(TEXT("BUTTON"), TEXT("☑ 成交后弹窗提醒"), 
                 WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_LEFTTEXT,
                 x, y, 300, 25, g_hSettingsPanel, (HMENU)IDC_CHECK_NOTIFY, hInstance, NULL);
    SendMessage(GetDlgItem(g_hSettingsPanel, IDC_CHECK_NOTIFY), BM_SETCHECK, BST_CHECKED, 0);
    y += 35;
    
    CreateWindow(TEXT("BUTTON"), TEXT("☑ 启用日志记录"), 
                 WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_LEFTTEXT,
                 x, y, 300, 25, g_hSettingsPanel, (HMENU)IDC_CHECK_LOG, hInstance, NULL);
    SendMessage(GetDlgItem(g_hSettingsPanel, IDC_CHECK_LOG), BM_SETCHECK, BST_CHECKED, 0);
    y += 50;
    
    // 系统信息
    CreateWindow(TEXT("STATIC"), TEXT("【 系统信息 】"), 
                 WS_VISIBLE | WS_CHILD,
                 x, y, 200, 30, g_hSettingsPanel, NULL, hInstance, NULL);
    y += 40;
    
    CreateWindow(TEXT("STATIC"), TEXT("版本: v1.0.0"), 
                 WS_VISIBLE | WS_CHILD,
                 x, y, 400, 25, g_hSettingsPanel, NULL, hInstance, NULL);
    y += 30;
    
    CreateWindow(TEXT("STATIC"), TEXT("CTP API: v6.7.0"), 
                 WS_VISIBLE | WS_CHILD,
                 x, y, 400, 25, g_hSettingsPanel, NULL, hInstance, NULL);
    y += 30;
    
    CreateWindow(TEXT("STATIC"), TEXT("编译日期: 2026-01-15"), 
                 WS_VISIBLE | WS_CHILD,
                 x, y, 400, 25, g_hSettingsPanel, NULL, hInstance, NULL);
}

// ========== TAB切换函数 ==========
void SwitchTab(int tabIndex) {
    // 显示/隐藏查询面板的所有控件
    for (int i = 0; i < g_nQueryControlCount; i++) {
        ShowWindow(g_hQueryControls[i], tabIndex == 0 ? SW_SHOW : SW_HIDE);
    }
    
    // 显示/隐藏交易面板容器（及其子控件）
    ShowWindow(g_hTradePanel, tabIndex == 1 ? SW_SHOW : SW_HIDE);
    
    // 显示/隐藏系统设置面板容器（及其子控件）
    ShowWindow(g_hSettingsPanel, tabIndex == 2 ? SW_SHOW : SW_HIDE);
    
    // 强制重绘主窗口
    InvalidateRect(g_hMainWnd, NULL, TRUE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            CreateMainWindow(hWnd, g_hInst);
            g_pTrader = CreateCTPTrader();
            SetListView(g_pTrader, g_hListViewQuery);  // 默认使用查询面板的ListView
            SetStatusCallback(g_pTrader, UpdateStatus);
            return 0;
            
        case WM_NOTIFY: {
            LPNMHDR pnmhdr = (LPNMHDR)lParam;
            
            // 处理TAB切换
            if (pnmhdr->idFrom == IDC_TAB_CONTROL && pnmhdr->code == TCN_SELCHANGE) {
                int tabIndex = TabCtrl_GetCurSel(g_hTabControl);
                SwitchTab(tabIndex);
                
                // 根据TAB切换ListView
                if (tabIndex == 0) {
                    SetListView(g_pTrader, g_hListViewQuery);
                } else if (tabIndex == 1) {
                    // 交易TAB默认使用持仓ListView
                    SetListView(g_pTrader, g_hListViewPosition);
                }
                return 0;
            }
            
            // 处理ListView点击（查询面板）
            if (pnmhdr->idFrom == IDC_LISTVIEW_QUERY) {
                if (pnmhdr->code == LVN_ITEMCHANGED) {
                    // 处理选中状态的变化
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if ((pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED)) {
                        // 从未选中变为选中
                        WCHAR instrumentID[32] = {0};
                        ListView_GetItemText(g_hListViewQuery, pnmv->iItem, 0, instrumentID, 32);
                        
                        // 去除尾部空格
                        int len = wcslen(instrumentID);
                        while (len > 0 && instrumentID[len-1] == L' ') {
                            instrumentID[--len] = 0;
                        }
                        
                        if (len > 0) {
                            SetWindowText(g_hEditQueryInstrument, instrumentID);
                            SetWindowText(g_hEditTradeInstrument, instrumentID);  // 同时填充到交易面板
                            
                            char ansiInstrumentID[32];
                            WideCharToMultiByte(CP_ACP, 0, instrumentID, -1, ansiInstrumentID, sizeof(ansiInstrumentID), NULL, NULL);
                            char statusMsg[128];
                            sprintf(statusMsg, "已选择合约: %s", ansiInstrumentID);
                            UpdateStatus(statusMsg);
                        }
                    }
                }
            }
            return 0;
        }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_CONNECT: {
                    char brokerID[32], userID[32], password[32], frontAddr[128], authCode[64];
                    WCHAR wBrokerID[32], wUserID[32], wPassword[32], wFrontAddr[128], wAuthCode[64];
                    
                    GetWindowText(g_hEditBrokerID, wBrokerID, 32);
                    GetWindowText(g_hEditUserID, wUserID, 32);
                    GetWindowText(g_hEditPassword, wPassword, 32);
                    GetWindowText(g_hEditFrontAddr, wFrontAddr, 128);
                    GetWindowText(g_hEditAuthCode, wAuthCode, 64);
                    
                    // 转换为 ANSI (CTP API 需要)
                    WideCharToMultiByte(CP_ACP, 0, wBrokerID, -1, brokerID, sizeof(brokerID), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wUserID, -1, userID, sizeof(userID), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wPassword, -1, password, sizeof(password), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wFrontAddr, -1, frontAddr, sizeof(frontAddr), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wAuthCode, -1, authCode, sizeof(authCode), NULL, NULL);
                    
                    UpdateStatus("正在连接...");
                    ConnectAndLogin(g_pTrader, brokerID, userID, password, frontAddr, authCode, APP_ID);
                    break;
                }
                
                case IDC_BTN_QUERY_ORDER:
                    if (g_pTrader && IsLoggedIn(g_pTrader)) {
                        UpdateStatus("正在查询委托...");
                        QueryOrders(g_pTrader);
                    } else {
                        UpdateStatus("错误: 请先连接登录");
                    }
                    break;
                case IDC_BTN_QUERY_POS:
                    if (g_pTrader && IsLoggedIn(g_pTrader)) {
                        UpdateStatus("正在查询持仓...");
                        QueryPositions(g_pTrader);
                    } else {
                        UpdateStatus("错误: 请先连接登录");
                    }
                    break;
                case IDC_BTN_QUERY_MARKET:
                    if (g_pTrader && IsLoggedIn(g_pTrader)) {
                        WCHAR instrumentID[32] = {0};
                        GetWindowText(g_hEditQueryInstrument, instrumentID, 32);
                        
                        // 去除首尾空格
                        int len = wcslen(instrumentID);
                        while (len > 0 && instrumentID[len-1] == L' ') {
                            instrumentID[--len] = 0;
                        }
                        
                        if (len == 0) {
                            MessageBox(g_hMainWnd, 
                                     TEXT("请输入合约代码！\n\n提示：\n1. 先点击\"查询合约\"获取合约列表\n2. 点击选中任意合约行，代码会自动填写\n3. 点击\"查询行情\"按钮\n\n注意：部分CTP柜台可能不支持通过交易接口查询行情"), 
                                     TEXT("提示"), 
                                     MB_ICONINFORMATION | MB_OK);
                            SetFocus(g_hEditQueryInstrument);
                        } else {
                            char ansiInstrumentID[32];
                            WideCharToMultiByte(CP_ACP, 0, instrumentID, -1, ansiInstrumentID, sizeof(ansiInstrumentID), NULL, NULL);
                            
                            char statusMsg[128];
                            sprintf(statusMsg, "正在查询合约 %s 的行情...", ansiInstrumentID);
                            UpdateStatus(statusMsg);
                            
                            int result = QueryMarketData(g_pTrader, ansiInstrumentID);
                            
                            // 提示用户可能的限制
                            if (result == 0) {
                                UpdateStatus("行情查询请求已发送，请等待响应...");
                                
                                // 设置一个定时器，5秒后检查是否有数据
                                SetTimer(g_hMainWnd, 1001, 5000, NULL);
                            } else {
                                UpdateStatus("错误: 行情查询请求发送失败");
                            }
                        }
                    } else {
                        UpdateStatus("错误: 请先连接登录");
                    }
                    break;
                case IDC_BTN_QUERY_INST:
                    if (g_pTrader && IsLoggedIn(g_pTrader)) {
                        UpdateStatus("正在查询合约...");
                        QueryInstrument(g_pTrader, "");
                    } else {
                        UpdateStatus("错误: 请先连接登录");
                    }
                    break;
                    
                // 交易功能 - 提交报单
                case IDC_BTN_SUBMIT_ORDER: {
                    if (!g_pTrader || !IsLoggedIn(g_pTrader)) {
                        MessageBox(g_hMainWnd, TEXT("请先连接登录！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        break;
                    }
                    
                    WCHAR wInstrument[32] = {0}, wPrice[32] = {0}, wVolume[32] = {0};
                    GetWindowText(g_hEditTradeInstrument, wInstrument, 32);
                    GetWindowText(g_hEditTradePrice, wPrice, 32);
                    GetWindowText(g_hEditTradeVolume, wVolume, 32);
                    
                    char instrument[32], priceStr[32], volumeStr[32];
                    WideCharToMultiByte(CP_ACP, 0, wInstrument, -1, instrument, sizeof(instrument), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wPrice, -1, priceStr, sizeof(priceStr), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wVolume, -1, volumeStr, sizeof(volumeStr), NULL, NULL);
                    
                    // 验证输入
                    if (strlen(instrument) == 0) {
                        MessageBox(g_hMainWnd, TEXT("请输入合约代码！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        SetFocus(g_hEditTradeInstrument);
                        break;
                    }
                    if (strlen(priceStr) == 0) {
                        MessageBox(g_hMainWnd, TEXT("请输入价格！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        SetFocus(g_hEditTradePrice);
                        break;
                    }
                    if (strlen(volumeStr) == 0) {
                        MessageBox(g_hMainWnd, TEXT("请输入手数！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        SetFocus(g_hEditTradeVolume);
                        break;
                    }
                    
                    double price = atof(priceStr);
                    int volume = atoi(volumeStr);
                    
                    if (price <= 0) {
                        MessageBox(g_hMainWnd, TEXT("价格必须大于0！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        SetFocus(g_hEditTradePrice);
                        break;
                    }
                    if (volume <= 0) {
                        MessageBox(g_hMainWnd, TEXT("手数必须大于0！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        SetFocus(g_hEditTradeVolume);
                        break;
                    }
                    
                    // 根据单选按钮确定方向和开平
                    char direction = (SendMessage(g_hRadioBuy, BM_GETCHECK, 0, 0) == BST_CHECKED) ? '0' : '1';
                    char offsetFlag = (SendMessage(g_hRadioOpen, BM_GETCHECK, 0, 0) == BST_CHECKED) ? '0' : '1';
                    
                    const char* actionName;
                    if (direction == '0' && offsetFlag == '0') actionName = "买开";
                    else if (direction == '1' && offsetFlag == '0') actionName = "卖开";
                    else if (direction == '0' && offsetFlag == '1') actionName = "买平";
                    else actionName = "卖平";
                    
                    // 显示报单信息到状态栏
                    char statusMsg[256];
                    sprintf(statusMsg, "正在报单: %s %s 价格:%.2f 手数:%d", instrument, actionName, price, volume);
                    UpdateStatus(statusMsg);
                    
                    // 直接提交订单
                    int result = SendOrder(g_pTrader, instrument, direction, offsetFlag, price, volume);
                    if (result != 0) {
                        char errorMsg[128];
                        sprintf(errorMsg, "报单失败: ErrorID=%d", result);
                        UpdateStatus(errorMsg);
                    } else {
                        UpdateStatus("报单成功，等待回报...");
                    }
                    break;
                }
                
                case IDC_BTN_REFRESH_POS: {
                    if (!g_pTrader || !IsLoggedIn(g_pTrader)) {
                        MessageBox(g_hMainWnd, TEXT("请先连接登录！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        break;
                    }
                    // 切换到持仓ListView
                    SetListView(g_pTrader, g_hListViewPosition);
                    UpdateStatus("正在刷新持仓...");
                    QueryPositions(g_pTrader);
                    break;
                }
                case IDC_BTN_CLOSE_ALL: {
                    if (!g_pTrader || !IsLoggedIn(g_pTrader)) {
                        MessageBox(g_hMainWnd, TEXT("请先连接登录！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        break;
                    }
                    MessageBox(g_hMainWnd, 
                             TEXT("平所有持仓功能说明：\n\n此功能需要先查询持仓，然后逐个发送平仓指令。\n\n建议：\n1. 先点击\"刷新持仓\"查看当前持仓\n2. 在持仓列表中选择要平仓的合约\n3. 手动在交易TAB中填写平仓单\n\n批量平仓功能将在后续版本中实现。"),
                             TEXT("平所有持仓"),
                             MB_ICONINFORMATION | MB_OK);
                    break;
                }
                case IDC_BTN_REFRESH_ORDERS: {
                    if (!g_pTrader || !IsLoggedIn(g_pTrader)) {
                        MessageBox(g_hMainWnd, TEXT("请先连接登录！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        break;
                    }
                    // 切换到委托ListView
                    SetListView(g_pTrader, g_hListViewOrders);
                    UpdateStatus("正在刷新委托...");
                    QueryOrders(g_pTrader);
                    break;
                }
                case IDC_BTN_CANCEL_ALL: {
                    if (!g_pTrader || !IsLoggedIn(g_pTrader)) {
                        MessageBox(g_hMainWnd, TEXT("请先连接登录！"), TEXT("提示"), MB_ICONWARNING | MB_OK);
                        break;
                    }
                    MessageBox(g_hMainWnd, 
                             TEXT("取消所有委托功能说明：\n\n此功能需要先查询委托，然后逐个发送撤单指令。\n\n建议：\n1. 先点击\"刷新委托\"查看当前委托\n2. 在委托列表中选择要撤销的委托\n3. 手动撤单（功能开发中）\n\n批量撤单功能将在后续版本中实现。"),
                             TEXT("取消所有委托"),
                             MB_ICONINFORMATION | MB_OK);
                    break;
                }
            }
            return 0;
        case WM_TIMER:
            if (wParam == 1001) {
                if (!g_hListViewQuery || !IsWindow(g_hListViewQuery)) {
                    MessageBox(g_hMainWnd, TEXT("ListView 未初始化或已销毁！"), TEXT("错误"), MB_ICONERROR | MB_OK);
                    KillTimer(g_hMainWnd, 1001);
                    return 0;
                }

                // 检查ListView是否有数据
                int itemCount = ListView_GetItemCount(g_hListViewQuery);
                if (itemCount == 0) {
                    MessageBox(g_hMainWnd,
                             TEXT("未收到行情数据！\n\n可能的原因：\n1. 该CTP柜台不支持通过交易接口查询行情\n2. 合约代码输入错误\n3. 当前非交易时间\n4. 需要使用行情API进行实时订阅\n\n建议：\n- 检查合约代码是否正确\n- 确认当前是交易时间\n- 或者联系管理员开通行情权限"),
                             TEXT("行情查询提示"),
                             MB_ICONWARNING | MB_OK);
                }
                KillTimer(g_hMainWnd, 1001);
            }
            return 0;
        case WM_DESTROY:
            if (g_pTrader) {
                DestroyCTPTrader(g_pTrader);
                g_pTrader = NULL;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void UpdateStatus(const char* msg) {
    if (g_hStatus && msg) {
        // 先将 UTF-8 转换为 UNICODE
        WCHAR wMsg[512];
        int wlen = MultiByteToWideChar(CP_UTF8, 0, msg, -1, wMsg, 512);
        
        if (wlen == 0) {
            // UTF-8 转换失败，尝试按 GBK 处理
            MultiByteToWideChar(CP_ACP, 0, msg, -1, wMsg, 512);
        }
        
        SetWindowText(g_hStatus, wMsg);
    }
}
