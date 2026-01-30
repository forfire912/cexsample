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
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>
#include <commdlg.h>
#include <oleauto.h>
#include <objbase.h>

// 判断字符串分隔符/空白字符（用于去除首尾分隔）
static int IsDelimW(WCHAR c) {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L',' || c == L';';
}
#include "ctp_trader.h"


#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

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
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// 主控件
#define IDC_TAB_CONTROL     1001
#define IDC_STATUS          1002

// 连接区域(测试环境)
#define IDC_BTN_CONNECT     1010
#define IDC_EDIT_BROKERID   1011
#define IDC_EDIT_USERID     1012
#define IDC_EDIT_PASSWORD   1013
#define IDC_EDIT_FRONTADDR  1014
#define IDC_EDIT_AUTHCODE   1015
#define IDC_EDIT_MD_FRONTADDR 1016

// 正式环境连接区域(设置页)
#define IDC_BTN_CONNECT_PROD     1020
#define IDC_EDIT_BROKERID_PROD   1021
#define IDC_EDIT_USERID_PROD     1022
#define IDC_EDIT_PASSWORD_PROD   1023
#define IDC_EDIT_FRONTADDR_PROD  1024
#define IDC_EDIT_AUTHCODE_PROD   1025
#define IDC_EDIT_MD_FRONTADDR_PROD 1026

// 查询环境选择
#define IDC_RADIO_ENV_TEST  1090
#define IDC_RADIO_ENV_PROD  1091

// TAB 1: 查询功能
#define IDC_QUERY_PANEL     1100
#define IDC_BTN_QUERY_ORDER 1101
#define IDC_BTN_QUERY_POS   1102
#define IDC_BTN_QUERY_MARKET 1103
#define IDC_BTN_QUERY_INST  1104
#define IDC_EDIT_QUERY_INSTRUMENT 1105
#define IDC_LISTVIEW_QUERY  1106
#define IDC_BTN_SUB_MD       1107
#define IDC_BTN_UNSUB_MD     1108
#define IDC_BTN_QUERY_OPTION 1109
#define IDC_BTN_IMPORT_CODES 1110
#define IDC_BTN_EXPORT_LATEST 1111

// TAB 2: 订阅功能
#define IDC_SUBSCRIBE_PANEL 1200
#define IDC_EDIT_SUB_INSTRUMENT 1201
#define IDC_RADIO_ENV_TEST_SUB 1202
#define IDC_RADIO_ENV_PROD_SUB 1203

// TAB 4: 系统设置

#define IDC_SETTINGS_PANEL  1400
#define IDC_EDIT_QUERY_MAX  1410

#define IDT_LOGIN_POLL_TEST 2001
#define IDT_LOGIN_POLL_PROD 2002
#define IDT_QUERY_TIMEOUT 2003
#define CONNECT_RETRY_DELAY_MS 10000
#define LOGIN_POLL_INTERVAL_MS 500
#define LOGIN_POLL_TIMEOUT_MS 20000
#define QUERY_MAX_DEFAULT 100
#define QUERY_MAX_LIMIT 10000

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
HWND g_hEditMdFrontAddr;
HWND g_hEditBrokerIDProd;
HWND g_hEditUserIDProd;
HWND g_hEditPasswordProd;
HWND g_hEditFrontAddrProd;
HWND g_hEditAuthCodeProd;
HWND g_hEditMdFrontAddrProd;

HWND g_hBtnConnect;
HWND g_hBtnConnectProd;

// TAB面板
HWND g_hQueryPanel;
HWND g_hSubscribePanel;

HWND g_hSettingsPanel;

HWND g_hEditQueryMax;
BOOL g_marketSubscribeMode = FALSE;
BOOL g_useProdEnv = FALSE;
BOOL g_syncingInstrumentText = FALSE;
BOOL g_queryInFlight = FALSE;
int g_queryInFlightType = 0;


// 查询面板控件
HWND g_hListViewQuery;
HWND g_hEditQueryInstrument;
HWND g_hRadioEnvTest;
HWND g_hRadioEnvProd;
HWND g_hEditSubscribeInstrument;
HWND g_hRadioEnvTestSub;
HWND g_hRadioEnvProdSub;
HWND g_hBtnImportCodes;
HWND g_hBtnExportLatest;
HWND g_hQueryControls[30];  // 查询面板所有控件数组
int g_nQueryControlCount = 0;
HWND g_hSubscribeControls[20];  // 订阅面板所有控件数组
int g_nSubscribeControlCount = 0;

// Excel 导入项：合约代码 + 行号（用于回写）
typedef struct ImportItem {
    WCHAR instrument[64];
    long row;
} ImportItem;

// Excel 导入缓存与文件路径状态
ImportItem* g_importItems = NULL;
int g_importCount = 0;
int g_importCap = 0;
WCHAR g_importExcelPath[MAX_PATH] = {0};

// 系统设置面板控件
HWND g_hSettingsControls[15];  // 系统设置面板所有控件数组
int g_nSettingsControlCount = 0;

// 测试/正式 Trader 实例指针
CTPTrader* g_pTraderTest = NULL;
CTPTrader* g_pTraderProd = NULL;

// 连接节流与登录轮询时间戳
ULONGLONG g_lastConnectAttemptTest = 0;
ULONGLONG g_lastConnectAttemptProd = 0;
ULONGLONG g_loginPollStartTest = 0;
ULONGLONG g_loginPollStartProd = 0;

// CTP AppID（登录认证使用）
const char* APP_ID = "client_long_1.0.0";

void CreateMainWindow(HWND hWnd, HINSTANCE hInstance);
void CreateQueryPanel(HWND hParent, HINSTANCE hInstance);
void CreateSubscribePanel(HWND hParent, HINSTANCE hInstance);
void CreateSettingsPanel(HWND hParent, HINSTANCE hInstance);
void SwitchTab(int tabIndex);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void UpdateStatus(const char* msg);
CTPTrader* GetActiveQueryTrader();
const char* GetActiveQueryEnvName();
static void GetActiveQueryMdFrontAddr(char* out, int outSize);
static void SyncEnvRadios(BOOL useProd);
static void SyncInstrumentTextFrom(HWND srcEdit);
static void SetInstrumentTextBoth(const WCHAR* text);
static BOOL BlockIfQueryInFlight(void);

static BOOL ImportInstrumentsFromExcel(HWND owner);
static BOOL ExportLatestToExcel(HWND owner);

static int ClampQueryMaxRecords(int value) {
    if (value < 1) return 1;
    if (value > QUERY_MAX_LIMIT) return QUERY_MAX_LIMIT;
    return value;
}

static void ApplyQueryMaxRecordsFromEdit(HWND hEdit) {
    if (!hEdit) return;
    WCHAR buf[32] = {0};
    GetWindowTextW(hEdit, buf, _countof(buf));
    int value = QUERY_MAX_DEFAULT;
    if (buf[0]) {
        WCHAR* end = NULL;
        long parsed = wcstol(buf, &end, 10);
        if (end != buf) {
            value = (int)parsed;
        }
    }
    value = ClampQueryMaxRecords(value);
    WCHAR out[32];
    swprintf_s(out, _countof(out), L"%d", value);
    if (wcscmp(buf, out) != 0) {
        SetWindowTextW(hEdit, out);
    }
    SetQueryMaxRecords(value);
}

// COM 接口安全释放
#define SAFE_RELEASE(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)

// 对 IDispatch 进行自动封装调用（属性/方法）
// 说明：AutoWrap 负责组装 DISPPARAMS 并调用 Invoke，屏蔽 COM 的繁琐参数管理。
static HRESULT AutoWrap(int autoType, VARIANT* pvResult, IDispatch* pDisp, LPOLESTR ptName, int cArgs, ...) {
    if (!pDisp) return E_INVALIDARG;
    DISPID dispid;
    // 根据名称获取 DISP ID
    HRESULT hr = pDisp->lpVtbl->GetIDsOfNames(pDisp, &IID_NULL, &ptName, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return hr;

    // 根据参数个数分配 VARIANT 数组
    VARIANT* pArgs = (VARIANT*)_alloca(sizeof(VARIANT) * cArgs);
    va_list marker;
    va_start(marker, cArgs);
    for (int i = 0; i < cArgs; i++) {
        pArgs[i] = va_arg(marker, VARIANT);
    }
    va_end(marker);

    // 组装 DISPPARAMS 供 Invoke 使用
    DISPPARAMS dp = {0};
    dp.cArgs = cArgs;
    dp.rgvarg = pArgs;

    DISPID dispidNamed = DISPID_PROPERTYPUT;
    if (autoType & DISPATCH_PROPERTYPUT) {
        dp.cNamedArgs = 1;
        dp.rgdispidNamedArgs = &dispidNamed;
    }

    // 调用 COM 方法/属性并返回结果
    return pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL, LOCALE_SYSTEM_DEFAULT, autoType, &dp, pvResult, NULL, NULL);
}

// 清空导入合约列表
// 说明：释放动态数组并重置计数，避免残留旧数据。
static void ClearImportItems(void) {
    if (g_importItems) {
        free(g_importItems);
    }
    g_importItems = NULL;
    g_importCount = 0;
    g_importCap = 0;
}

// 追加导入合约（自动扩容）
// 说明：按需扩容导入列表，并记录 Excel 行号用于回写。
static BOOL AddImportItem(const WCHAR* inst, long row) {
    if (!inst || !inst[0]) return FALSE;
    if (g_importCount >= g_importCap) {
        int newCap = g_importCap == 0 ? 64 : g_importCap * 2;
        ImportItem* next = (ImportItem*)realloc(g_importItems, sizeof(ImportItem) * newCap);
        if (!next) return FALSE;
        g_importItems = next;
        g_importCap = newCap;
    }
    wcsncpy(g_importItems[g_importCount].instrument, inst, _countof(g_importItems[g_importCount].instrument) - 1);
    g_importItems[g_importCount].instrument[_countof(g_importItems[g_importCount].instrument) - 1] = 0;
    g_importItems[g_importCount].row = row;
    g_importCount++;
    return TRUE;
}

// 原地去除首尾空白/分隔符
// 说明：仅处理首尾，不修改中间内容，适用于用户输入/Excel 单元格。
static void TrimInPlaceW(WCHAR* s) {
    if (!s) return;
    int len = (int)wcslen(s);
    int start = 0;
    while (start < len && IsDelimW(s[start])) start++;
    int end = len;
    while (end > start && IsDelimW(s[end - 1])) end--;
    if (start > 0) {
        memmove(s, s + start, (end - start) * sizeof(WCHAR));
    }
    s[end - start] = 0;
}

// 打开 Excel 并获取 base 工作表（COM 自动化）
// 说明：创建 Excel.Application，打开文件后取名为 "base" 的工作表。
static BOOL ExcelOpenBaseSheet(const WCHAR* path, IDispatch** outApp, IDispatch** outBook, IDispatch** outSheet) {
    if (!path || !outApp || !outBook || !outSheet) return FALSE;
    *outApp = NULL;
    *outBook = NULL;
    *outSheet = NULL;

    // 获取 Excel.Application 的 CLSID
    CLSID clsid;
    if (FAILED(CLSIDFromProgID(L"Excel.Application", &clsid))) return FALSE;

    // Excel COM 对象指针
    IDispatch* app = NULL;
    // 创建 Excel 应用实例（后台）
    HRESULT hr = CoCreateInstance(&clsid, NULL, CLSCTX_LOCAL_SERVER, &IID_IDispatch, (void**)&app);
    if (FAILED(hr) || !app) return FALSE;

    VARIANT vFalse; VariantInit(&vFalse); vFalse.vt = VT_BOOL; vFalse.boolVal = VARIANT_FALSE;
    AutoWrap(DISPATCH_PROPERTYPUT, NULL, app, L"Visible", 1, vFalse);
    AutoWrap(DISPATCH_PROPERTYPUT, NULL, app, L"DisplayAlerts", 1, vFalse);

    // 准备接收 COM 调用返回值
    VARIANT result; VariantInit(&result);
    // 获取 Workbooks 集合
    hr = AutoWrap(DISPATCH_PROPERTYGET, &result, app, L"Workbooks", 0);
    if (FAILED(hr) || result.vt != VT_DISPATCH || !result.pdispVal) {
        SAFE_RELEASE(app);
        return FALSE;
    }
    IDispatch* workbooks = result.pdispVal;

    VARIANT vPath; VariantInit(&vPath); vPath.vt = VT_BSTR; vPath.bstrVal = SysAllocString(path);
    VariantInit(&result);
    // 打开指定 Excel 文件
    hr = AutoWrap(DISPATCH_METHOD, &result, workbooks, L"Open", 1, vPath);
    VariantClear(&vPath);
    SAFE_RELEASE(workbooks);
    if (FAILED(hr) || result.vt != VT_DISPATCH || !result.pdispVal) {
        SAFE_RELEASE(app);
        return FALSE;
    }
    IDispatch* book = result.pdispVal;

    VariantInit(&result);
    // 获取 Worksheets 集合
    hr = AutoWrap(DISPATCH_PROPERTYGET, &result, book, L"Worksheets", 0);
    if (FAILED(hr) || result.vt != VT_DISPATCH || !result.pdispVal) {
        SAFE_RELEASE(book);
        SAFE_RELEASE(app);
        return FALSE;
    }
    IDispatch* sheets = result.pdispVal;

    VARIANT vSheet; VariantInit(&vSheet); vSheet.vt = VT_BSTR; vSheet.bstrVal = SysAllocString(L"base");
    VariantInit(&result);
    // 获取名为 base 的工作表
    hr = AutoWrap(DISPATCH_PROPERTYGET, &result, sheets, L"Item", 1, vSheet);
    VariantClear(&vSheet);
    SAFE_RELEASE(sheets);
    if (FAILED(hr) || result.vt != VT_DISPATCH || !result.pdispVal) {
        SAFE_RELEASE(book);
        SAFE_RELEASE(app);
        return FALSE;
    }

    *outApp = app;
    *outBook = book;
    *outSheet = result.pdispVal;
    return TRUE;
}

// 关闭 Excel 并释放 COM 对象
// 说明：保证按顺序关闭工作簿/应用并释放引用计数。
static void ExcelClose(IDispatch* app, IDispatch* book) {
    if (book) {
        AutoWrap(DISPATCH_METHOD, NULL, book, L"Close", 0);
    }
    if (app) {
        AutoWrap(DISPATCH_METHOD, NULL, app, L"Quit", 0);
    }
    SAFE_RELEASE(book);
    SAFE_RELEASE(app);
}

// 从 Excel(base 页签 B 列)导入合约代码
static BOOL ImportInstrumentsFromExcel(HWND owner) {
    // 弹出文件对话框选择 Excel 文件
    OPENFILENAMEW ofn = {0};
    WCHAR filePath[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    // 设置文件过滤器（仅 Excel）
    ofn.lpstrFilter = L"Excel Files (*.xlsx;*.xls;*.xlsm)\0*.xlsx;*.xls;*.xlsm\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) {
        return FALSE;
    }

    // 重置导入状态，确保本次导入干净
    ClearImportItems();

    IDispatch* app = NULL;
    IDispatch* book = NULL;
    IDispatch* sheet = NULL;
    // 打开 Excel 并定位 base 页签
    if (!ExcelOpenBaseSheet(filePath, &app, &book, &sheet)) {
        MessageBox(owner, TEXT("无法打开Excel文件或未找到base页签"), TEXT("导入失败"), MB_ICONERROR | MB_OK);
        return FALSE;
    }

    // 逐行读取 B 列，最多读取 200 行（可根据模板调整）
    // 按行扫描合约列（B 列）
    for (long row = 1; row <= 200; row++) {
        WCHAR cellRef[16];
        // 读取 base 页 B 列合约代码
        swprintf_s(cellRef, _countof(cellRef), L"B%ld", row);

        VARIANT vCell; VariantInit(&vCell); vCell.vt = VT_BSTR; vCell.bstrVal = SysAllocString(cellRef);
        VARIANT result; VariantInit(&result);
        // 获取指定单元格 Range 对象
        HRESULT hr = AutoWrap(DISPATCH_PROPERTYGET, &result, sheet, L"Range", 1, vCell);
        VariantClear(&vCell);
        if (FAILED(hr) || result.vt != VT_DISPATCH || !result.pdispVal) {
            // 释放 VARIANT 内存，避免泄露
            VariantClear(&result);
            break;
        }
        IDispatch* cell = result.pdispVal;

        VariantInit(&result);
        // 读取单元格 Value 内容
        hr = AutoWrap(DISPATCH_PROPERTYGET, &result, cell, L"Value", 0);
        SAFE_RELEASE(cell);
        if (FAILED(hr)) {
            // 释放 VARIANT 内存，避免泄露
            VariantClear(&result);
            break;
        }

        // 将单元格值转换为字符串并去除首尾空白
        WCHAR buf[128] = {0};
        if (result.vt == VT_BSTR && result.bstrVal) {
            wcsncpy(buf, result.bstrVal, _countof(buf) - 1);
        } else if (result.vt == VT_R8) {
            swprintf_s(buf, _countof(buf), L"%.8g", result.dblVal);
        } else if (result.vt == VT_I4) {
            swprintf_s(buf, _countof(buf), L"%d", (int)result.lVal);
        }
        // 释放 VARIANT 内存，避免泄露
        VariantClear(&result);

        TrimInPlaceW(buf);
        if (buf[0] == 0) {
            continue;
        }
        AddImportItem(buf, row);
    }

    SAFE_RELEASE(sheet);
    ExcelClose(app, book);

    if (g_importCount == 0) {
        MessageBox(owner, TEXT("base页签的B列没有找到合约代码"), TEXT("导入失败"), MB_ICONWARNING | MB_OK);
        return FALSE;
    }

    // 缓存 Excel 路径，供导出回写使用
    wcsncpy(g_importExcelPath, filePath, _countof(g_importExcelPath) - 1);
    g_importExcelPath[_countof(g_importExcelPath) - 1] = 0;

    // 将导入的合约拼成 "A, B, C" 形式放回输入框
    int totalLen = 0;
    for (int i = 0; i < g_importCount; i++) {
        totalLen += (int)wcslen(g_importItems[i].instrument) + 2;
    }
    // 拼接合约列表为逗号分隔字符串
    WCHAR* buf = (WCHAR*)malloc((totalLen + 1) * sizeof(WCHAR));
    if (!buf) {
        MessageBox(owner, TEXT("内存不足"), TEXT("导入失败"), MB_ICONERROR | MB_OK);
        return FALSE;
    }
    buf[0] = 0;
    int offset = 0;
    for (int i = 0; i < g_importCount; i++) {
        const WCHAR* inst = g_importItems[i].instrument;
        int len = (int)wcslen(inst);
        memcpy(buf + offset, inst, len * sizeof(WCHAR));
        offset += len;
        if (i < g_importCount - 1) {
            buf[offset++] = L',';
            buf[offset++] = L' ';
        }
    }
    buf[offset] = 0;

    SetInstrumentTextBoth(buf);
    free(buf);

    MessageBox(owner, TEXT("已导入合约代码"), TEXT("导入成功"), MB_ICONINFORMATION | MB_OK);
    return TRUE;
}

// 在行情列表中查找合约最新价（用于导出）
static BOOL TryGetListViewPrice(const WCHAR* instrument, double* outPrice) {
    if (!g_hListViewQuery || !instrument || !outPrice) return FALSE;
    // ListView 列索引：合约列/价格列
    int instCol = 1;  // left 2nd column
    int priceCol = 8; // left 9th column
    WCHAR want[64] = {0};
    wcsncpy(want, instrument, _countof(want) - 1);
    want[_countof(want) - 1] = 0;
    TrimInPlaceW(want);
    CharUpperBuffW(want, (DWORD)wcslen(want));
    if (want[0] == 0) return FALSE;

    // 将输入合约规整成仅含数字/大写字母的形式，便于匹配
    WCHAR wantNorm[64] = {0};
    int wn = 0;
    // 规整输入合约为字母数字便于匹配
    for (int i = 0; want[i] && wn < (int)_countof(wantNorm) - 1; i++) {
        if ((want[i] >= L'0' && want[i] <= L'9') || (want[i] >= L'A' && want[i] <= L'Z')) {
            wantNorm[wn++] = want[i];
        }
    }
    wantNorm[wn] = 0;
    if (wantNorm[0] == 0) return FALSE;
    int itemCount = ListView_GetItemCount(g_hListViewQuery);
    for (int i = 0; i < itemCount; i++) {
        WCHAR instBuf[64] = {0};
        ListView_GetItemText(g_hListViewQuery, i, instCol, instBuf, _countof(instBuf));
        TrimInPlaceW(instBuf);
        CharUpperBuffW(instBuf, (DWORD)wcslen(instBuf));

        // ListView 中的合约也做同样规整，支持子串匹配
        WCHAR instNorm[64] = {0};
        int in = 0;
        for (int j = 0; instBuf[j] && in < (int)_countof(instNorm) - 1; j++) {
            if ((instBuf[j] >= L'0' && instBuf[j] <= L'9') || (instBuf[j] >= L'A' && instBuf[j] <= L'Z')) {
                instNorm[in++] = instBuf[j];
            }
        }
        instNorm[in] = 0;

        if (instNorm[0] == 0) {
            continue;
        }

        // 双向子串匹配，兼容前后缀差异
        if (wcsstr(instNorm, wantNorm) != NULL || wcsstr(wantNorm, instNorm) != NULL) {
            WCHAR priceBuf[64] = {0};
            ListView_GetItemText(g_hListViewQuery, i, priceCol, priceBuf, _countof(priceBuf));
            TrimInPlaceW(priceBuf);
            if (wcscmp(priceBuf, L"--") == 0) return FALSE;
            if (priceBuf[0] == 0) return FALSE;
            WCHAR* endPtr = NULL;
            // 解析价格字符串为 double
            double v = wcstod(priceBuf, &endPtr);
            if (endPtr == priceBuf) return FALSE;
            *outPrice = v;
            return TRUE;
        }
    }
    return FALSE;
}

// 将已导入合约的最新价写回 Excel(base 页签 I 列)
static BOOL ExportLatestToExcel(HWND owner) {
    if (g_importExcelPath[0] == 0 || g_importCount == 0) {
        MessageBox(owner, TEXT("请先导入合约代码"), TEXT("导出失败"), MB_ICONWARNING | MB_OK);
        return FALSE;
    }

    IDispatch* app = NULL;
    IDispatch* book = NULL;
    IDispatch* sheet = NULL;
    if (!ExcelOpenBaseSheet(g_importExcelPath, &app, &book, &sheet)) {
        MessageBox(owner, TEXT("无法打开Excel文件或未找到base页签"), TEXT("导出失败"), MB_ICONERROR | MB_OK);
        return FALSE;
    }

    // 统计写入成功与缺失行情的条数，便于提示
    int written = 0;
    int missing = 0;
    for (int i = 0; i < g_importCount; i++) {
        // 从 ListView 获取该合约最新价
        double price = 0.0;
        if (!TryGetListViewPrice(g_importItems[i].instrument, &price)) {
            missing++;
            continue;
        }

        WCHAR cellRef[16];
        // I 列为回写目标（同一行号）
        swprintf_s(cellRef, _countof(cellRef), L"I%ld", g_importItems[i].row);
        VARIANT vCell; VariantInit(&vCell); vCell.vt = VT_BSTR; vCell.bstrVal = SysAllocString(cellRef);
        VARIANT result; VariantInit(&result);
        HRESULT hr = AutoWrap(DISPATCH_PROPERTYGET, &result, sheet, L"Range", 1, vCell);
        VariantClear(&vCell);
        if (FAILED(hr) || result.vt != VT_DISPATCH || !result.pdispVal) {
            // 释放 VARIANT 内存，避免泄露
            VariantClear(&result);
            continue;
        }
        IDispatch* cell = result.pdispVal;

        VARIANT vVal; VariantInit(&vVal); vVal.vt = VT_R8; vVal.dblVal = price;
        AutoWrap(DISPATCH_PROPERTYPUT, NULL, cell, L"Value", 1, vVal);
        SAFE_RELEASE(cell);
        written++;
    }

    AutoWrap(DISPATCH_METHOD, NULL, book, L"Save", 0);
    SAFE_RELEASE(sheet);
    ExcelClose(app, book);

    WCHAR msg[128];
    swprintf_s(msg, _countof(msg), L"已写入 %d 条，未找到行情 %d 条", written, missing);
    MessageBox(owner, msg, TEXT("导出完成"), MB_ICONINFORMATION | MB_OK);
    return TRUE;
}

// 根据连接状态切换按钮文案
static void SetConnectButtonText(HWND hButton, BOOL loggedIn, BOOL isProd) {
    if (!hButton) return;
    if (loggedIn) {
        // 已登录时显示“退出连接”
        SetWindowText(hButton, TEXT("退出连接"));
    } else {
        // 未登录时显示“连接登录”
        SetWindowText(hButton, isProd ? TEXT("连接登录(正式)") : TEXT("连接登录"));
    }
}

// 启动登录状态轮询定时器
static void StartLoginPollTimer(BOOL isProd) {
    if (!g_hMainWnd) return;
    if (isProd) {
        // 记录轮询起始时间并启动正式环境定时器
        g_loginPollStartProd = GetTickCount64();
        SetTimer(g_hMainWnd, IDT_LOGIN_POLL_PROD, LOGIN_POLL_INTERVAL_MS, NULL);
    } else {
        // 记录轮询起始时间并启动测试环境定时器
        g_loginPollStartTest = GetTickCount64();
        SetTimer(g_hMainWnd, IDT_LOGIN_POLL_TEST, LOGIN_POLL_INTERVAL_MS, NULL);
    }
}

// 全局异常过滤器（防止崩溃无提示）
LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* pExceptionInfo) {
    MessageBox(NULL, TEXT("发生了异常错误\n请联系管理员"), TEXT("错误"), MB_ICONERROR | MB_OK);
    return EXCEPTION_EXECUTE_HANDLER;
}

// 程序入口：初始化 COM、控件、窗口并进入消息循环
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化 COM（用于 Excel 自动化）
    HRESULT hrCo = CoInitialize(NULL);
    // 注册全局异常处理，避免无提示崩溃
    SetUnhandledExceptionFilter(ExceptionFilter);
    g_hInst = hInstance;
    // 初始化常用控件（ListView 等）
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
    // 注册主窗口类
    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, TEXT("窗口类注册失败"), TEXT("错误"), MB_ICONERROR | MB_OK);
        return 0;
    }
    // 创建主窗口实例
    g_hMainWnd = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("CTPTraderClass"), TEXT("CTP交易系统"),
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 700,
                                NULL, NULL, hInstance, NULL);
    if (g_hMainWnd == NULL) {
        MessageBox(NULL, TEXT("主窗口创建失败"), TEXT("错误"), MB_ICONERROR | MB_OK);
        return 0;
    }
    // 显示并刷新窗口
    ShowWindow(g_hMainWnd, nCmdShow);
    // 触发首次 WM_PAINT
    UpdateWindow(g_hMainWnd);
    // 标准消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    // 释放 COM 初始化
    if (SUCCEEDED(hrCo)) {
        CoUninitialize();
    }
    return (int)msg.wParam;
}

// 面板窗口过程 - 转发WM_COMMAND到主窗口
WNDPROC g_oldPanelProc = NULL;
// 子面板窗口过程：转发消息或绘制处理
LRESULT CALLBACK PanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // 将面板按钮消息转发给主窗口统一处理
    if (message == WM_COMMAND) {
        // 转发WM_COMMAND到主窗口
        return SendMessage(g_hMainWnd, WM_COMMAND, wParam, lParam);
    }
    // 非 WM_COMMAND 消息交给原窗口过程
    return CallWindowProc(g_oldPanelProc, hWnd, message, wParam, lParam);
}

// 创建主窗口 UI（Tab/状态栏/连接区）
void CreateMainWindow(HWND hWnd, HINSTANCE hInstance) {
    // ========== Connection Area ==========
    int y = 10;

    // Row 1: 交易前置/行情前置/认证码（默认值为测试环境示例）
    CreateWindow(TEXT("STATIC"), TEXT("交易前置:"), WS_VISIBLE | WS_CHILD,
                 10, y, 70, 24, hWnd, NULL, hInstance, NULL);
    g_hEditFrontAddr = CreateWindow(TEXT("EDIT"), TEXT("tcp://106.37.101.162:31205"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                 80, y, 250, 24, hWnd, (HMENU)IDC_EDIT_FRONTADDR, hInstance, NULL);

    CreateWindow(TEXT("STATIC"), TEXT("行情前置:"), WS_VISIBLE | WS_CHILD,
                 340, y, 70, 24, hWnd, NULL, hInstance, NULL);
    g_hEditMdFrontAddr = CreateWindow(TEXT("EDIT"), TEXT("tcp://106.37.101.162:31213"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                 410, y, 250, 24, hWnd, (HMENU)IDC_EDIT_MD_FRONTADDR, hInstance, NULL);

    CreateWindow(TEXT("STATIC"), TEXT("认证码:"), WS_VISIBLE | WS_CHILD,
                 670, y, 60, 24, hWnd, NULL, hInstance, NULL);
    g_hEditAuthCode = CreateWindow(TEXT("EDIT"), TEXT("YHQHYHQHYHQHYHQH"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                 730, y, 150, 24, hWnd, (HMENU)IDC_EDIT_AUTHCODE, hInstance, NULL);

    // Row 2: 经纪商/用户/密码/连接按钮（测试环境默认值）
    y += 35;
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

    CreateWindow(TEXT("BUTTON"), TEXT("连接登录"),
                 WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 460, y-2, 120, 30, hWnd, (HMENU)IDC_BTN_CONNECT, hInstance, NULL);

    // ========== TAB ==========
    // 创建 TAB 控件作为主功能区容器
    // 主功能区：Tab 控件
    g_hTabControl = CreateWindowEx(0, WC_TABCONTROL, TEXT(""),
                 WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
                 10, 90, 1160, 520, hWnd, (HMENU)IDC_TAB_CONTROL, hInstance, NULL);

    // 添加各个 TAB 页签
    TCITEM tie;
    tie.mask = TCIF_TEXT;

    tie.pszText = TEXT("查 询");
    TabCtrl_InsertItem(g_hTabControl, 0, &tie);

    tie.pszText = TEXT("行情订阅");
    TabCtrl_InsertItem(g_hTabControl, 1, &tie);

    tie.pszText = TEXT("系统设置");
    TabCtrl_InsertItem(g_hTabControl, 2, &tie);

    // 创建各 TAB 对应的面板内容
    CreateQueryPanel(hWnd, hInstance);
    CreateSubscribePanel(hWnd, hInstance);
    CreateSettingsPanel(hWnd, hInstance);

    SwitchTab(0);

    // 底部状态栏，用于显示连接/查询状态
    g_hStatus = CreateWindow(TEXT("STATIC"), TEXT("状态: 未连接"),
                 WS_VISIBLE | WS_CHILD | SS_LEFT,
                 10, 620, 1160, 20, hWnd, (HMENU)IDC_STATUS, hInstance, NULL);
}
// 创建查询面板 UI（列表、按钮、输入框）
void CreateQueryPanel(HWND hParent, HINSTANCE hInstance) {
    // 获取 TAB 控件的客户区（用于放置面板）
    // 计算 TAB 客户区用于摆放设置面板
    RECT rcTab;
    GetClientRect(g_hTabControl, &rcTab);
    TabCtrl_AdjustRect(g_hTabControl, FALSE, &rcTab);
    
    // 创建查询面板容器（控件多数放在主窗口，但容器用于布局/可见性）
    g_hQueryPanel = CreateWindow(TEXT("STATIC"), TEXT(""),
                 WS_CHILD | WS_VISIBLE,
                 rcTab.left + 10, rcTab.top + 10, 
                 rcTab.right - rcTab.left - 20, rcTab.bottom - rcTab.top - 20,
                 g_hTabControl, (HMENU)IDC_QUERY_PANEL, hInstance, NULL);
    
    // 设置面板子类化：把面板内 WM_COMMAND 转发到主窗口
    if (g_oldPanelProc == NULL) {
        g_oldPanelProc = (WNDPROC)GetWindowLongPtr(g_hQueryPanel, GWLP_WNDPROC);
    }
    SetWindowLongPtr(g_hQueryPanel, GWLP_WNDPROC, (LONG_PTR)PanelProc);
    
    // 统一布局参数（边距/行高）
    int x = 15;  // 左侧边距
    int y = 10;  // 顶部边距
    
    // 快速查询按钮区（直接触发交易/行情查询接口）
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("快速查询:"), WS_CHILD, 
                 x, y, 80, 20, g_hQueryPanel, NULL, hInstance, NULL);
    
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询合约"), WS_CHILD | BS_PUSHBUTTON,
                 x+85, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_INST, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询行情"), WS_CHILD | BS_PUSHBUTTON,
                 x+185, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_MARKET, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询期权"), WS_CHILD | BS_PUSHBUTTON,
                 x+285, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_OPTION, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询委托"), WS_CHILD | BS_PUSHBUTTON,
                 x+385, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_ORDER, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("查询持仓"), WS_CHILD | BS_PUSHBUTTON,
                 x+485, y-2, 90, 28, g_hQueryPanel, (HMENU)IDC_BTN_QUERY_POS, hInstance, NULL);

    // 环境选择（测试/正式），影响查询使用的 Trader 实例
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("环境:"), WS_CHILD,
                 x+590, y+3, 50, 20, g_hQueryPanel, NULL, hInstance, NULL);
    g_hRadioEnvTest = CreateWindow(TEXT("BUTTON"), TEXT("测试"), WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                 x+635, y-2, 60, 24, g_hQueryPanel, (HMENU)IDC_RADIO_ENV_TEST, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = g_hRadioEnvTest;
    g_hRadioEnvProd = CreateWindow(TEXT("BUTTON"), TEXT("正式"), WS_CHILD | BS_AUTORADIOBUTTON,
                 x+700, y-2, 60, 24, g_hQueryPanel, (HMENU)IDC_RADIO_ENV_PROD, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = g_hRadioEnvProd;
    SyncEnvRadios(FALSE);
    y += 35;
    
    // 合约输入区：支持多行、逗号分隔
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("合约代码:"), WS_CHILD, 
                 x, y+3, 70, 20, g_hQueryPanel, NULL, hInstance, NULL);
        g_hEditQueryInstrument = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), 
                 WS_CHILD | WS_BORDER | ES_UPPERCASE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 
                 x+75, y, 340, 70, g_hQueryPanel, (HMENU)IDC_EDIT_QUERY_INSTRUMENT, hInstance, NULL);
    SendMessage(g_hEditQueryInstrument, EM_LIMITTEXT, 32767, 0);
    g_hQueryControls[g_nQueryControlCount++] = g_hEditQueryInstrument;

    // Excel 导入/导出按钮
    g_hBtnImportCodes = CreateWindow(TEXT("BUTTON"), TEXT("导入合约代码"), WS_CHILD | BS_PUSHBUTTON,
                 x+430, y-2, 120, 28, g_hQueryPanel, (HMENU)IDC_BTN_IMPORT_CODES, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = g_hBtnImportCodes;
    g_hBtnExportLatest = CreateWindow(TEXT("BUTTON"), TEXT("导出最新值"), WS_CHILD | BS_PUSHBUTTON,
                 x+555, y-2, 120, 28, g_hQueryPanel, (HMENU)IDC_BTN_EXPORT_LATEST, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = g_hBtnExportLatest;
    
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("(点击下方列表可自动填充)"), WS_CHILD, 
                 x+75, y+50, 300, 20, g_hQueryPanel, NULL, hInstance, NULL);
    y += 80;
    
    // 查询结果区（ListView 展示行情/委托/持仓等）
    g_hQueryControls[g_nQueryControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("查询结果:"), WS_CHILD, 
                 x, y, 80, 20, g_hQueryPanel, NULL, hInstance, NULL);
    y += 25;
    
    // ListView 直接创建在主窗口下，这样 WM_NOTIFY 可以正确传递
    // 计算 ListView 相对于主窗口的实际坐标
    // 计算 ListView 相对主窗口位置
    POINT pt = {0, 0};
    ClientToScreen(g_hTabControl, &pt);
    ScreenToClient(hParent, &pt);
    
    int listViewX = pt.x + x;
    int listViewY = pt.y + y + 10;
    int listViewWidth = (rcTab.right - rcTab.left) - 20;
    int listViewHeight = (rcTab.bottom - rcTab.top) - (y + 10) - 10;
    if (listViewHeight < 120) {
        listViewHeight = 120;
    }
    
    // 创建结果列表（报表模式，支持整行选中）
    g_hListViewQuery = CreateWindowEx(0, WC_LISTVIEW, TEXT(""),
                 WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                 listViewX, listViewY, listViewWidth, listViewHeight, 
                 hParent, (HMENU)IDC_LISTVIEW_QUERY, hInstance, NULL);
    g_hQueryControls[g_nQueryControlCount++] = g_hListViewQuery;
    // 设置列表样式：整行选中+网格线
    ListView_SetExtendedListViewStyle(g_hListViewQuery, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
}

// 创建订阅面板 UI（订阅按钮、输入框）
void CreateSubscribePanel(HWND hParent, HINSTANCE hInstance) {
    RECT rcTab;
    GetClientRect(g_hTabControl, &rcTab);
    TabCtrl_AdjustRect(g_hTabControl, FALSE, &rcTab);

    g_hSubscribePanel = CreateWindow(TEXT("STATIC"), TEXT(""),
                 WS_CHILD,
                 rcTab.left + 10, rcTab.top + 10,
                 rcTab.right - rcTab.left - 20, rcTab.bottom - rcTab.top - 20,
                 g_hTabControl, (HMENU)IDC_SUBSCRIBE_PANEL, hInstance, NULL);
    g_hSubscribeControls[g_nSubscribeControlCount++] = g_hSubscribePanel;

    if (g_oldPanelProc == NULL) {
        g_oldPanelProc = (WNDPROC)GetWindowLongPtr(g_hSubscribePanel, GWLP_WNDPROC);
    }
    SetWindowLongPtr(g_hSubscribePanel, GWLP_WNDPROC, (LONG_PTR)PanelProc);

    int x = 15;
    int y = 10;

    g_hSubscribeControls[g_nSubscribeControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("订阅操作:"), WS_CHILD,
                 x, y, 80, 20, g_hSubscribePanel, NULL, hInstance, NULL);
    g_hSubscribeControls[g_nSubscribeControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("订阅行情"), WS_CHILD | BS_PUSHBUTTON,
                 x+85, y-2, 90, 28, g_hSubscribePanel, (HMENU)IDC_BTN_SUB_MD, hInstance, NULL);
    g_hSubscribeControls[g_nSubscribeControlCount++] = CreateWindow(TEXT("BUTTON"), TEXT("取消订阅"), WS_CHILD | BS_PUSHBUTTON,
                 x+185, y-2, 90, 28, g_hSubscribePanel, (HMENU)IDC_BTN_UNSUB_MD, hInstance, NULL);

    // 环境选择（测试/正式）
    g_hSubscribeControls[g_nSubscribeControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("环境:"), WS_CHILD,
                 x+590, y+3, 50, 20, g_hSubscribePanel, NULL, hInstance, NULL);
    g_hRadioEnvTestSub = CreateWindow(TEXT("BUTTON"), TEXT("测试"), WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                 x+635, y-2, 60, 24, g_hSubscribePanel, (HMENU)IDC_RADIO_ENV_TEST_SUB, hInstance, NULL);
    g_hSubscribeControls[g_nSubscribeControlCount++] = g_hRadioEnvTestSub;
    g_hRadioEnvProdSub = CreateWindow(TEXT("BUTTON"), TEXT("正式"), WS_CHILD | BS_AUTORADIOBUTTON,
                 x+700, y-2, 60, 24, g_hSubscribePanel, (HMENU)IDC_RADIO_ENV_PROD_SUB, hInstance, NULL);
    g_hSubscribeControls[g_nSubscribeControlCount++] = g_hRadioEnvProdSub;
    SyncEnvRadios(g_useProdEnv);

    y += 35;

    g_hSubscribeControls[g_nSubscribeControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("合约代码:"), WS_CHILD,
                 x, y+3, 70, 20, g_hSubscribePanel, NULL, hInstance, NULL);
    g_hEditSubscribeInstrument = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
                 WS_CHILD | WS_BORDER | ES_UPPERCASE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                 x+75, y, 340, 70, g_hSubscribePanel, (HMENU)IDC_EDIT_SUB_INSTRUMENT, hInstance, NULL);
    SendMessage(g_hEditSubscribeInstrument, EM_LIMITTEXT, 32767, 0);
    g_hSubscribeControls[g_nSubscribeControlCount++] = g_hEditSubscribeInstrument;

    g_hSubscribeControls[g_nSubscribeControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("(输入合约代码后点击订阅)"), WS_CHILD,
                 x+75, y+50, 300, 20, g_hSubscribePanel, NULL, hInstance, NULL);

    y += 80;
    g_hSubscribeControls[g_nSubscribeControlCount++] = CreateWindow(TEXT("STATIC"), TEXT("订阅结果:"), WS_CHILD,
                 x, y, 80, 20, g_hSubscribePanel, NULL, hInstance, NULL);
}

// ========== 创建持仓管理面板 ==========
// ========== 创建系统设置面板 ==========
// 创建系统设置面板 UI
void CreateSettingsPanel(HWND hParent, HINSTANCE hInstance) {
    RECT rcTab;
    GetClientRect(g_hTabControl, &rcTab);
    TabCtrl_AdjustRect(g_hTabControl, FALSE, &rcTab);
    
    // 创建设置面板容器（实际控件挂在其下）
    g_hSettingsPanel = CreateWindow(TEXT("STATIC"), TEXT(""),
                 WS_CHILD,
                 rcTab.left + 10, rcTab.top + 10, 
                 rcTab.right - rcTab.left - 20, rcTab.bottom - rcTab.top - 20,
                 g_hTabControl, (HMENU)IDC_SETTINGS_PANEL, hInstance, NULL);
    
    // 子类化：把面板内 WM_COMMAND 转发给主窗口
    if (g_oldPanelProc == NULL) {
        g_oldPanelProc = (WNDPROC)GetWindowLongPtr(g_hSettingsPanel, GWLP_WNDPROC);
    }
    SetWindowLongPtr(g_hSettingsPanel, GWLP_WNDPROC, (LONG_PTR)PanelProc);
    
    int x = 30, y = 15;

    // 正式环境连接区：用于单独配置正式账号
    CreateWindow(TEXT("STATIC"), TEXT("【 正式环境查询连接 】"),
                 WS_VISIBLE | WS_CHILD,
                 x, y, 220, 25, g_hSettingsPanel, NULL, hInstance, NULL);
    y += 30;

    CreateWindow(TEXT("STATIC"), TEXT("交易前置:"), WS_VISIBLE | WS_CHILD,
                 x, y, 70, 22, g_hSettingsPanel, NULL, hInstance, NULL);
    g_hEditFrontAddrProd = CreateWindow(TEXT("EDIT"), TEXT("tcp://58.247.171.151:31205"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                 x+75, y-2, 240, 24, g_hSettingsPanel, (HMENU)IDC_EDIT_FRONTADDR_PROD, hInstance, NULL);

    CreateWindow(TEXT("STATIC"), TEXT("行情前置:"), WS_VISIBLE | WS_CHILD,
                 x+325, y, 70, 22, g_hSettingsPanel, NULL, hInstance, NULL);
    g_hEditMdFrontAddrProd = CreateWindow(TEXT("EDIT"), TEXT("tcp://58.247.171.151:31213"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                 x+395, y-2, 240, 24, g_hSettingsPanel, (HMENU)IDC_EDIT_MD_FRONTADDR_PROD, hInstance, NULL);

    CreateWindow(TEXT("STATIC"), TEXT("认证码:"), WS_VISIBLE | WS_CHILD,
                 x+645, y, 55, 22, g_hSettingsPanel, NULL, hInstance, NULL);
    g_hEditAuthCodeProd = CreateWindow(TEXT("EDIT"), TEXT("AM8P99PA7UPUXREW"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                 x+700, y-2, 150, 24, g_hSettingsPanel, (HMENU)IDC_EDIT_AUTHCODE_PROD, hInstance, NULL);

    y += 30;
    CreateWindow(TEXT("STATIC"), TEXT("经纪商ID:"), WS_VISIBLE | WS_CHILD,
                 x, y, 70, 22, g_hSettingsPanel, NULL, hInstance, NULL);
    g_hEditBrokerIDProd = CreateWindow(TEXT("EDIT"), TEXT("4040"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                 x+75, y-2, 80, 24, g_hSettingsPanel, (HMENU)IDC_EDIT_BROKERID_PROD, hInstance, NULL);

    CreateWindow(TEXT("STATIC"), TEXT("用户ID:"), WS_VISIBLE | WS_CHILD,
                 x+165, y, 55, 22, g_hSettingsPanel, NULL, hInstance, NULL);
    g_hEditUserIDProd = CreateWindow(TEXT("EDIT"), TEXT("0022868599"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                 x+220, y-2, 100, 24, g_hSettingsPanel, (HMENU)IDC_EDIT_USERID_PROD, hInstance, NULL);

    CreateWindow(TEXT("STATIC"), TEXT("密码:"), WS_VISIBLE | WS_CHILD,
                 x+330, y, 40, 22, g_hSettingsPanel, NULL, hInstance, NULL);
    g_hEditPasswordProd = CreateWindow(TEXT("EDIT"), TEXT("XXXX"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD,
                 x+370, y-2, 120, 24, g_hSettingsPanel, (HMENU)IDC_EDIT_PASSWORD_PROD, hInstance, NULL);

    CreateWindow(TEXT("BUTTON"), TEXT("连接登录(正式)"),
                 WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                 x+505, y-2, 140, 26, g_hSettingsPanel, (HMENU)IDC_BTN_CONNECT_PROD, hInstance, NULL);

    y += 45;

    // 查询设置
    CreateWindow(TEXT("STATIC"), TEXT("【 查询设置 】"),
                 WS_VISIBLE | WS_CHILD,
                 x, y, 200, 30, g_hSettingsPanel, NULL, hInstance, NULL);
    y += 35;

    CreateWindow(TEXT("STATIC"), TEXT("最大记录数:"), WS_VISIBLE | WS_CHILD,
                 x, y, 80, 22, g_hSettingsPanel, NULL, hInstance, NULL);
    g_hEditQueryMax = CreateWindow(TEXT("EDIT"), TEXT("100"),
                 WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                 x+85, y-2, 80, 24, g_hSettingsPanel, (HMENU)IDC_EDIT_QUERY_MAX, hInstance, NULL);
    SendMessage(g_hEditQueryMax, EM_LIMITTEXT, 5, 0);
    ApplyQueryMaxRecordsFromEdit(g_hEditQueryMax);
    CreateWindow(TEXT("STATIC"), TEXT("默认100，最大10000"), WS_VISIBLE | WS_CHILD,
                 x+175, y, 150, 22, g_hSettingsPanel, NULL, hInstance, NULL);

    y += 40;

    // 系统信息展示（版本/接口/编译日期）

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
// 切换 Tab 页并显示对应面板
// 根据 tabIndex 显示对应面板，隐藏其它控件
void SwitchTab(int tabIndex) {
    // 显示/隐藏查询面板的所有控件
    for (int i = 0; i < g_nQueryControlCount; i++) {
        ShowWindow(g_hQueryControls[i], tabIndex == 0 ? SW_SHOW : SW_HIDE);
    }

    // 显示/隐藏订阅面板的所有控件
    for (int i = 0; i < g_nSubscribeControlCount; i++) {
        ShowWindow(g_hSubscribeControls[i], tabIndex == 1 ? SW_SHOW : SW_HIDE);
    }

    // 显示/隐藏系统设置面板容器（及其子控件）
    ShowWindow(g_hSettingsPanel, tabIndex == 2 ? SW_SHOW : SW_HIDE);

    // 行情列表用于查询/订阅两页
    if (g_hListViewQuery) {
        ShowWindow(g_hListViewQuery, (tabIndex == 0 || tabIndex == 1) ? SW_SHOW : SW_HIDE);
    }
    
    // 强制重绘主窗口
    InvalidateRect(g_hMainWnd, NULL, TRUE);
}

static void SyncEnvRadios(BOOL useProd) {
    g_useProdEnv = useProd ? TRUE : FALSE;
    if (g_hRadioEnvTest) SendMessage(g_hRadioEnvTest, BM_SETCHECK, g_useProdEnv ? BST_UNCHECKED : BST_CHECKED, 0);
    if (g_hRadioEnvProd) SendMessage(g_hRadioEnvProd, BM_SETCHECK, g_useProdEnv ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_hRadioEnvTestSub) SendMessage(g_hRadioEnvTestSub, BM_SETCHECK, g_useProdEnv ? BST_UNCHECKED : BST_CHECKED, 0);
    if (g_hRadioEnvProdSub) SendMessage(g_hRadioEnvProdSub, BM_SETCHECK, g_useProdEnv ? BST_CHECKED : BST_UNCHECKED, 0);
}

static void SyncInstrumentTextFrom(HWND srcEdit) {
    if (g_syncingInstrumentText || !srcEdit) return;
    HWND dstEdit = (srcEdit == g_hEditQueryInstrument) ? g_hEditSubscribeInstrument : g_hEditQueryInstrument;
    if (!dstEdit) return;

    int len = GetWindowTextLengthW(srcEdit);
    WCHAR* buf = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
    if (!buf) return;
    GetWindowTextW(srcEdit, buf, len + 1);

    g_syncingInstrumentText = TRUE;
    SetWindowTextW(dstEdit, buf);
    g_syncingInstrumentText = FALSE;
    free(buf);
}

static void SetInstrumentTextBoth(const WCHAR* text) {
    if (!text) return;
    g_syncingInstrumentText = TRUE;
    if (g_hEditQueryInstrument) {
        SetWindowTextW(g_hEditQueryInstrument, text);
    }
    if (g_hEditSubscribeInstrument) {
        SetWindowTextW(g_hEditSubscribeInstrument, text);
    }
    g_syncingInstrumentText = FALSE;
}

static BOOL BlockIfQueryInFlight(void) {
    if (g_queryInFlight) {
        UpdateStatus("上一条查询未返回，请稍后再试");
        return TRUE;
    }
    return FALSE;
}

// 根据查询环境单选按钮获取当前 Trader 实例（测试/正式）
CTPTrader* GetActiveQueryTrader() {
    if (g_useProdEnv) {
        // 正式环境被选中时使用正式 Trader
        return g_pTraderProd;
    }
    return g_pTraderTest;
}

// 获取当前查询环境的显示名称（用于状态提示）
const char* GetActiveQueryEnvName() {
    if (g_useProdEnv) {
        return "正式";
    }
    return "测试";
}

static void GetActiveQueryMdFrontAddr(char* out, int outSize) {
    if (!out || outSize <= 0) return;
    out[0] = 0;
    WCHAR wAddr[128] = {0};
    if (g_useProdEnv) {
        if (g_hEditMdFrontAddrProd) {
            GetWindowText(g_hEditMdFrontAddrProd, wAddr, _countof(wAddr));
        }
    } else {
        if (g_hEditMdFrontAddr) {
            GetWindowText(g_hEditMdFrontAddr, wAddr, _countof(wAddr));
        }
    }
    WideCharToMultiByte(CP_ACP, 0, wAddr, -1, out, outSize, NULL, NULL);
}

// 主窗口过程：处理菜单、按钮、定时器、行情更新等消息
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            // 创建主窗口 UI，并初始化交易对象
            CreateMainWindow(hWnd, g_hInst);
            // 分别创建测试/正式 Trader，并挂接回调
            g_pTraderTest = CreateCTPTrader();
            g_pTraderProd = CreateCTPTrader();
            if (g_pTraderTest) {
                SetMainWindow(g_pTraderTest, g_hMainWnd);
                SetListView(g_pTraderTest, g_hListViewQuery);  // 默认使用查询面板的ListView
                SetStatusCallback(g_pTraderTest, UpdateStatus);
            }
            if (g_pTraderProd) {
                SetMainWindow(g_pTraderProd, g_hMainWnd);
                SetStatusCallback(g_pTraderProd, UpdateStatus);
            }
            return 0;
            
        case WM_NOTIFY: {
            // 处理 Tab 和 ListView 等控件通知
            LPNMHDR pnmhdr = (LPNMHDR)lParam;
            
            // 处理 TAB 切换：切换面板并更新当前 Trader 的 ListView
            // 判断是否为 Tab 控件通知
            if (pnmhdr->idFrom == IDC_TAB_CONTROL && pnmhdr->code == TCN_SELCHANGE) {
                int tabIndex = TabCtrl_GetCurSel(g_hTabControl);
                SwitchTab(tabIndex);
                
                // 根据TAB切换ListView
                if (tabIndex == 0 || tabIndex == 1) {
                    CTPTrader* trader = GetActiveQueryTrader();
                    if (trader) {
                        // 切换 Trader 的输出列表到查询列表
                        SetListView(trader, g_hListViewQuery);
                    }
                }
                return 0;
            }
            
            // 处理 ListView 点击：选中合约后自动填充输入框
            if (pnmhdr->idFrom == IDC_LISTVIEW_QUERY) {
                if (pnmhdr->code == LVN_INSERTITEM) {
                    g_queryInFlight = FALSE;
                    g_queryInFlightType = 0;
                    KillTimer(g_hMainWnd, IDT_QUERY_TIMEOUT);
                    return 0;
                }
                if (pnmhdr->code == LVN_ITEMCHANGED) {
                    // 处理选中状态的变化
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if ((pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED)) {
                        // 从未选中变为选中
                        WCHAR instrumentID[32] = {0};
                        ListView_GetItemText(g_hListViewQuery, pnmv->iItem, 0, instrumentID, 32);
                        
                        // 去除尾部空格/分隔符
                        int len = wcslen(instrumentID);
                        while (len > 0 && IsDelimW(instrumentID[len-1])) {
                            instrumentID[--len] = 0;
                        }
                        
                        if (len > 0) {
                            int curLen = GetWindowTextLength(g_hEditQueryInstrument);
                            if (curLen <= 0) {
                                SetInstrumentTextBoth(instrumentID);
                            } else {
                                WCHAR* curBuf = (WCHAR*)malloc((curLen + 1) * sizeof(WCHAR));
                                if (curBuf) {
                                    GetWindowText(g_hEditQueryInstrument, curBuf, curLen + 1);
                                    int i = 0;
                                    int exists = 0;
                                    while (i < curLen) {
                                        while (i < curLen && IsDelimW(curBuf[i])) i++;
                                        int start = i;
                                        while (i < curLen && !IsDelimW(curBuf[i])) i++;
                                        if (i > start) {
                                            int tokLen = i - start;
                                            if (tokLen == len && _wcsnicmp(curBuf + start, instrumentID, len) == 0) {
                                                exists = 1;
                                                break;
                                            }
                                        }
                                    }
                                    if (!exists) {
                                        int end = curLen;
                                        while (end > 0 && IsDelimW(curBuf[end-1])) end--;
                                        int sepLen = (end > 0) ? 2 : 0; // , 
                                        int newLen = end + sepLen + len;
                                        WCHAR* newBuf = (WCHAR*)malloc((newLen + 1) * sizeof(WCHAR));
                                        if (newBuf) {
                                            if (end > 0) memcpy(newBuf, curBuf, (end) * sizeof(WCHAR));
                                            if (sepLen) {
                                                newBuf[end] = L',';
                                                newBuf[end + 1] = L' ';
                                            }
                                            memcpy(newBuf + end + sepLen, instrumentID, (len) * sizeof(WCHAR));
                                            newBuf[newLen] = 0;
                                            SetInstrumentTextBoth(newBuf);
                                            free(newBuf);
                                        }
                                    }
                                    free(curBuf);
                                }
                            }

                            
                            char ansiInstrumentID[32];
                            // 将合约名转为 ANSI 用于状态提示
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
            // 按钮/菜单命令统一在此处理
            switch (LOWORD(wParam)) {
                case IDC_RADIO_ENV_TEST:
                case IDC_RADIO_ENV_PROD:
                case IDC_RADIO_ENV_TEST_SUB:
                case IDC_RADIO_ENV_PROD_SUB: {
                    // 切换环境时同步更新默认连接参数
                    if (HIWORD(wParam) == BN_CLICKED) {
                        BOOL useProd = (LOWORD(wParam) == IDC_RADIO_ENV_PROD || LOWORD(wParam) == IDC_RADIO_ENV_PROD_SUB);
                        SyncEnvRadios(useProd);
                        if (useProd) {
                            if (g_hEditFrontAddrProd) SetWindowText(g_hEditFrontAddrProd, TEXT("tcp://58.247.171.151:31205"));
                            if (g_hEditMdFrontAddrProd) SetWindowText(g_hEditMdFrontAddrProd, TEXT("tcp://58.247.171.151:31213"));
                            if (g_hEditAuthCodeProd) SetWindowText(g_hEditAuthCodeProd, TEXT("AM8P99PA7UPUXREW"));
                            if (g_hEditBrokerIDProd) SetWindowText(g_hEditBrokerIDProd, TEXT("4040"));
                            if (g_hEditUserIDProd) SetWindowText(g_hEditUserIDProd, TEXT("0022868599"));
                            if (g_hEditPasswordProd) SetWindowText(g_hEditPasswordProd, TEXT("XXXX"));
                            if (g_hEditFrontAddr) SetWindowText(g_hEditFrontAddr, TEXT("tcp://58.247.171.151:31205"));
                            if (g_hEditMdFrontAddr) SetWindowText(g_hEditMdFrontAddr, TEXT("tcp://58.247.171.151:31213"));
                            if (g_hEditAuthCode) SetWindowText(g_hEditAuthCode, TEXT("AM8P99PA7UPUXREW"));
                            if (g_hEditBrokerID) SetWindowText(g_hEditBrokerID, TEXT("4040"));
                            if (g_hEditUserID) SetWindowText(g_hEditUserID, TEXT("0022868599"));
                            if (g_hEditPassword) SetWindowText(g_hEditPassword, TEXT("XXXX"));
                        } else {
                            if (g_hEditFrontAddr) SetWindowText(g_hEditFrontAddr, TEXT("tcp://106.37.101.162:31205"));
                            if (g_hEditMdFrontAddr) SetWindowText(g_hEditMdFrontAddr, TEXT("tcp://106.37.101.162:31213"));
                            if (g_hEditAuthCode) SetWindowText(g_hEditAuthCode, TEXT("YHQHYHQHYHQHYHQH"));
                            if (g_hEditBrokerID) SetWindowText(g_hEditBrokerID, TEXT("1010"));
                            if (g_hEditUserID) SetWindowText(g_hEditUserID, TEXT("20833"));
                            if (g_hEditPassword) SetWindowText(g_hEditPassword, TEXT("826109"));
                        }
                    }
                    break;
                }

                case IDC_EDIT_QUERY_MAX: {
                    if (HIWORD(wParam) == EN_KILLFOCUS) {
                        ApplyQueryMaxRecordsFromEdit(g_hEditQueryMax);
                    }
                    break;
                }
                case IDC_EDIT_QUERY_INSTRUMENT:
                case IDC_EDIT_SUB_INSTRUMENT: {
                    if (HIWORD(wParam) == EN_CHANGE) {
                        HWND srcEdit = (HWND)lParam;
                        SyncInstrumentTextFrom(srcEdit);
                    }
                    break;
                }

                case IDC_BTN_CONNECT: {
                    // 测试环境连接/断开
                    CTPTrader* trader = g_pTraderTest;
                    if (trader && IsLoggedIn(trader)) {
                        Disconnect(trader);
                        // 状态变更时同步更新连接按钮文字
                        // 根据当前登录状态刷新按钮文案
// 根据当前登录状态刷新按钮文案
                        SetConnectButtonText(g_hBtnConnect, FALSE, FALSE);
                        break;
                    }

                    // 简单的连接节流，避免频繁重连
                    ULONGLONG now = GetTickCount64();
                    if (g_lastConnectAttemptTest && (now - g_lastConnectAttemptTest) < CONNECT_RETRY_DELAY_MS) {
                        UpdateStatus("未连接，请10秒后重试");
                        break;
                    }

                    char brokerID[32], userID[32], password[32], frontAddr[128], authCode[64];
                    WCHAR wBrokerID[32], wUserID[32], wPassword[32], wFrontAddr[128], wAuthCode[64];

                    // 从界面读取参数并做宽窄字符转换
                    GetWindowText(g_hEditBrokerID, wBrokerID, 32);
                    GetWindowText(g_hEditUserID, wUserID, 32);
                    GetWindowText(g_hEditPassword, wPassword, 32);
                    GetWindowText(g_hEditFrontAddr, wFrontAddr, 128);
                    GetWindowText(g_hEditAuthCode, wAuthCode, 64);

                    // 将界面输入转换为 ANSI 供 CTP 使用
                    WideCharToMultiByte(CP_ACP, 0, wBrokerID, -1, brokerID, sizeof(brokerID), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wUserID, -1, userID, sizeof(userID), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wPassword, -1, password, sizeof(password), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wFrontAddr, -1, frontAddr, sizeof(frontAddr), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wAuthCode, -1, authCode, sizeof(authCode), NULL, NULL);

                    // 发起连接并启动登录轮询
                    UpdateStatus("正在连接(测试)...");
                    if (trader) {
                        Disconnect(trader);
                        ConnectAndLogin(trader, brokerID, userID, password, frontAddr, authCode, APP_ID);
                        g_lastConnectAttemptTest = now;
                        StartLoginPollTimer(FALSE);
                    } else {
                        UpdateStatus("错误: 测试环境对象未初始化");
                    }
                    break;
                }
                
                case IDC_BTN_CONNECT_PROD: {
                    // 正式环境连接/断开
                    CTPTrader* trader = g_pTraderProd;
                    if (trader && IsLoggedIn(trader)) {
                        Disconnect(trader);
                        SetConnectButtonText(g_hBtnConnectProd, FALSE, TRUE);
                        break;
                    }

                    // 简单的连接节流，避免频繁重连
                    ULONGLONG now = GetTickCount64();
                    if (g_lastConnectAttemptProd && (now - g_lastConnectAttemptProd) < CONNECT_RETRY_DELAY_MS) {
                        UpdateStatus("未连接，请10秒后重试");
                        break;
                    }

                    char brokerID[32], userID[32], password[32], frontAddr[128], authCode[64];
                    WCHAR wBrokerID[32], wUserID[32], wPassword[32], wFrontAddr[128], wAuthCode[64];

                    // 从界面读取参数并做宽窄字符转换
                    GetWindowText(g_hEditBrokerIDProd, wBrokerID, 32);
                    GetWindowText(g_hEditUserIDProd, wUserID, 32);
                    GetWindowText(g_hEditPasswordProd, wPassword, 32);
                    GetWindowText(g_hEditFrontAddrProd, wFrontAddr, 128);
                    GetWindowText(g_hEditAuthCodeProd, wAuthCode, 64);

                    WideCharToMultiByte(CP_ACP, 0, wBrokerID, -1, brokerID, sizeof(brokerID), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wUserID, -1, userID, sizeof(userID), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wPassword, -1, password, sizeof(password), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wFrontAddr, -1, frontAddr, sizeof(frontAddr), NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, wAuthCode, -1, authCode, sizeof(authCode), NULL, NULL);

                    // 发起连接并启动登录轮询
                    UpdateStatus("正在连接(正式)...");
                    if (trader) {
                        Disconnect(trader);
                        ConnectAndLogin(trader, brokerID, userID, password, frontAddr, authCode, APP_ID);
                        g_lastConnectAttemptProd = now;
                        StartLoginPollTimer(TRUE);
                    } else {
                        UpdateStatus("错误: 正式环境对象未初始化");
                    }
                    break;
                }

                case IDC_BTN_QUERY_ORDER: {
                    // 查询委托：要求已登录
                    if (BlockIfQueryInFlight()) break;
                    CTPTrader* trader = GetActiveQueryTrader();
                    if (trader && IsLoggedIn(trader)) {
                        SetListView(trader, g_hListViewQuery);
                        UpdateStatus("正在查询委托...");
                        g_queryInFlight = TRUE;
                        g_queryInFlightType = 1;
                        SetTimer(g_hMainWnd, IDT_QUERY_TIMEOUT, 8000, NULL);
                        QueryOrders(trader);
                    } else {
                        char msg[128];
                        sprintf(msg, "错误: 请先连接登录(%s)", GetActiveQueryEnvName());
                        UpdateStatus(msg);
                    }
                    break;
                }
                case IDC_BTN_QUERY_POS: {
                    // 查询持仓：要求已登录
                    if (BlockIfQueryInFlight()) break;
                    CTPTrader* trader = GetActiveQueryTrader();
                    if (trader && IsLoggedIn(trader)) {
                        SetListView(trader, g_hListViewQuery);
                        UpdateStatus("正在查询持仓...");
                        g_queryInFlight = TRUE;
                        g_queryInFlightType = 2;
                        SetTimer(g_hMainWnd, IDT_QUERY_TIMEOUT, 8000, NULL);
                        QueryPositions(trader);
                    } else {
                        char msg[128];
                        sprintf(msg, "错误: 请先连接登录(%s)", GetActiveQueryEnvName());
                        UpdateStatus(msg);
                    }
                    break;
                }
                case IDC_BTN_QUERY_MARKET: {
                    // 查询行情：按合约代码查询（不走订阅）
                    if (BlockIfQueryInFlight()) break;
                    CTPTrader* trader = GetActiveQueryTrader();
                    if (trader && IsLoggedIn(trader)) {
                        SetListView(trader, g_hListViewQuery);
                        WCHAR* instrumentBuf = NULL;
                        int textLen = GetWindowTextLength(g_hEditQueryInstrument);
                        if (textLen > 0) {
                            instrumentBuf = (WCHAR*)malloc((textLen + 1) * sizeof(WCHAR));
                            if (!instrumentBuf) {
                                UpdateStatus("错误: 内存不足");
                                break;
                            }
                            GetWindowText(g_hEditQueryInstrument, instrumentBuf, textLen + 1);
                            TrimInPlaceW(instrumentBuf);
                        }

                        g_marketSubscribeMode = FALSE;
                        char* ansiInstruments = NULL;
                        const char* queryList = "";
                        if (instrumentBuf && instrumentBuf[0]) {
                            int ansiLen = WideCharToMultiByte(CP_ACP, 0, instrumentBuf, -1, NULL, 0, NULL, NULL);
                            if (ansiLen <= 0) {
                                UpdateStatus("错误: 字符转换失败");
                                free(instrumentBuf);
                                break;
                            }
                            ansiInstruments = (char*)malloc(ansiLen);
                            if (!ansiInstruments) {
                                UpdateStatus("错误: 内存不足");
                                free(instrumentBuf);
                                break;
                            }
                            WideCharToMultiByte(CP_ACP, 0, instrumentBuf, -1, ansiInstruments, ansiLen, NULL, NULL);
                            queryList = ansiInstruments;
                        }

                        if (!queryList[0]) {
                            UpdateStatus("正在查询全部行情...");
                        } else {
                            UpdateStatus("正在查询行情...");
                        }
                        int result = QueryMarketDataBatch(trader, queryList);
                        if (result == 0) {
                            g_queryInFlight = TRUE;
                            g_queryInFlightType = 3;
                            UpdateStatus("行情查询请求已发送，请等待响应...");
                            SetTimer(g_hMainWnd, 1001, 5000, NULL);
                        } else {
                            UpdateStatus("错误: 行情查询请求发送失败");
                        }
                        if (ansiInstruments) free(ansiInstruments);
                        if (instrumentBuf) free(instrumentBuf);
                    } else {
                        char msg[128];
                        sprintf(msg, "错误: 请先连接登录(%s)", GetActiveQueryEnvName());
                        UpdateStatus(msg);
                    }
                    break;
                }
                case IDC_BTN_SUB_MD: {
                    // 订阅行情：使用行情接口
                    CTPTrader* trader = GetActiveQueryTrader();
                    if (trader && IsLoggedIn(trader)) {
                        SetListView(trader, g_hListViewQuery);
                        HWND hEdit = g_hEditSubscribeInstrument ? g_hEditSubscribeInstrument : g_hEditQueryInstrument;
                        int textLen = hEdit ? GetWindowTextLength(hEdit) : 0;
                        if (textLen <= 0) {
                            UpdateStatus("错误: 请输入合约代码");
                            break;
                        }
                        WCHAR* instrumentBuf = (WCHAR*)malloc((textLen + 1) * sizeof(WCHAR));
                        if (!instrumentBuf) {
                            UpdateStatus("错误: 内存不足");
                            break;
                        }
                        GetWindowText(hEdit, instrumentBuf, textLen + 1);
                        TrimInPlaceW(instrumentBuf);
                        if (instrumentBuf[0] == 0) {
                            UpdateStatus("错误: 请输入合约代码");
                            free(instrumentBuf);
                            break;
                        }

                        int ansiLen = WideCharToMultiByte(CP_ACP, 0, instrumentBuf, -1, NULL, 0, NULL, NULL);
                        if (ansiLen <= 0) {
                            UpdateStatus("错误: 字符转换失败");
                            free(instrumentBuf);
                            break;
                        }
                        char* ansiInstruments = (char*)malloc(ansiLen);
                        if (!ansiInstruments) {
                            UpdateStatus("错误: 内存不足");
                            free(instrumentBuf);
                            break;
                        }
                        WideCharToMultiByte(CP_ACP, 0, instrumentBuf, -1, ansiInstruments, ansiLen, NULL, NULL);

                        char mdFrontAddr[128] = {0};
                        GetActiveQueryMdFrontAddr(mdFrontAddr, sizeof(mdFrontAddr));
                        if (!mdFrontAddr[0]) {
                            UpdateStatus("错误: 行情前置地址为空");
                            free(ansiInstruments);
                            free(instrumentBuf);
                            break;
                        }
                        int connectResult = ConnectMarket(trader, mdFrontAddr);
                        if (connectResult < 0) {
                            free(ansiInstruments);
                            free(instrumentBuf);
                            break;
                        }

                        // 清空列表与列，确保展示为行情列
                        if (g_hListViewQuery && IsWindow(g_hListViewQuery)) {
                            ListView_DeleteAllItems(g_hListViewQuery);
                            while (ListView_DeleteColumn(g_hListViewQuery, 0));
                        }

                        UnsubscribeMarketData(trader, ansiInstruments);

                        UpdateStatus("正在订阅行情...");
                        g_marketSubscribeMode = TRUE;
                        int result = SubscribeMarketData(trader, ansiInstruments);
                        if (result == 0 || result == -2) {
                            UpdateStatus("行情订阅请求已发送，请等待响应...");
                            SetTimer(g_hMainWnd, 1001, 5000, NULL);
                        } else {
                            UpdateStatus("错误: 行情订阅请求发送失败");
                        }
                        free(ansiInstruments);
                        free(instrumentBuf);
                    } else {
                        char msg[128];
                        sprintf(msg, "错误: 请先连接登录(%s)", GetActiveQueryEnvName());
                        UpdateStatus(msg);
                    }
                    break;
                }
                case IDC_BTN_UNSUB_MD: {
                    // 取消订阅行情
                    CTPTrader* trader = GetActiveQueryTrader();
                    if (trader && IsLoggedIn(trader)) {
                        HWND hEdit = g_hEditSubscribeInstrument ? g_hEditSubscribeInstrument : g_hEditQueryInstrument;
                        int textLen = hEdit ? GetWindowTextLength(hEdit) : 0;
                        if (textLen <= 0) {
                            UpdateStatus("错误: 请输入合约代码");
                            break;
                        }
                        WCHAR* instrumentBuf = (WCHAR*)malloc((textLen + 1) * sizeof(WCHAR));
                        if (!instrumentBuf) {
                            UpdateStatus("错误: 内存不足");
                            break;
                        }
                        GetWindowText(hEdit, instrumentBuf, textLen + 1);
                        TrimInPlaceW(instrumentBuf);
                        if (instrumentBuf[0] == 0) {
                            UpdateStatus("错误: 请输入合约代码");
                            free(instrumentBuf);
                            break;
                        }

                        int ansiLen = WideCharToMultiByte(CP_ACP, 0, instrumentBuf, -1, NULL, 0, NULL, NULL);
                        if (ansiLen <= 0) {
                            UpdateStatus("错误: 字符转换失败");
                            free(instrumentBuf);
                            break;
                        }
                        char* ansiInstruments = (char*)malloc(ansiLen);
                        if (!ansiInstruments) {
                            UpdateStatus("错误: 内存不足");
                            free(instrumentBuf);
                            break;
                        }
                        WideCharToMultiByte(CP_ACP, 0, instrumentBuf, -1, ansiInstruments, ansiLen, NULL, NULL);

                        UnsubscribeMarketData(trader, ansiInstruments);
                        UpdateStatus("已发送取消订阅请求");
                        free(ansiInstruments);
                        free(instrumentBuf);
                    } else {
                        char msg[128];
                        sprintf(msg, "错误: 请先连接登录(%s)", GetActiveQueryEnvName());
                        UpdateStatus(msg);
                    }
                    break;
                }
                case IDC_BTN_IMPORT_CODES: {
                    // 从 Excel 导入合约代码
                    ImportInstrumentsFromExcel(g_hMainWnd);
                    break;
                }
                case IDC_BTN_EXPORT_LATEST: {
                    // 将最新价回写到 Excel
                    ExportLatestToExcel(g_hMainWnd);
                    break;
                }

                case IDC_BTN_QUERY_OPTION: {
                    // 查询期权合约：要求已登录
                    if (BlockIfQueryInFlight()) break;
                    CTPTrader* trader = GetActiveQueryTrader();
                    if (trader && IsLoggedIn(trader)) {
                        SetListView(trader, g_hListViewQuery);
                        UpdateStatus("正在查询期权合约...");
                        g_queryInFlight = TRUE;
                        g_queryInFlightType = 4;
                        SetTimer(g_hMainWnd, IDT_QUERY_TIMEOUT, 8000, NULL);
                        QueryOptions(trader);
                    } else {
                        char msg[128];
                        sprintf(msg, "错误: 请先连接登录(%s)", GetActiveQueryEnvName());
                        UpdateStatus(msg);
                    }
                    break;
                }
                case IDC_BTN_QUERY_INST: {
                    // 查询合约列表：要求已登录
                    if (BlockIfQueryInFlight()) break;
                    CTPTrader* trader = GetActiveQueryTrader();
                    if (trader && IsLoggedIn(trader)) {
                        SetListView(trader, g_hListViewQuery);
                        UpdateStatus("正在查询合约...");
                        g_queryInFlight = TRUE;
                        g_queryInFlightType = 4;
                        SetTimer(g_hMainWnd, IDT_QUERY_TIMEOUT, 8000, NULL);
                        QueryInstrument(trader, "");
                    } else {
                        char msg[128];
                        sprintf(msg, "错误: 请先连接登录(%s)", GetActiveQueryEnvName());
                        UpdateStatus(msg);
                    }
                    break;
                }

            }

            return 0;
        case WM_APP_MD_UPDATE: {
            // 行情更新消息：刷新/新增 ListView 行
            MdUpdate* u = (MdUpdate*)lParam;
            if (!u) return 0;
            g_queryInFlight = FALSE;
            // ListView 不可用则丢弃行情
            if (!g_hListViewQuery || !IsWindow(g_hListViewQuery)) {
                free(u);
                return 0;
            }

            // 如果列未初始化，则重建列头
            HWND hdr = ListView_GetHeader(g_hListViewQuery);
            int colCount = hdr ? Header_GetItemCount(hdr) : 0;
            if (colCount < 9) {
                ListView_DeleteAllItems(g_hListViewQuery);
                while (ListView_DeleteColumn(g_hListViewQuery, 0));

                LVCOLUMNW c = {0};
                c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
                c.fmt = LVCFMT_LEFT;

                c.cx = 90;  c.pszText = L"合约";      ListView_InsertColumn(g_hListViewQuery, 0, &c);
                c.cx = 80;  c.pszText = L"最新价";    ListView_InsertColumn(g_hListViewQuery, 1, &c);
                c.cx = 80;  c.pszText = L"成交量";    ListView_InsertColumn(g_hListViewQuery, 2, &c);
                c.cx = 80;  c.pszText = L"买一价";    ListView_InsertColumn(g_hListViewQuery, 3, &c);
                c.cx = 80;  c.pszText = L"买一量";    ListView_InsertColumn(g_hListViewQuery, 4, &c);
                c.cx = 80;  c.pszText = L"卖一价";    ListView_InsertColumn(g_hListViewQuery, 5, &c);
                c.cx = 80;  c.pszText = L"卖一量";    ListView_InsertColumn(g_hListViewQuery, 6, &c);
                c.cx = 90;  c.pszText = L"更新时间";  ListView_InsertColumn(g_hListViewQuery, 7, &c);
                c.cx = 60;  c.pszText = L"毫秒";      ListView_InsertColumn(g_hListViewQuery, 8, &c);
            }

            WCHAR wInst[32] = {0};
            MultiByteToWideChar(CP_ACP, 0, u->instrumentID, -1, wInst, 32);

            // 查找合约所在行，不存在则新增一行
            int row = -1;
            int itemCount = ListView_GetItemCount(g_hListViewQuery);
            for (int i = 0; i < itemCount; i++) {
                WCHAR buf[64] = {0};
                ListView_GetItemText(g_hListViewQuery, i, 0, buf, 64);
                if (wcscmp(buf, wInst) == 0) {
                    row = i;
                    break;
                }
            }
            if (row == -1) {
                LVITEMW it = {0};
                it.mask = LVIF_TEXT;
                it.iItem = itemCount;
                it.iSubItem = 0;
                it.pszText = wInst;
                row = (int)SendMessage(g_hListViewQuery, LVM_INSERTITEMW, 0, (LPARAM)&it);
            }

            // 按列填充最新价/成交量/买卖盘/时间等数据
            WCHAR w[64];
            swprintf_s(w, _countof(w), L"%.2f", u->lastPrice);
            ListView_SetItemText(g_hListViewQuery, row, 1, w);

            wsprintfW(w, L"%I64d", (LONGLONG)u->volume);
            ListView_SetItemText(g_hListViewQuery, row, 2, w);

            swprintf_s(w, _countof(w), L"%.2f", u->bidPrice1);
            ListView_SetItemText(g_hListViewQuery, row, 3, w);

            wsprintfW(w, L"%I64d", (LONGLONG)u->bidVolume1);
            ListView_SetItemText(g_hListViewQuery, row, 4, w);

            swprintf_s(w, _countof(w), L"%.2f", u->askPrice1);
            ListView_SetItemText(g_hListViewQuery, row, 5, w);

            wsprintfW(w, L"%I64d", (LONGLONG)u->askVolume1);
            ListView_SetItemText(g_hListViewQuery, row, 6, w);

            WCHAR wTime[16] = {0};
            MultiByteToWideChar(CP_ACP, 0, u->updateTime, -1, wTime, 16);
            ListView_SetItemText(g_hListViewQuery, row, 7, wTime);

            wsprintfW(w, L"%d", u->updateMillisec);
            ListView_SetItemText(g_hListViewQuery, row, 8, w);

            free(u);
            return 0;
        }

        case WM_TIMER:
            // 定时器：登录轮询 + 行情超时提示
            if (wParam == IDT_LOGIN_POLL_TEST) {
                if (g_pTraderTest && IsLoggedIn(g_pTraderTest)) {
                    SetConnectButtonText(g_hBtnConnect, TRUE, FALSE);
                    KillTimer(g_hMainWnd, IDT_LOGIN_POLL_TEST);
                } else if (g_loginPollStartTest && (GetTickCount64() - g_loginPollStartTest) > LOGIN_POLL_TIMEOUT_MS) {
                    KillTimer(g_hMainWnd, IDT_LOGIN_POLL_TEST);
                }
                return 0;
            }
            if (wParam == IDT_LOGIN_POLL_PROD) {
                if (g_pTraderProd && IsLoggedIn(g_pTraderProd)) {
                    SetConnectButtonText(g_hBtnConnectProd, TRUE, TRUE);
                    KillTimer(g_hMainWnd, IDT_LOGIN_POLL_PROD);
                } else if (g_loginPollStartProd && (GetTickCount64() - g_loginPollStartProd) > LOGIN_POLL_TIMEOUT_MS) {
                    KillTimer(g_hMainWnd, IDT_LOGIN_POLL_PROD);
                }
                return 0;
            }
            if (wParam == IDT_QUERY_TIMEOUT) {
                if (g_queryInFlight) {
                    UpdateStatus("上一条查询未返回，请稍后再试");
                    g_queryInFlight = FALSE;
                    g_queryInFlightType = 0;
                }
                KillTimer(g_hMainWnd, IDT_QUERY_TIMEOUT);
                return 0;
            }
            
            // 行情查询后检查是否收到数据
            if (wParam == 1001) {
                if (!g_hListViewQuery || !IsWindow(g_hListViewQuery)) {
                    MessageBox(g_hMainWnd, TEXT("ListView 未初始化或已销毁！"), TEXT("错误"), MB_ICONERROR | MB_OK);
                    CTPTrader* trader = GetActiveQueryTrader();
                if (trader) {
                    CancelMarketQuery(trader);
                }
                KillTimer(g_hMainWnd, 1001);
                    return 0;
                }

                // 检查ListView是否有数据
                int itemCount = ListView_GetItemCount(g_hListViewQuery);
                if (itemCount == 0) {
                    if (g_marketSubscribeMode) {
                        MessageBox(g_hMainWnd,
                                 TEXT("未收到行情订阅数据！\n\n可能的原因：\n1. 行情前置未连接成功\n2. 合约代码输入错误\n3. 当前非交易时间\n4. 账户无行情权限\n\n建议：\n- 检查行情前置地址\n- 检查合约代码是否正确\n- 确认当前是交易时间\n- 或者联系管理员开通行情权限"),
                                 TEXT("行情订阅提示"),
                                 MB_ICONWARNING | MB_OK);
                    } else {
                        MessageBox(g_hMainWnd,
                                 TEXT("未收到行情数据！\n\n可能的原因：\n1. 该CTP柜台不支持通过交易接口查询行情\n2. 合约代码输入错误\n3. 当前非交易时间\n4. 需要使用行情API进行实时订阅\n\n建议：\n- 检查合约代码是否正确\n- 确认当前是交易时间\n- 或者联系管理员开通行情权限"),
                                 TEXT("行情查询提示"),
                                 MB_ICONWARNING | MB_OK);
                    }
                }
                CTPTrader* trader = GetActiveQueryTrader();
                if (trader && !g_marketSubscribeMode) {
                    CancelMarketQuery(trader);
                }
                g_queryInFlight = FALSE;
                g_queryInFlightType = 0;
                KillTimer(g_hMainWnd, IDT_QUERY_TIMEOUT);
                KillTimer(g_hMainWnd, 1001);
            }
            return 0;
        case WM_DESTROY:
            // 退出前清理 Trader 对象
            if (g_pTraderTest) {
                // 释放测试环境 Trader 资源
                DestroyCTPTrader(g_pTraderTest);
                g_pTraderTest = NULL;
            }
            if (g_pTraderProd) {
                DestroyCTPTrader(g_pTraderProd);
                g_pTraderProd = NULL;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// 判断字符串是否为 UTF-8（用于状态栏编码处理）
static BOOL IsUtf8String(const char* s) {
    if (!s) return FALSE;
    // 尝试按 UTF-8 解码，失败则认为不是 UTF-8
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    return wlen > 0;
}

// 更新状态栏文本并同步连接按钮状态
// 将状态字符串转换为宽字符并更新状态栏，同时刷新连接按钮文案
void UpdateStatus(const char* msg) {
    if (g_hStatus && msg) {
        WCHAR wMsg[512];
        // 支持 UTF-8 或 ACP 编码的消息
        if (IsUtf8String(msg)) {
            // 按 UTF-8 转换消息
            MultiByteToWideChar(CP_UTF8, 0, msg, -1, wMsg, 512);
        } else {
            // 按系统默认编码转换消息
            MultiByteToWideChar(CP_ACP, 0, msg, -1, wMsg, 512);
        }
        
        // 写入状态栏控件文本
        SetWindowText(g_hStatus, wMsg);
        if (g_hBtnConnect) {
            SetConnectButtonText(g_hBtnConnect, (g_pTraderTest && IsLoggedIn(g_pTraderTest)) ? TRUE : FALSE, FALSE);
        }
        if (g_hBtnConnectProd) {
            SetConnectButtonText(g_hBtnConnectProd, (g_pTraderProd && IsLoggedIn(g_pTraderProd)) ? TRUE : FALSE, TRUE);
        }
    }
}
