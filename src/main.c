// CTP Trading System - Windows GUI Application

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

#define IDC_BTN_CONNECT     1001
#define IDC_BTN_QUERY_ORDER 1002
#define IDC_BTN_QUERY_POS   1003
#define IDC_BTN_QUERY_MARKET 1004
#define IDC_BTN_QUERY_INST  1005
#define IDC_LISTVIEW        1006
#define IDC_STATUS          1007
#define IDC_EDIT_INSTRUMENT 1008
#define IDC_BTN_CONNECT_MD  1009
#define IDC_BTN_SUBSCRIBE   1010
#define IDC_EDIT_MD_FRONT   1011

HINSTANCE g_hInst;
HWND g_hMainWnd;
HWND g_hListView;
HWND g_hStatus;
HWND g_hEditBrokerID;
HWND g_hEditUserID;
HWND g_hEditPassword;
HWND g_hEditFrontAddr;
HWND g_hEditAuthCode;
HWND g_hEditInstrument;
HWND g_hEditMdFront;
CTPTrader* g_pTrader = NULL;

const char* BROKER_ID = "1010";
const char* USER_ID = "20833";
const char* PASSWORD = "******";
const char* FRONT_ADDR = "tcp://106.37.101.162:31213";
const char* AUTH_CODE = "YHQHYHQHYHQHYHQH";
const char* APP_ID = "client_long_1.0.0";

void CreateMainWindow(HWND hWnd, HINSTANCE hInstance);
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

void CreateMainWindow(HWND hWnd, HINSTANCE hInstance) {
    CreateWindow(TEXT("STATIC"), TEXT("经纪商ID:"), WS_VISIBLE | WS_CHILD, 10, 10, 80, 24, hWnd, NULL, hInstance, NULL);
    g_hEditBrokerID = CreateWindow(TEXT("EDIT"), TEXT("1010"), WS_VISIBLE | WS_CHILD | WS_BORDER, 90, 10, 100, 24, hWnd, NULL, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("用户ID:"), WS_VISIBLE | WS_CHILD, 200, 10, 60, 24, hWnd, NULL, hInstance, NULL);
    g_hEditUserID = CreateWindow(TEXT("EDIT"), TEXT("20833"), WS_VISIBLE | WS_CHILD | WS_BORDER, 260, 10, 100, 24, hWnd, NULL, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("密码:"), WS_VISIBLE | WS_CHILD, 370, 10, 40, 24, hWnd, NULL, hInstance, NULL);
    g_hEditPassword = CreateWindow(TEXT("EDIT"), TEXT("826109"), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD, 410, 10, 100, 24, hWnd, NULL, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("交易前置:"), WS_VISIBLE | WS_CHILD, 520, 10, 70, 24, hWnd, NULL, hInstance, NULL);
    g_hEditFrontAddr = CreateWindow(TEXT("EDIT"), TEXT("tcp://106.37.101.162:31205"), WS_VISIBLE | WS_CHILD | WS_BORDER, 590, 10, 200, 24, hWnd, NULL, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("认证码:"), WS_VISIBLE | WS_CHILD, 800, 10, 60, 24, hWnd, NULL, hInstance, NULL);
    g_hEditAuthCode = CreateWindow(TEXT("EDIT"), TEXT("YHQHYHQHYHQHYHQH"), WS_VISIBLE | WS_CHILD | WS_BORDER, 860, 10, 150, 24, hWnd, NULL, hInstance, NULL);
    
    CreateWindow(TEXT("BUTTON"), TEXT("连接登录"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 1020, 10, 120, 30, hWnd, (HMENU)IDC_BTN_CONNECT, hInstance, NULL);
    
    CreateWindow(TEXT("BUTTON"), TEXT("查询委托"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 10, 50, 120, 30, hWnd, (HMENU)IDC_BTN_QUERY_ORDER, hInstance, NULL);
    CreateWindow(TEXT("BUTTON"), TEXT("查询持仓"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 140, 50, 120, 30, hWnd, (HMENU)IDC_BTN_QUERY_POS, hInstance, NULL);
    
    // 行情查询区域
    CreateWindow(TEXT("STATIC"), TEXT("合约代码:"), WS_VISIBLE | WS_CHILD, 270, 55, 70, 20, hWnd, NULL, hInstance, NULL);
    g_hEditInstrument = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER | ES_UPPERCASE, 
                 340, 53, 100, 24, hWnd, (HMENU)IDC_EDIT_INSTRUMENT, hInstance, NULL);
    
    CreateWindow(TEXT("STATIC"), TEXT("行情前置:"), WS_VISIBLE | WS_CHILD, 450, 55, 70, 20, hWnd, NULL, hInstance, NULL);
    g_hEditMdFront = CreateWindow(TEXT("EDIT"), TEXT("tcp://106.37.101.162:31213"), 
                 WS_VISIBLE | WS_CHILD | WS_BORDER, 520, 53, 180, 24, hWnd, (HMENU)IDC_EDIT_MD_FRONT, hInstance, NULL);
    
    CreateWindow(TEXT("BUTTON"), TEXT("连接行情"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 710, 50, 100, 30, hWnd, (HMENU)IDC_BTN_CONNECT_MD, hInstance, NULL);
    CreateWindow(TEXT("BUTTON"), TEXT("订阅行情"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 820, 50, 100, 30, hWnd, (HMENU)IDC_BTN_SUBSCRIBE, hInstance, NULL);
    
    CreateWindow(TEXT("BUTTON"), TEXT("查询合约"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 930, 50, 100, 30, hWnd, (HMENU)IDC_BTN_QUERY_INST, hInstance, NULL);
    
    g_hListView = CreateWindowEx(0, WC_LISTVIEW, TEXT(""), WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                  10, 90, 1160, 520, hWnd, (HMENU)IDC_LISTVIEW, hInstance, NULL);
    ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    
    g_hStatus = CreateWindow(TEXT("STATIC"), TEXT("状态: 未连接"), WS_VISIBLE | WS_CHILD | SS_LEFT,
                             10, 620, 1160, 20, hWnd, (HMENU)IDC_STATUS, hInstance, NULL);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            CreateMainWindow(hWnd, g_hInst);
            g_pTrader = CreateCTPTrader();
            SetListView(g_pTrader, g_hListView);
            SetStatusCallback(g_pTrader, UpdateStatus);
            return 0;
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
                        GetWindowText(g_hEditInstrument, instrumentID, 32);
                        
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
                            SetFocus(g_hEditInstrument);
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
                case IDC_BTN_CONNECT_MD: {
                    char brokerID[32], userID[32], password[32], mdFrontAddr[128];
                    WCHAR wBrokerID[32], wUserID[32], wPassword[32], wMdFrontAddr[128];
                    
                    GetWindowText(g_hEditBrokerID, wBrokerID, 32);
                    GetWindowText(g_hEditUserID, wUserID, 32);
                    GetWindowText(g_hEditPassword, wPassword, 32);
                    GetWindowText(g_hEditMdFront, wMdFrontAddr, 128);
                    
                    WideCharToMultiByte(CP_ACP, 0, wBrokerID, -1, brokerID, sizeof(brokerID), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wUserID, -1, userID, sizeof(userID), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wPassword, -1, password, sizeof(password), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wMdFrontAddr, -1, mdFrontAddr, sizeof(mdFrontAddr), NULL, NULL);
                    
                    UpdateStatus("正在连接行情服务器...");
                    ConnectMarketData(g_pTrader, brokerID, userID, password, mdFrontAddr);
                    break;
                }
                case IDC_BTN_SUBSCRIBE:
                    if (g_pTrader && IsMarketDataConnected(g_pTrader)) {
                        WCHAR instrumentID[32] = {0};
                        GetWindowText(g_hEditInstrument, instrumentID, 32);
                        
                        int len = wcslen(instrumentID);
                        while (len > 0 && instrumentID[len-1] == L' ') {
                            instrumentID[--len] = 0;
                        }
                        
                        if (len == 0) {
                            MessageBox(g_hMainWnd, 
                                     TEXT("请输入合约代码！\n\n提示：\n1. 先点击\"查询合约\"获取合约列表\n2. 点击选中任意合约行，代码会自动填写\n3. 点击\"订阅行情\"按钮"), 
                                     TEXT("提示"), 
                                     MB_ICONINFORMATION | MB_OK);
                            SetFocus(g_hEditInstrument);
                        } else {
                            char ansiInstrumentID[32];
                            WideCharToMultiByte(CP_ACP, 0, instrumentID, -1, ansiInstrumentID, sizeof(ansiInstrumentID), NULL, NULL);
                            
                            char statusMsg[128];
                            sprintf(statusMsg, "正在订阅合约 %s 的行情...", ansiInstrumentID);
                            UpdateStatus(statusMsg);
                            
                            SubscribeMarketData(g_pTrader, ansiInstrumentID);
                        }
                    } else {
                        UpdateStatus("错误: 请先连接行情服务器");
                        MessageBox(g_hMainWnd,
                                 TEXT("请先点击\"连接行情\"按钮连接行情服务器！"),
                                 TEXT("提示"),
                                 MB_ICONWARNING | MB_OK);
                    }
                    break;
            }
            return 0;
        case WM_TIMER:
            if (wParam == 1001) {
                // 检查ListView是否有数据
                int itemCount = ListView_GetItemCount(g_hListView);
                if (itemCount == 0) {
                    MessageBox(g_hMainWnd,
                             TEXT("未收到行情数据！\n\n可能的原因：\n1. 该CTP柜台不支持通过交易接口查询行情\n2. 合约代码输入错误\n3. 当前非交易时间\n4. 需要使用行情API进行实时订阅\n\n建议：\n- 检查合约代码是否正确\n- 确认当前是交易时间\n- 或者联系管理员开通行情权限"),
                             TEXT("行情查询提示"),
                             MB_ICONWARNING | MB_OK);
                }
                KillTimer(g_hMainWnd, 1001);
            }
            return 0;
        case WM_NOTIFY:
            if (((LPNMHDR)lParam)->idFrom == IDC_LISTVIEW) {
                if (((LPNMHDR)lParam)->code == NM_CLICK || ((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    
                    // 检查是否有选中项
                    if (pnmv->uNewState & LVIS_SELECTED) {
                        WCHAR instrumentID[32] = {0};
                        
                        // 获取选中行的第一列（InstrumentID）的文本
                        ListView_GetItemText(g_hListView, pnmv->iItem, 0, instrumentID, 32);
                        
                        // 去除首尾空格
                        int len = wcslen(instrumentID);
                        while (len > 0 && instrumentID[len-1] == L' ') {
                            instrumentID[--len] = 0;
                        }
                        
                        // 填写到合约代码输入框
                        if (len > 0) {
                            SetWindowText(g_hEditInstrument, instrumentID);
                            
                            // 更新状态栏
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
        char statusText[512];
        WCHAR wStatusText[512];
        
        sprintf(statusText, "状态: %s", msg);
        
        // 转换为 Unicode
        MultiByteToWideChar(CP_UTF8, 0, statusText, -1, wStatusText, 512);
        SetWindowText(g_hStatus, wStatusText);
    }
}
