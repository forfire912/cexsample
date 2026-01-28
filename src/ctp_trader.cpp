// CTP Trading API Wrapper
#pragma execution_character_set("utf-8")
#include "ctp_trader.h"
#include "../api/allapi/ThostFtdcTraderApi.h"
#include "../api/allapi/ThostFtdcMdApi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <commctrl.h>
#include <time.h>
#include <windows.h> // 确保包含 windows.h
#include <direct.h>
#include <fstream>
#include <string>
#include <cmath>
#include <cfloat>
#include <vector>

// Helper function: 将 GBK 编码转换为 UTF-8
// CTP API 返回的 ErrorMsg 是 GBK 编码，需要转换为 UTF-8
char* GbkToUtf8(const char* gbkStr) {
    if (!gbkStr) return NULL;
    
    // GBK 转 UNICODE
    int wlen = MultiByteToWideChar(CP_ACP, 0, gbkStr, -1, NULL, 0);
    if (wlen == 0) {
        // 转换失败，返回原字符串副本
        int len = strlen(gbkStr) + 1;
        char* result = new char[len];
        strcpy(result, gbkStr);
        return result;
    }
    
    WCHAR* wStr = new WCHAR[wlen];
    MultiByteToWideChar(CP_ACP, 0, gbkStr, -1, wStr, wlen);
    
    // UNICODE 转 UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wStr, -1, NULL, 0, NULL, NULL);
    char* utf8Str = new char[utf8Len];
    WideCharToMultiByte(CP_UTF8, 0, wStr, -1, utf8Str, utf8Len, NULL, NULL);
    
    delete[] wStr;
    return utf8Str;
}

// 兼容旧代码，重定向到 GbkToUtf8
char* Utf8ToGbk(const char* str) {
    return GbkToUtf8(str);
}

static void FormatChar(char* buf, char v) {
    if (!buf) return;
    if (v == 0) {
        buf[0] = '\0';
        return;
    }
    buf[0] = v;
    buf[1] = '\0';
}

static void FormatInt64(char* buf, long long v) {
    if (!buf) return;
    sprintf(buf, "%lld", v);
}

static void FormatDouble(char* buf, double v, int precision) {
    if (!buf) return;
    if (!std::isfinite(v) || v >= DBL_MAX / 10) {
        strcpy(buf, "--");
        return;
    }
    char fmt[8];
    sprintf(fmt, "%%.%df", precision);
    sprintf(buf, fmt, v);
}

static int SafeStrnlen(const char* s, int maxLen) {
    if (!s || maxLen <= 0) return 0;
    for (int i = 0; i < maxLen; ++i) {
        if (s[i] == '\0') return i;
    }
    return maxLen;
}

static void CopyFixedField(char* dst, size_t dstSize, const char* src, size_t srcSize) {
    if (!dst || dstSize == 0) return;
    dst[0] = '\0';
    if (!src || srcSize == 0) return;
    size_t n = 0;
    while (n < srcSize && src[n] != '\0') n++;
    if (n >= dstSize) n = dstSize - 1;
    if (n) memcpy(dst, src, n);
    dst[n] = '\0';
}


static std::string CsvEscape(const std::string& s) {
    bool needQuotes = false;
    std::string out;
    out.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"' || c == ',' || c == '\n' || c == '\r') {
            needQuotes = true;
        }
        if (c == '"') {
            out.push_back('"');
            out.push_back('"');
        } else {
            out.push_back(c);
        }
    }
    if (needQuotes) {
        return std::string("\"") + out + "\"";
    }
    return out;
}

static void GetTodayYYYYMMDD(char* buf, size_t len) {
    if (!buf || len < 9) return;
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    sprintf(buf, "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

static void GetNowYYYYMMDDHHMMSS(char* buf, size_t len) {
    if (!buf || len < 15) return;
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    sprintf(buf, "%04d%02d%02d%02d%02d%02d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
}

static void EnsureDir(const char* path) {
    if (!path || !path[0]) return;
    _mkdir(path);
}

// 校验日期字符串是否是8位数字 (YYYYMMDD)
static bool IsValidDate8(const char* s) {
    if (!s) return false;
    for (int i = 0; i < 8; ++i) {
        if (s[i] == '\0') return false;
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return s[8] == '\0' || s[8] == '\r' || s[8] == '\n';
}

static std::wstring Utf8ToWide(const char* s) {
    if (!s) return std::wstring();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (wlen <= 0) return std::wstring();
    std::wstring ws;
    ws.resize(wlen - 1);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &ws[0], wlen);
    return ws;
}

static void ExportCsv(const char* userID, const char* contentName, const char* dateStr,
                      const std::vector<std::string>& headers,
                      const std::vector< std::vector<std::string> >& rows) {
    if (!userID || !contentName || !dateStr) return;
    if (headers.empty()) return;
    EnsureDir("export");
    char nameUtf8[260];
    sprintf(nameUtf8, "export\\%s_%s_%s.csv", userID, contentName, dateStr);
    std::wstring wpath = Utf8ToWide(nameUtf8);
    FILE* f = _wfopen(wpath.c_str(), L"wb");
    if (!f) return;
    // UTF-8 BOM for Excel compatibility
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, 1, 3, f);
    // header
    std::string line;
    for (size_t i = 0; i < headers.size(); ++i) {
        if (i) line.append(",");
        line.append(CsvEscape(headers[i]));
    }
    line.append("\n");
    fwrite(line.data(), 1, line.size(), f);
    // rows
    for (size_t r = 0; r < rows.size(); ++r) {
        const std::vector<std::string>& row = rows[r];
        line.clear();
        for (size_t c = 0; c < row.size(); ++c) {
            if (c) line.append(",");
            line.append(CsvEscape(row[c]));
        }
        line.append("\n");
        fwrite(line.data(), 1, line.size(), f);
    }
    fclose(f);
}

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

class TraderSpi : public CThostFtdcTraderSpi {
public:
    CThostFtdcTraderApi* pUserApi;
    HWND hListView;
    StatusCallback statusCallback;
    char brokerID[11];
    char userID[16];
    char password[41];
    char authCode[17];
    char appID[33];
    int requestID;
    bool isConnected;
    bool isAuthenticated;
    bool isLoggedIn;
    bool isQueryingOrders;
    bool isQueryingPositions;
    bool isQueryingMarket;
    bool isQueryingInstrument;
    bool isOptionQuery;
    CRITICAL_SECTION queryLock;
    CRITICAL_SECTION listViewLock;
    
    std::vector<std::string> orderHeaders;
    std::vector< std::vector<std::string> > orderRows;
    std::string orderTradingDay;
    
    std::vector<std::string> positionHeaders;
    std::vector< std::vector<std::string> > positionRows;
    std::string positionTradingDay;
    
    std::vector<std::string> marketHeaders;
    std::vector< std::vector<std::string> > marketRows;
    std::string marketTradingDay;
    
    std::vector<std::string> instrumentHeaders;
    std::vector< std::vector<std::string> > instrumentRows;
    
    std::vector<std::string> marketQueryQueue;
    size_t marketQueryIndex;
    bool marketBatchActive;
    int marketBatchStartRequestID;
    
    TraderSpi() {
        pUserApi = NULL;
        hListView = NULL;
        statusCallback = NULL;
        requestID = 0;
        isConnected = false;
        isAuthenticated = false;
        isLoggedIn = false;
        isQueryingOrders = false;
        isQueryingPositions = false;
        isQueryingMarket = false;
        isQueryingInstrument = false;
        isOptionQuery = false;
        marketQueryIndex = 0;
        marketBatchActive = false;
        marketBatchStartRequestID = 0;
        memset(brokerID, 0, sizeof(brokerID));
        memset(userID, 0, sizeof(userID));
        memset(password, 0, sizeof(password));
        memset(authCode, 0, sizeof(authCode));
        memset(appID, 0, sizeof(appID));
        InitializeCriticalSection(&queryLock);
        InitializeCriticalSection(&listViewLock);
    }
    
    virtual ~TraderSpi() {
        // 析构函数
        DeleteCriticalSection(&queryLock);
        DeleteCriticalSection(&listViewLock);
    }
    
    void UpdateStatus(const char* msg) {
        if (statusCallback) {
            // 直接传递 UTF-8 编码的消息，由 main.c 的 UpdateStatus 统一处理
            statusCallback(msg);
        }
    }

    bool BeginQuery(int queryType) {
        bool ok = false;
        EnterCriticalSection(&queryLock);
        bool any = isQueryingOrders || isQueryingPositions || isQueryingMarket || isQueryingInstrument;
        if (!any) {
            switch (queryType) {
                case 1: ok = true; isQueryingOrders = true; break;
                case 2: ok = true; isQueryingPositions = true; break;
                case 3: ok = true; isQueryingMarket = true; break;
                case 4: ok = true; isQueryingInstrument = true; break;
                default: break;
            }
        }
        LeaveCriticalSection(&queryLock);
        return ok;
    }
    
    void EndQuery(int queryType) {
        EnterCriticalSection(&queryLock);
        switch (queryType) {
            case 1: isQueryingOrders = false; break;
            case 2: isQueryingPositions = false; break;
            case 3: isQueryingMarket = false; break;
            case 4: isQueryingInstrument = false; isOptionQuery = false; break;
            default: break;
        }
        LeaveCriticalSection(&queryLock);
    }

    void ClearMarketQueryQueue() {
        marketQueryQueue.clear();
        marketQueryIndex = 0;
        marketBatchActive = false;
        marketBatchStartRequestID = 0;
    }

    bool HasPendingMarketQuery() const {
        return marketBatchActive && marketQueryIndex < marketQueryQueue.size();
    }

    int SendNextMarketQuery() {
        if (!pUserApi) return -1;
        if (marketQueryIndex >= marketQueryQueue.size()) return 0;
        size_t total = marketQueryQueue.size();
        size_t currentIndex = marketQueryIndex;
        const std::string& inst = marketQueryQueue[marketQueryIndex++];
        CThostFtdcQryDepthMarketDataField req = {0};
        if (!inst.empty()) {
            strncpy(req.InstrumentID, inst.c_str(), sizeof(req.InstrumentID) - 1);
            req.InstrumentID[sizeof(req.InstrumentID) - 1] = '\0';
        }
        int nextRequestID = requestID + 1;
        if (marketBatchStartRequestID == 0) marketBatchStartRequestID = nextRequestID;
        {
            char msg[256];
            size_t sent = currentIndex + 1;
            size_t remaining = total - sent;
            sprintf(msg, "行情查询请求发送失败，跳过合约=%s", inst.c_str());
            UpdateStatus(msg);
        }
        int ret = pUserApi->ReqQryDepthMarketData(&req, ++requestID);
        if (ret != 0) {
            for (int i = 0; i < 3 && ret != 0; i++) {
                Sleep(200);
                ret = pUserApi->ReqQryDepthMarketData(&req, ++requestID);
            }
        }
        if (ret != 0) {
            char msg[256];
            sprintf(msg, "???????????????=%s", inst.c_str());
            UpdateStatus(msg);
            if (HasPendingMarketQuery()) {
                return SendNextMarketQuery();
            }
            ClearMarketQueryQueue();
            EndQuery(3);
            return 0;
        }
        return 0;
    }

    int StartMarketQueryBatch(const std::vector<std::string>& instruments) {
        if (instruments.empty()) return -1;
        if (!BeginQuery(3)) {
            UpdateStatus("琛屾儏鏌ヨ杩涜涓紝璇风◢鍚庨噸璇?");
            return -3;
        }
        marketQueryQueue = instruments;
        marketQueryIndex = 0;
        marketBatchActive = true;
        marketBatchStartRequestID = 0;
        int ret = SendNextMarketQuery();
        if (ret != 0) {
            ClearMarketQueryQueue();
            EndQuery(3);
        }
        return ret;
    }
    
    void ResetOrderExport(const std::vector<std::string>& headers) {
        orderHeaders = headers;
        orderRows.clear();
        orderTradingDay.clear();
    }
    
    void ResetPositionExport(const std::vector<std::string>& headers) {
        positionHeaders = headers;
        positionRows.clear();
        positionTradingDay.clear();
    }
    
    void ResetMarketExport(const std::vector<std::string>& headers) {
        marketHeaders = headers;
        marketRows.clear();
        marketTradingDay.clear();
    }
    
    void ResetInstrumentExport(const std::vector<std::string>& headers) {
        instrumentHeaders = headers;
        instrumentRows.clear();
    }
    
    void ExportOrdersIfReady() {
        if (orderHeaders.empty()) return;
        char datetimeStr[15];
        GetNowYYYYMMDDHHMMSS(datetimeStr, sizeof(datetimeStr));
        ExportCsv(userID, "委托", datetimeStr, orderHeaders, orderRows);
    }
    
    void ExportPositionsIfReady() {
        if (positionHeaders.empty()) return;
        char datetimeStr[15];
        GetNowYYYYMMDDHHMMSS(datetimeStr, sizeof(datetimeStr));
        ExportCsv(userID, "持仓", datetimeStr, positionHeaders, positionRows);
    }
    
    void ExportMarketIfReady() {
        if (marketHeaders.empty()) return;
        // 使用当前时间戳到秒，避免同日多次导出文件覆盖
        char datetimeStr[15];
        GetNowYYYYMMDDHHMMSS(datetimeStr, sizeof(datetimeStr));
        ExportCsv(userID, "行情", datetimeStr, marketHeaders, marketRows);
    }
    
    void ExportInstrumentIfReady(bool isOption) {
        if (instrumentHeaders.empty()) return;
        char datetimeStr[15];
        GetNowYYYYMMDDHHMMSS(datetimeStr, sizeof(datetimeStr));
        const char* contentName = isOption ? "期权" : "合约";
        ExportCsv(userID, contentName, datetimeStr, instrumentHeaders, instrumentRows);
    }
    
    void ClearListView() {
        EnterCriticalSection(&listViewLock);
        HWND target = hListView;
        if (target && IsWindow(target)) {
            ListView_DeleteAllItems(target);
            while (ListView_DeleteColumn(target, 0));
        }
        LeaveCriticalSection(&listViewLock);
    }

    // 统一的列头添加函数，直接使用宽字符串和 SendMessage
    void AddColumn(int col, const WCHAR* text, int width) {
        EnterCriticalSection(&listViewLock);
        HWND target = hListView;
        if (!target || !IsWindow(target)) {
            LeaveCriticalSection(&listViewLock);
            return;
        }
        LVCOLUMNW lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.fmt = LVCFMT_LEFT;
        lvc.cx = width;
        lvc.pszText = (LPWSTR)text;
        SendMessage(target, LVM_INSERTCOLUMNW, col, (LPARAM)&lvc);
        LeaveCriticalSection(&listViewLock);
    }
    
    // 统一添加列表项的函数
    // 输入的文本应该是 UTF-8 编码（源文件和 CTP 消息都已转为 UTF-8）
    void AddItem(int row, int col, const char* text) {
        EnterCriticalSection(&listViewLock);
        HWND target = hListView;
        if (!target || !IsWindow(target)) {
            LeaveCriticalSection(&listViewLock);
            return;
        }
        const char* safeText = text ? text : "";
        WCHAR wtext[256];
        // Limit input length to avoid reading past unterminated strings
        int inLen = SafeStrnlen(safeText, 240);
        int wlen = MultiByteToWideChar(CP_UTF8, 0, safeText, inLen, wtext, 255);
        if (wlen == 0) {
            wlen = MultiByteToWideChar(CP_ACP, 0, safeText, inLen, wtext, 255);
        }
        if (wlen < 0) wlen = 0;
        wtext[wlen] = L'\0';
        if (col == 0) {
            LVITEMW lvi = {0};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = row;
            lvi.iSubItem = 0;
            lvi.pszText = wtext;
            SendMessage(target, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
        } else {
            LVITEMW lvi2 = {0};
            lvi2.iSubItem = col;
            lvi2.pszText = wtext;
            SendMessage(target, LVM_SETITEMTEXTW, row, (LPARAM)&lvi2);
        }
        LeaveCriticalSection(&listViewLock);
    }
    
    virtual void OnFrontConnected() {
        isConnected = true;
        LogMessage("OnFrontConnected called");
        UpdateStatus("已连接服务器，正在认证...");
        CThostFtdcReqAuthenticateField req = {0};
        strcpy(req.BrokerID, brokerID);
        strcpy(req.UserID, userID);
        strcpy(req.AuthCode, authCode);
        strcpy(req.AppID, appID);
        pUserApi->ReqAuthenticate(&req, ++requestID);
    }
    
    virtual void OnFrontDisconnected(int nReason) {
        isConnected = false;
        isAuthenticated = false;
        isLoggedIn = false;
        EndQuery(1);
        EndQuery(2);
        EndQuery(3);
        EndQuery(4);
        char msg[128];
        sprintf(msg, "OnFrontDisconnected: reason=%d", nReason);
        LogMessage(msg);
        sprintf(msg, "连接断开，原因代码: %d", nReason);
        UpdateStatus(msg);
    }
    
    virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, 
                                   CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            char msg[256];
            char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
            sprintf(msg, "认证失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
            return;
        }
        isAuthenticated = true;
        UpdateStatus("认证成功，正在登录...");
        CThostFtdcReqUserLoginField req = {0};
        strcpy(req.BrokerID, brokerID);
        strcpy(req.UserID, userID);
        strcpy(req.Password, password);
        pUserApi->ReqUserLogin(&req, ++requestID);
    }
    
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, 
                               CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            char msg[256];
            char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
            sprintf(msg, "登录失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
            return;
        }
        isLoggedIn = true;
        char msg[256];
        sprintf(msg, "登录成功！交易日: %s", pRspUserLogin ? pRspUserLogin->TradingDay : "未知");
        UpdateStatus(msg);
        
        // 登录成功后，自动确认结算结果
        CThostFtdcSettlementInfoConfirmField req = {0};
        strcpy(req.BrokerID, brokerID);
        strcpy(req.InvestorID, userID);
        pUserApi->ReqSettlementInfoConfirm(&req, ++requestID);
    }
    
    virtual void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            char msg[256];
            char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
            sprintf(msg, "结算单确认失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
        } else {
            UpdateStatus("结算单确认成功，可以开始交易。");
        }
    }
    
    virtual void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    
    // 交易相关回调
    virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);
    virtual void OnRtnTrade(CThostFtdcTradeField *pTrade);
    virtual void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);
    virtual void OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo);
};

void TraderSpi::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "查询委托失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        EndQuery(1);
        return;
    }
    static int row = 0;
    static int lastRequestID = 0;
    if (lastRequestID != nRequestID) {
        ClearListView();
        row = 0;
        int col = 0;
        AddColumn(col++, L"交易日", 80);
        AddColumn(col++, L"合约", 90);
        AddColumn(col++, L"方向", 50);
        AddColumn(col++, L"开平", 60);
        AddColumn(col++, L"价格", 80);
        AddColumn(col++, L"数量", 70);
        AddColumn(col++, L"成交", 70);
        AddColumn(col++, L"状态", 70);
        AddColumn(col++, L"报单时间", 80);
        AddColumn(col++, L"撤单时间", 80);
        std::vector<std::string> headers;
        headers.push_back("交易日");
        headers.push_back("合约");
        headers.push_back("方向");
        headers.push_back("开平");
        headers.push_back("价格");
        headers.push_back("数量");
        headers.push_back("成交");
        headers.push_back("状态");
        headers.push_back("报单时间");
        headers.push_back("撤单时间");
        ResetOrderExport(headers);
        lastRequestID = nRequestID;
    }
    if (pOrder) {
        char buf[256];
        int col = 0;
        std::vector<std::string> rowData;
        AddItem(row, col++, pOrder->TradingDay);
        rowData.push_back(pOrder->TradingDay);
        AddItem(row, col++, pOrder->InstrumentID);
        rowData.push_back(pOrder->InstrumentID);
        const char* dir = (pOrder->Direction == '0') ? "买" : "卖";
        AddItem(row, col++, dir);
        rowData.push_back(dir);
        const char* offset = (pOrder->CombOffsetFlag[0] == '0') ? "开" : "平";
        AddItem(row, col++, offset);
        rowData.push_back(offset);
        FormatDouble(buf, pOrder->LimitPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pOrder->VolumeTotalOriginal);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pOrder->VolumeTraded);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatChar(buf, pOrder->OrderStatus);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        AddItem(row, col++, pOrder->InsertTime);
        rowData.push_back(pOrder->InsertTime);
        AddItem(row, col++, pOrder->CancelTime);
        rowData.push_back(pOrder->CancelTime);
        orderRows.push_back(rowData);
        row++;
    }
    if (bIsLast) {
        UpdateStatus("委托查询完成");
        ExportOrdersIfReady();
        EndQuery(1);
    }
}

void TraderSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "查询持仓失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        EndQuery(2);
        return;
    }
    static int row = 0;
    static int lastRequestID = 0;
    if (lastRequestID != nRequestID) {
        ClearListView();
        row = 0;
        int col = 0;
        AddColumn(col++, L"交易日", 80);
        AddColumn(col++, L"合约", 90);
        AddColumn(col++, L"方向", 50);
        AddColumn(col++, L"持仓", 70);
        AddColumn(col++, L"昨持仓", 70);
        AddColumn(col++, L"今持仓", 70);
        AddColumn(col++, L"开仓成本", 90);
        AddColumn(col++, L"持仓成本", 90);
        AddColumn(col++, L"保证金", 80);
        std::vector<std::string> headers;
        headers.push_back("交易日");
        headers.push_back("合约");
        headers.push_back("方向");
        headers.push_back("持仓");
        headers.push_back("昨持仓");
        headers.push_back("今持仓");
        headers.push_back("开仓成本");
        headers.push_back("持仓成本");
        headers.push_back("保证金");
        ResetPositionExport(headers);
        lastRequestID = nRequestID;
    }
    if (pInvestorPosition) {
        char buf[256];
        int col = 0;
        std::vector<std::string> rowData;
        AddItem(row, col++, pInvestorPosition->TradingDay);
        rowData.push_back(pInvestorPosition->TradingDay);
        AddItem(row, col++, pInvestorPosition->InstrumentID);
        rowData.push_back(pInvestorPosition->InstrumentID);
        FormatChar(buf, pInvestorPosition->PosiDirection);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pInvestorPosition->Position);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pInvestorPosition->YdPosition);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pInvestorPosition->TodayPosition);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pInvestorPosition->OpenCost, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pInvestorPosition->PositionCost, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pInvestorPosition->UseMargin, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        positionRows.push_back(rowData);
        row++;
    }
    if (bIsLast) {
        UpdateStatus("持仓查询完成");
        ExportPositionsIfReady();
        EndQuery(2);
    }
}

void TraderSpi::OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "查询行情失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        if (HasPendingMarketQuery()) {
            int ret = SendNextMarketQuery();
            if (ret != 0) {
                ClearMarketQueryQueue();
                EndQuery(3);
            }
        } else {
            ClearMarketQueryQueue();
            EndQuery(3);
        }
        return;
    }
    static int row = 0;
    static int lastRequestID = 0;
    if (lastRequestID != nRequestID) {
        bool isBatchStart = (!marketBatchActive) || (nRequestID == marketBatchStartRequestID);
        if (isBatchStart) {
            ClearListView();
            row = 0;
            int col = 0;
            AddColumn(col++, L"交易日", 80);
            AddColumn(col++, L"合约", 90);
            AddColumn(col++, L"交易所", 70);
            AddColumn(col++, L"合约在所", 90);
            AddColumn(col++, L"最新价", 80);
            AddColumn(col++, L"昨结算", 80);
            AddColumn(col++, L"昨收", 80);
            AddColumn(col++, L"昨持仓", 80);
            AddColumn(col++, L"开盘", 80);
            AddColumn(col++, L"最高", 80);
            AddColumn(col++, L"最低", 80);
            AddColumn(col++, L"成交量", 80);
            AddColumn(col++, L"成交额", 90);
            AddColumn(col++, L"持仓量", 80);
            AddColumn(col++, L"收盘", 80);
            AddColumn(col++, L"结算", 80);
            AddColumn(col++, L"涨停", 80);
            AddColumn(col++, L"跌停", 80);
            AddColumn(col++, L"昨Delta", 80);
            AddColumn(col++, L"今Delta", 80);
            AddColumn(col++, L"更新时间", 80);
            AddColumn(col++, L"毫秒", 60);
            AddColumn(col++, L"买一价", 80);
            AddColumn(col++, L"买一量", 80);
            AddColumn(col++, L"卖一价", 80);
            AddColumn(col++, L"卖一量", 80);
            AddColumn(col++, L"买二价", 80);
            AddColumn(col++, L"买二量", 80);
            AddColumn(col++, L"卖二价", 80);
            AddColumn(col++, L"卖二量", 80);
            AddColumn(col++, L"买三价", 80);
            AddColumn(col++, L"买三量", 80);
            AddColumn(col++, L"卖三价", 80);
            AddColumn(col++, L"卖三量", 80);
            AddColumn(col++, L"买四价", 80);
            AddColumn(col++, L"买四量", 80);
            AddColumn(col++, L"卖四价", 80);
            AddColumn(col++, L"卖四量", 80);
            AddColumn(col++, L"买五价", 80);
            AddColumn(col++, L"买五量", 80);
            AddColumn(col++, L"卖五价", 80);
            AddColumn(col++, L"卖五量", 80);
            AddColumn(col++, L"均价", 80);
            AddColumn(col++, L"业务日", 80);
            AddColumn(col++, L"涨停带", 80);
            AddColumn(col++, L"跌停带", 80);
            
            std::vector<std::string> headers;
            headers.push_back("交易日");
            headers.push_back("合约");
            headers.push_back("交易所");
            headers.push_back("合约在所");
            headers.push_back("最新价");
            headers.push_back("昨结算");
            headers.push_back("昨收");
            headers.push_back("昨持仓");
            headers.push_back("开盘");
            headers.push_back("最高");
            headers.push_back("最低");
            headers.push_back("成交量");
            headers.push_back("成交额");
            headers.push_back("持仓量");
            headers.push_back("收盘");
            headers.push_back("结算");
            headers.push_back("涨停");
            headers.push_back("跌停");
            headers.push_back("昨Delta");
            headers.push_back("今Delta");
            headers.push_back("更新时间");
            headers.push_back("毫秒");
            headers.push_back("买一价");
            headers.push_back("买一量");
            headers.push_back("卖一价");
            headers.push_back("卖一量");
            headers.push_back("买二价");
            headers.push_back("买二量");
            headers.push_back("卖二价");
            headers.push_back("卖二量");
            headers.push_back("买三价");
            headers.push_back("买三量");
            headers.push_back("卖三价");
            headers.push_back("卖三量");
            headers.push_back("买四价");
            headers.push_back("买四量");
            headers.push_back("卖四价");
            headers.push_back("卖四量");
            headers.push_back("买五价");
            headers.push_back("买五量");
            headers.push_back("卖五价");
            headers.push_back("卖五量");
            headers.push_back("均价");
            headers.push_back("业务日");
            headers.push_back("涨停带");
            headers.push_back("跌停带");
            ResetMarketExport(headers);
        }
        lastRequestID = nRequestID;
    }
    if (pDepthMarketData) {
        char buf[256];
        int col = 0;
        std::vector<std::string> rowData;
        char tradingDay[16], instrumentID[64], exchangeID[16], exchangeInstID[64], updateTime[16], actionDay[16];
        CopyFixedField(tradingDay, sizeof(tradingDay), pDepthMarketData->TradingDay, sizeof(pDepthMarketData->TradingDay));
        CopyFixedField(instrumentID, sizeof(instrumentID), pDepthMarketData->InstrumentID, sizeof(pDepthMarketData->InstrumentID));
        CopyFixedField(exchangeID, sizeof(exchangeID), pDepthMarketData->ExchangeID, sizeof(pDepthMarketData->ExchangeID));
        CopyFixedField(exchangeInstID, sizeof(exchangeInstID), pDepthMarketData->ExchangeInstID, sizeof(pDepthMarketData->ExchangeInstID));
        CopyFixedField(updateTime, sizeof(updateTime), pDepthMarketData->UpdateTime, sizeof(pDepthMarketData->UpdateTime));
        CopyFixedField(actionDay, sizeof(actionDay), pDepthMarketData->ActionDay, sizeof(pDepthMarketData->ActionDay));

        AddItem(row, col++, tradingDay);
        rowData.push_back(tradingDay);
        AddItem(row, col++, instrumentID);
        rowData.push_back(instrumentID);
        AddItem(row, col++, exchangeID);
        rowData.push_back(exchangeID);
        AddItem(row, col++, exchangeInstID);
        rowData.push_back(exchangeInstID);
        FormatDouble(buf, pDepthMarketData->LastPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->PreSettlementPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->PreClosePrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->PreOpenInterest);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->OpenPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->HighestPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->LowestPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->Volume);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->Turnover, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->OpenInterest);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->ClosePrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->SettlementPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->UpperLimitPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->LowerLimitPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->PreDelta, 6);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->CurrDelta, 6);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        AddItem(row, col++, updateTime);
        rowData.push_back(updateTime);
        FormatInt64(buf, pDepthMarketData->UpdateMillisec);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->BidPrice1, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->BidVolume1);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->AskPrice1, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->AskVolume1);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->BidPrice2, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->BidVolume2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->AskPrice2, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->AskVolume2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->BidPrice3, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->BidVolume3);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->AskPrice3, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->AskVolume3);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->BidPrice4, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->BidVolume4);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->AskPrice4, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->AskVolume4);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->BidPrice5, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->BidVolume5);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->AskPrice5, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatInt64(buf, pDepthMarketData->AskVolume5);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->AveragePrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        AddItem(row, col++, actionDay);
        rowData.push_back(actionDay);
        FormatDouble(buf, pDepthMarketData->BandingUpperPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);
        FormatDouble(buf, pDepthMarketData->BandingLowerPrice, 2);
        AddItem(row, col++, buf);
        rowData.push_back(buf);

        // 优先使用有效交易日，其次业务日，最后回退今天日期，避免文件名日期异常
        if (IsValidDate8(tradingDay)) {
            marketTradingDay = tradingDay;
        } else if (IsValidDate8(actionDay)) {
            marketTradingDay = actionDay;
        } else if (marketTradingDay.empty()) {
            char today[9];
            GetTodayYYYYMMDD(today, sizeof(today));
            marketTradingDay = today;
        }
        marketRows.push_back(rowData);
        row++;
    }
    if (bIsLast) {
        if (HasPendingMarketQuery()) {
            int ret = SendNextMarketQuery();
            if (ret != 0) {
                ClearMarketQueryQueue();
                EndQuery(3);
            }
        } else {
            UpdateStatus("行情查询完成");
            ExportMarketIfReady();
            ClearMarketQueryQueue();
            EndQuery(3);
        }
    }
}

// 判断是否是主力合约的辅助函数
static bool IsMainContract(const char* instrumentID) {
    // 提取合约的年月信息
    // 例如: "IF2601" -> "2601", "CU2602" -> "2602"
    int len = strlen(instrumentID);
    if (len < 4) return false;
    
    // 查找数字开始的位置
    int digitStart = -1;
    for (int i = 0; i < len; i++) {
        if (instrumentID[i] >= '0' && instrumentID[i] <= '9') {
            digitStart = i;
            break;
        }
    }
    
    if (digitStart == -1 || len - digitStart < 4) return false;
    
    // 提取年月部分 (YYMM)
    char yearMonth[5];
    strncpy(yearMonth, instrumentID + digitStart, 4);
    yearMonth[4] = '\0';
    
    // 获取当前年月
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    int currentYear = (t->tm_year + 1900) % 100;  // 取后两位
    int currentMonth = t->tm_mon + 1;
    
    // 解析合约年月
    int contractYear = (yearMonth[0] - '0') * 10 + (yearMonth[1] - '0');
    int contractMonth = (yearMonth[2] - '0') * 10 + (yearMonth[3] - '0');
    
    // 计算月份差
    int monthDiff = (contractYear - currentYear) * 12 + (contractMonth - currentMonth);
    
    // 主力合约判断规则：
    // 1. 当月及未来3个月内的合约
    if (monthDiff >= 0 && monthDiff <= 3) {
        return true;
    }
    
    // 2. 季月合约 (3, 6, 9, 12月) - 一年内的
    if (contractMonth == 3 || contractMonth == 6 || contractMonth == 9 || contractMonth == 12) {
        if (monthDiff >= 0 && monthDiff <= 12) {
            return true;
        }
    }
    
    return false;
}

void TraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "查询合约失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        EndQuery(4);
        return;
    }
    
    // 使用静态变量存储合约信息
    struct InstrumentInfo {
        char instrumentID[31];
        char instrumentName[61];
        char exchangeID[9];
        char exchangeInstID[31];
        char productID[31];
        char underlyingInstrID[31];
        char productClass;
        int deliveryYear;
        int deliveryMonth;
        int maxMarketOrderVolume;
        int minMarketOrderVolume;
        int maxLimitOrderVolume;
        int minLimitOrderVolume;
        int volumeMultiple;
        double priceTick;
        char createDate[9];
        char expireDate[9];
        char openDate[9];
        char startDelivDate[9];
        char endDelivDate[9];
        char instLifePhase;
        char isTrading;
        char positionType;
        char positionDateType;
        double longMarginRatio;
        double shortMarginRatio;
        char maxMarginSideAlgorithm;
        double strikePrice;
        char optionsType;
        double underlyingMultiple;
        char combinationType;
    };
    
    static InstrumentInfo* instruments = NULL;
    static int instrumentCount = 0;
    static int totalCount = 0;
    static int lastRequestID = 0;
    const int MAX_INSTRUMENTS = 5000; // 支持更多期权
    const int MAX_DISPLAY = 5000;
    
    if (lastRequestID != nRequestID) {
        // 新的查询请求，清空之前的数据
        if (instruments) {
            delete[] instruments;
            instruments = NULL;
        }
        instruments = new InstrumentInfo[MAX_INSTRUMENTS];  // 预分配空间
        instrumentCount = 0;
        totalCount = 0;
        lastRequestID = nRequestID;
        
        ClearListView();
        int col = 0;
        AddColumn(col++, L"合约", 90);
        AddColumn(col++, L"名称", 140);
        AddColumn(col++, L"交易所", 70);
        AddColumn(col++, L"合约在所", 90);
        AddColumn(col++, L"品种", 80);
        AddColumn(col++, L"产品类型", 70);
        AddColumn(col++, L"交割年", 70);
        AddColumn(col++, L"交割月", 70);
        AddColumn(col++, L"市价单最大", 90);
        AddColumn(col++, L"市价单最小", 90);
        AddColumn(col++, L"限价单最大", 90);
        AddColumn(col++, L"限价单最小", 90);
        AddColumn(col++, L"合约乘数", 80);
        AddColumn(col++, L"最小变动", 80);
        AddColumn(col++, L"创建日", 80);
        AddColumn(col++, L"上市日", 80);
        AddColumn(col++, L"到期日", 80);
        AddColumn(col++, L"开始交割", 90);
        AddColumn(col++, L"结束交割", 90);
        AddColumn(col++, L"生命周期", 70);
        AddColumn(col++, L"可交易", 60);
        AddColumn(col++, L"持仓类型", 70);
        AddColumn(col++, L"持仓日期类型", 90);
        AddColumn(col++, L"多头保证金", 90);
        AddColumn(col++, L"空头保证金", 90);
        AddColumn(col++, L"最大保证金算法", 110);
        AddColumn(col++, L"执行价", 80);
        AddColumn(col++, L"期权类型", 70);
        AddColumn(col++, L"合约乘数(标的)", 120);
        AddColumn(col++, L"组合类型", 70);
        AddColumn(col++, L"标的合约", 90);
        
        std::vector<std::string> headers;
        headers.push_back("合约");
        headers.push_back("名称");
        headers.push_back("交易所");
        headers.push_back("合约在所");
        headers.push_back("品种");
        headers.push_back("产品类型");
        headers.push_back("交割年");
        headers.push_back("交割月");
        headers.push_back("市价单最大");
        headers.push_back("市价单最小");
        headers.push_back("限价单最大");
        headers.push_back("限价单最小");
        headers.push_back("合约乘数");
        headers.push_back("最小变动");
        headers.push_back("创建日");
        headers.push_back("上市日");
        headers.push_back("到期日");
        headers.push_back("开始交割");
        headers.push_back("结束交割");
        headers.push_back("生命周期");
        headers.push_back("可交易");
        headers.push_back("持仓类型");
        headers.push_back("持仓日期类型");
        headers.push_back("多头保证金");
        headers.push_back("空头保证金");
        headers.push_back("最大保证金算法");
        headers.push_back("执行价");
        headers.push_back("期权类型");
        headers.push_back("合约乘数(标的)");
        headers.push_back("组合类型");
        headers.push_back("标的合约");
        ResetInstrumentExport(headers);
    }
    
    if (pInstrument) {
        totalCount++;
        
        bool accept = false;
        if (isOptionQuery) {
            // 期权：不过滤主力，产品类型为期权或现货期权都收
            if ((pInstrument->ProductClass == '2' || pInstrument->ProductClass == '6') && instrumentCount < MAX_INSTRUMENTS) {
                accept = true;
            }
        } else {
            // 原逻辑：仅主力期货合约且未过期
            if (pInstrument->ProductClass == '1' && instrumentCount < MAX_INSTRUMENTS) {
                time_t now = time(NULL);
                struct tm* t = localtime(&now);
                char today[9];
                sprintf(today, "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
                if (strlen(pInstrument->ExpireDate) > 0 && 
                    strcmp(pInstrument->ExpireDate, today) >= 0 &&
                    IsMainContract(pInstrument->InstrumentID)) {
                    accept = true;
                }
            }
        }

        if (accept) {
            // 保存到数组
            strcpy(instruments[instrumentCount].instrumentID, pInstrument->InstrumentID);
            
            // 转换合约名称从 GBK 到 UTF-8
            char* utf8Name = GbkToUtf8(pInstrument->InstrumentName);
            if (utf8Name) {
                strncpy(instruments[instrumentCount].instrumentName, utf8Name, 60);
                instruments[instrumentCount].instrumentName[60] = '\0';
                delete[] utf8Name;
            } else {
                strcpy(instruments[instrumentCount].instrumentName, pInstrument->InstrumentName);
            }
            
            strcpy(instruments[instrumentCount].exchangeID, pInstrument->ExchangeID);
            strcpy(instruments[instrumentCount].exchangeInstID, pInstrument->ExchangeInstID);
            strcpy(instruments[instrumentCount].productID, pInstrument->ProductID);
            strcpy(instruments[instrumentCount].underlyingInstrID, pInstrument->UnderlyingInstrID);
            instruments[instrumentCount].productClass = pInstrument->ProductClass;
            instruments[instrumentCount].deliveryYear = pInstrument->DeliveryYear;
            instruments[instrumentCount].deliveryMonth = pInstrument->DeliveryMonth;
            instruments[instrumentCount].maxMarketOrderVolume = pInstrument->MaxMarketOrderVolume;
            instruments[instrumentCount].minMarketOrderVolume = pInstrument->MinMarketOrderVolume;
            instruments[instrumentCount].maxLimitOrderVolume = pInstrument->MaxLimitOrderVolume;
            instruments[instrumentCount].minLimitOrderVolume = pInstrument->MinLimitOrderVolume;
            instruments[instrumentCount].volumeMultiple = pInstrument->VolumeMultiple;
            instruments[instrumentCount].priceTick = pInstrument->PriceTick;
            strcpy(instruments[instrumentCount].createDate, pInstrument->CreateDate);
            strcpy(instruments[instrumentCount].expireDate, pInstrument->ExpireDate);
            strcpy(instruments[instrumentCount].openDate, pInstrument->OpenDate);
            strcpy(instruments[instrumentCount].startDelivDate, pInstrument->StartDelivDate);
            strcpy(instruments[instrumentCount].endDelivDate, pInstrument->EndDelivDate);
            instruments[instrumentCount].instLifePhase = pInstrument->InstLifePhase;
            instruments[instrumentCount].isTrading = pInstrument->IsTrading;
            instruments[instrumentCount].positionType = pInstrument->PositionType;
            instruments[instrumentCount].positionDateType = pInstrument->PositionDateType;
            instruments[instrumentCount].longMarginRatio = pInstrument->LongMarginRatio;
            instruments[instrumentCount].shortMarginRatio = pInstrument->ShortMarginRatio;
            instruments[instrumentCount].maxMarginSideAlgorithm = pInstrument->MaxMarginSideAlgorithm;
            instruments[instrumentCount].strikePrice = pInstrument->StrikePrice;
            instruments[instrumentCount].optionsType = pInstrument->OptionsType;
            instruments[instrumentCount].underlyingMultiple = pInstrument->UnderlyingMultiple;
            instruments[instrumentCount].combinationType = pInstrument->CombinationType;
            instrumentCount++;
        }
    }
    
    if (bIsLast && instruments) {
        // 简单的冒泡排序：按到期日从近到远排序（最近到期的在前面）
        for (int i = 0; i < instrumentCount - 1; i++) {
            for (int j = 0; j < instrumentCount - i - 1; j++) {
                if (strcmp(instruments[j].expireDate, instruments[j+1].expireDate) > 0) {
                    InstrumentInfo temp = instruments[j];
                    instruments[j] = instruments[j+1];
                    instruments[j+1] = temp;
                }
            }
        }
        
        // 显示前MAX_DISPLAY条
        int displayCount = instrumentCount < MAX_DISPLAY ? instrumentCount : MAX_DISPLAY;
        for (int i = 0; i < displayCount; i++) {
            char buf[256];
            int col = 0;
            std::vector<std::string> rowData;
            AddItem(i, col++, instruments[i].instrumentID);
            rowData.push_back(instruments[i].instrumentID);
            AddItem(i, col++, instruments[i].instrumentName);
            rowData.push_back(instruments[i].instrumentName);
            AddItem(i, col++, instruments[i].exchangeID);
            rowData.push_back(instruments[i].exchangeID);
            AddItem(i, col++, instruments[i].exchangeInstID);
            rowData.push_back(instruments[i].exchangeInstID);
            AddItem(i, col++, instruments[i].productID);
            rowData.push_back(instruments[i].productID);
            FormatChar(buf, instruments[i].productClass);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].deliveryYear);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].deliveryMonth);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].maxMarketOrderVolume);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].minMarketOrderVolume);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].maxLimitOrderVolume);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].minLimitOrderVolume);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            sprintf(buf, "%d", instruments[i].volumeMultiple);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            sprintf(buf, "%.4f", instruments[i].priceTick);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            AddItem(i, col++, instruments[i].createDate);
            rowData.push_back(instruments[i].createDate);
            AddItem(i, col++, instruments[i].openDate);
            rowData.push_back(instruments[i].openDate);
            AddItem(i, col++, instruments[i].expireDate);
            rowData.push_back(instruments[i].expireDate);
            AddItem(i, col++, instruments[i].startDelivDate);
            rowData.push_back(instruments[i].startDelivDate);
            AddItem(i, col++, instruments[i].endDelivDate);
            rowData.push_back(instruments[i].endDelivDate);
            FormatChar(buf, instruments[i].instLifePhase);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].isTrading);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].positionType);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].positionDateType);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatDouble(buf, instruments[i].longMarginRatio, 6);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatDouble(buf, instruments[i].shortMarginRatio, 6);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].maxMarginSideAlgorithm);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatDouble(buf, instruments[i].strikePrice, 2);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].optionsType);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatDouble(buf, instruments[i].underlyingMultiple, 2);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].combinationType);
            AddItem(i, col++, buf);
            rowData.push_back(buf);
            AddItem(i, col++, instruments[i].underlyingInstrID);
            rowData.push_back(instruments[i].underlyingInstrID);
            
            instrumentRows.push_back(rowData);
        }
        
        // 显示统计信息
        char msg[256];
        sprintf(msg, "主力合约查询完成，总合约数: %d，主力合约: %d，已显示: %d（按到期日排序）", 
                totalCount, instrumentCount, displayCount);
        UpdateStatus(msg);
        ExportInstrumentIfReady(isOptionQuery);
        EndQuery(4);
        
        // 清理内存
        delete[] instruments;
        instruments = NULL;
    }
}

// 报单响应
void TraderSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[512];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "报单失败: ErrorID=%d, %s", pRspInfo->ErrorID, gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        LogMessage(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
    } else {
        char msg[256];
        sprintf(msg, "报单请求已提交: %s", pInputOrder ? pInputOrder->InstrumentID : "");
        UpdateStatus(msg);
        LogMessage(msg);
    }
}

// 撤单响应
void TraderSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[512];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "撤单失败: ErrorID=%d, %s", pRspInfo->ErrorID, gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        LogMessage(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
    } else {
        char msg[256];
        sprintf(msg, "撤单请求已提交");
        UpdateStatus(msg);
        LogMessage(msg);
    }
}

// 报单回报
void TraderSpi::OnRtnOrder(CThostFtdcOrderField *pOrder) {
    if (!pOrder) return;
    
    // StatusMsg 来自 CTP API，是 GBK 编码，需要转换
    char* utf8Status = GbkToUtf8(pOrder->StatusMsg);
    
    char msg[512];
    sprintf(msg, "报单回报: %s %s %s, 价格:%.2f, 数量:%d, 状态:%s", 
            pOrder->InstrumentID,
            pOrder->Direction == '0' ? "买" : "卖",
            pOrder->CombOffsetFlag[0] == '0' ? "开仓" : "平仓",
            pOrder->LimitPrice,
            pOrder->VolumeTotalOriginal,
            utf8Status ? utf8Status : pOrder->StatusMsg);
    
    UpdateStatus(msg);
    LogMessage(msg);
    
    if (utf8Status) delete[] utf8Status;
}

// 成交回报
void TraderSpi::OnRtnTrade(CThostFtdcTradeField *pTrade) {
    if (!pTrade) return;
    
    char msg[512];
    sprintf(msg, "成交回报: %s %s %s, 价格:%.2f, 数量:%d, 时间:%s", 
            pTrade->InstrumentID,
            pTrade->Direction == '0' ? "买" : "卖",
            pTrade->OffsetFlag == '0' ? "开仓" : "平仓",
            pTrade->Price,
            pTrade->Volume,
            pTrade->TradeTime);
    
    UpdateStatus(msg);
    UpdateStatus(msg);
    LogMessage(msg);
}

// 报单录入错误回报
void TraderSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo) {
    if (pRspInfo) {
        char msg[512];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "报单错误: ErrorID=%d, %s", pRspInfo->ErrorID, gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        LogMessage(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
    }
}

// 撤单操作错误回报
void TraderSpi::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo) {
    if (pRspInfo) {
        char msg[512];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "撤单错误: ErrorID=%d, %s", pRspInfo->ErrorID, gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        LogMessage(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
    }
}

class MdSpi : public CThostFtdcMdSpi {
public:
    CThostFtdcMdApi* pMdApi;
    StatusCallback statusCallback;
    HWND hMainWnd;
    int requestID;
    bool isConnected;
    bool isLoggedIn;
    char brokerID[11];
    char userID[16];
    char password[41];

    MdSpi() {
        pMdApi = NULL;
        statusCallback = NULL;
        hMainWnd = NULL;
        requestID = 0;
        isConnected = false;
        isLoggedIn = false;
        memset(brokerID, 0, sizeof(brokerID));
        memset(userID, 0, sizeof(userID));
        memset(password, 0, sizeof(password));
    }

    void UpdateStatus(const char* msg) {
        if (statusCallback) statusCallback(msg);
    }

    virtual void OnFrontConnected() {
        isConnected = true;
        UpdateStatus("行情连接成功，正在登录...");
        if (!pMdApi) return;
        CThostFtdcReqUserLoginField req = {0};
        strncpy(req.BrokerID, brokerID, sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, userID, sizeof(req.UserID) - 1);
        strncpy(req.Password, password, sizeof(req.Password) - 1);
        pMdApi->ReqUserLogin(&req, ++requestID);
    }

    virtual void OnFrontDisconnected(int nReason) {
        (void)nReason;
        isConnected = false;
        isLoggedIn = false;
        UpdateStatus("行情连接断开");
    }

    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin,
                                CThostFtdcRspInfoField* pRspInfo,
                                int nRequestID, bool bIsLast) {
        (void)pRspUserLogin;
        (void)nRequestID;
        if (!bIsLast) return;
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            char msg[256];
            char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
            sprintf(msg, "行情登录失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
            return;
        }
        isLoggedIn = true;
        UpdateStatus("行情登录成功");
    }

    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pDepthMarketData) {
        if (!pDepthMarketData || !hMainWnd) return;
        MdUpdate* u = (MdUpdate*)malloc(sizeof(MdUpdate));
        if (!u) return;
        memset(u, 0, sizeof(MdUpdate));

        CopyFixedField(u->instrumentID, sizeof(u->instrumentID),
                       pDepthMarketData->InstrumentID, sizeof(pDepthMarketData->InstrumentID));
        CopyFixedField(u->updateTime, sizeof(u->updateTime),
                       pDepthMarketData->UpdateTime, sizeof(pDepthMarketData->UpdateTime));
        u->lastPrice = pDepthMarketData->LastPrice;
        u->volume = pDepthMarketData->Volume;
        u->bidPrice1 = pDepthMarketData->BidPrice1;
        u->bidVolume1 = pDepthMarketData->BidVolume1;
        u->askPrice1 = pDepthMarketData->AskPrice1;
        u->askVolume1 = pDepthMarketData->AskVolume1;
        u->updateMillisec = (int)pDepthMarketData->UpdateMillisec;

        PostMessage(hMainWnd, WM_APP_MD_UPDATE, 0, (LPARAM)u);
    }
};

static void SplitInstrumentsCsv(const char* csv, std::vector<std::string>& out) {
    out.clear();
    if (!csv) return;
    const char* p = csv;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',' || *p == ';') p++;
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != ',' && *p != ';') p++;
        std::string s(start, p - start);
        if (!s.empty()) out.push_back(s);
    }
}

struct CTPTrader {
    TraderSpi* pSpi;
    MdSpi* pMdSpi;
    CThostFtdcMdApi* pMdApi;
    HWND hMainWnd;
};

extern "C" CTPTrader* CreateCTPTrader() {
    LogMessage("CreateCTPTrader called");
    CTPTrader* trader = NULL;
    try {
        trader = new CTPTrader();
        LogMessage("CTPTrader struct allocated");
        
        trader->pSpi = new TraderSpi();
        LogMessage("TraderSpi created");

        trader->pMdSpi = new MdSpi();
        trader->pMdApi = NULL;
        trader->hMainWnd = NULL;
        LogMessage("MdSpi created");
        
        LogMessage("CreateCTPTrader completed successfully");
    } catch (...) {
        LogMessage("ERROR: Exception in CreateCTPTrader");
        if (trader) {
            if (trader->pSpi) delete trader->pSpi;
            delete trader;
        }
        return NULL;
    }
    return trader;
}

extern "C" void DestroyCTPTrader(CTPTrader* trader) {
    if (trader) {
        if (trader->pMdApi) {
            trader->pMdApi->RegisterSpi(NULL);
            trader->pMdApi->Release();
            trader->pMdApi = NULL;
        }
        if (trader->pMdSpi) {
            delete trader->pMdSpi;
            trader->pMdSpi = NULL;
        }
        if (trader->pSpi && trader->pSpi->pUserApi) {
            trader->pSpi->pUserApi->RegisterSpi(NULL);
            trader->pSpi->pUserApi->Release();
            trader->pSpi->pUserApi = NULL;
        }
        if (trader->pSpi) { delete trader->pSpi; }
        delete trader;
    }
}

extern "C" void Disconnect(CTPTrader* trader) {
    if (!trader) return;
    if (trader->pMdApi) {
        trader->pMdApi->RegisterSpi(NULL);
        trader->pMdApi->Release();
        trader->pMdApi = NULL;
    }
    if (trader->pMdSpi) {
        trader->pMdSpi->pMdApi = NULL;
        trader->pMdSpi->isConnected = false;
        trader->pMdSpi->isLoggedIn = false;
    }
    if (trader->pSpi) {
        if (trader->pSpi->pUserApi) {
            trader->pSpi->pUserApi->RegisterSpi(NULL);
            trader->pSpi->pUserApi->Release();
            trader->pSpi->pUserApi = NULL;
        }
        trader->pSpi->isConnected = false;
        trader->pSpi->isAuthenticated = false;
        trader->pSpi->isLoggedIn = false;
        trader->pSpi->UpdateStatus("?????");
    }
}


extern "C" void SetMainWindow(CTPTrader* trader, HWND hMainWnd) {
    if (!trader) return;
    trader->hMainWnd = hMainWnd;
    if (trader->pMdSpi) trader->pMdSpi->hMainWnd = hMainWnd;
}

extern "C" void SetListView(CTPTrader* trader, HWND hListView) {
    if (trader && trader->pSpi) {
        EnterCriticalSection(&trader->pSpi->listViewLock);
        trader->pSpi->hListView = hListView;
        LeaveCriticalSection(&trader->pSpi->listViewLock);
    }
}

extern "C" void SetStatusCallback(CTPTrader* trader, StatusCallback callback) {
    if (trader && trader->pSpi) { trader->pSpi->statusCallback = callback; }
    if (trader && trader->pMdSpi) { trader->pMdSpi->statusCallback = callback; }
}

extern "C" int ConnectAndLogin(CTPTrader* trader, const char* brokerID, const char* userID, const char* password, const char* frontAddr, const char* authCode, const char* appID) {
    LogMessage("ConnectAndLogin called");
    if (!trader || !trader->pSpi) {
        LogMessage("ERROR: trader or pSpi is NULL");
        return -1;
    }
    TraderSpi* pSpi = trader->pSpi;
    
    // Check if already initialized
    if (pSpi->pUserApi != NULL) {
        pSpi->UpdateStatus("已连接或正在连接中...");
        LogMessage("WARNING: Already connected");
        return -1;
    }
    
    // Create flow directory if not exists
    CreateDirectoryA("flow", NULL);
    LogMessage("flow directory created");
    
    strncpy(pSpi->brokerID, brokerID, sizeof(pSpi->brokerID) - 1);
    strncpy(pSpi->userID, userID, sizeof(pSpi->userID) - 1);
    strncpy(pSpi->password, password, sizeof(pSpi->password) - 1);
    strncpy(pSpi->authCode, authCode, sizeof(pSpi->authCode) - 1);
    strncpy(pSpi->appID, appID, sizeof(pSpi->appID) - 1);
    
    char logMsg[256];
    sprintf(logMsg, "Credentials set: BrokerID=%s, UserID=%s, Front=%s", brokerID, userID, frontAddr);
    LogMessage(logMsg);
    
    try {
        LogMessage("Creating API instance...");
        pSpi->pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi("./flow/");
        if (!pSpi->pUserApi) {
            pSpi->UpdateStatus("创建API实例失败");
            LogMessage("ERROR: Failed to create API instance");
            return -1;
        }
        LogMessage("API instance created");
        
        pSpi->pUserApi->RegisterSpi(pSpi);
        LogMessage("SPI registered");
        
        try {
            pSpi->pUserApi->SubscribePublicTopic(THOST_TERT_QUICK);
            LogMessage("SubscribePublicTopic called");
        } catch (...) {
            LogMessage("ERROR: SubscribePublicTopic threw exception");
            throw;
        }
        
        try {
            pSpi->pUserApi->SubscribePrivateTopic(THOST_TERT_QUICK);
            LogMessage("SubscribePrivateTopic called");
        } catch (...) {
            LogMessage("ERROR: SubscribePrivateTopic threw exception");
            throw;
        }
        
        LogMessage("Topics subscribed");
        
        try {
            pSpi->pUserApi->RegisterFront((char*)frontAddr);
            LogMessage("RegisterFront called");
        } catch (...) {
            LogMessage("ERROR: RegisterFront threw exception");
            throw;
        }
        
        LogMessage("Front registered");
        
        try {
            pSpi->pUserApi->Init();
            LogMessage("Init called");
        } catch (...) {
            LogMessage("ERROR: Init threw exception");
            throw;
        }
        
        LogMessage("API Init called - waiting for connection...");
        
        pSpi->UpdateStatus("API已初始化，正在连接...");
    } catch (...) {
        pSpi->UpdateStatus("API初始化过程中发生异常");
        LogMessage("EXCEPTION: During API initialization");
        return -1;
    }
    
    LogMessage("ConnectAndLogin finished successfully");
    return 0;
}

extern "C" int IsLoggedIn(CTPTrader* trader) {
    if (!trader || !trader->pSpi) return 0;
    return trader->pSpi->isLoggedIn ? 1 : 0;
}

extern "C" int QueryOrders(CTPTrader* trader) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    if (!trader->pSpi->BeginQuery(1)) {
        trader->pSpi->UpdateStatus("委托查询进行中，请稍后重试");
        return -3;
    }
    CThostFtdcQryOrderField req = {0};
    strcpy(req.BrokerID, trader->pSpi->brokerID);
    strcpy(req.InvestorID, trader->pSpi->userID);
    Sleep(1000);
    int ret = trader->pSpi->pUserApi->ReqQryOrder(&req, ++trader->pSpi->requestID);
    if (ret != 0) trader->pSpi->EndQuery(1);
    return ret;
}

extern "C" int QueryPositions(CTPTrader* trader) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    if (!trader->pSpi->BeginQuery(2)) {
        trader->pSpi->UpdateStatus("持仓查询进行中，请稍后重试");
        return -3;
    }
    CThostFtdcQryInvestorPositionField req = {0};
    strcpy(req.BrokerID, trader->pSpi->brokerID);
    strcpy(req.InvestorID, trader->pSpi->userID);
    Sleep(1000);
    int ret = trader->pSpi->pUserApi->ReqQryInvestorPosition(&req, ++trader->pSpi->requestID);
    if (ret != 0) trader->pSpi->EndQuery(2);
    return ret;
}

extern "C" int QueryMarketData(CTPTrader* trader, const char* instrumentID) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    std::vector<std::string> insts;
    if (instrumentID && instrumentID[0]) insts.push_back(instrumentID);
    if (insts.empty()) {
        trader->pSpi->UpdateStatus("请输入合约代码");
        return -1;
    }
    return trader->pSpi->StartMarketQueryBatch(insts);
}

extern "C" int QueryMarketDataBatch(CTPTrader* trader, const char* instrumentsCsv) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    std::vector<std::string> insts;
    SplitInstrumentsCsv(instrumentsCsv, insts);
    if (insts.empty()) {
        trader->pSpi->UpdateStatus("请输入合约代码(支持逗号/空格/换行)");
        return -1;
    }
    return trader->pSpi->StartMarketQueryBatch(insts);
}

extern "C" int QueryInstrument(CTPTrader* trader, const char* instrumentID) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    if (!trader->pSpi->BeginQuery(4)) {
        trader->pSpi->UpdateStatus("合约查询进行中，请稍后重试");
        return -3;
    }
    CThostFtdcQryInstrumentField req = {0};
    if (instrumentID && instrumentID[0]) {
        strncpy(req.InstrumentID, instrumentID, sizeof(req.InstrumentID) - 1);
        req.InstrumentID[sizeof(req.InstrumentID) - 1] = '\0';
    }
    Sleep(1000);
    int ret = trader->pSpi->pUserApi->ReqQryInstrument(&req, ++trader->pSpi->requestID);
    if (ret != 0) trader->pSpi->EndQuery(4);
    return ret;
}

extern "C" int QueryOptions(CTPTrader* trader) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    if (!trader->pSpi->BeginQuery(4)) {
        trader->pSpi->UpdateStatus("合约查询进行中，请稍后重试");
        return -3;
    }
    trader->pSpi->isOptionQuery = true;
    CThostFtdcQryInstrumentField req = {0};
    // 空InstrumentID表示查询全部合约，包括全部期权
    Sleep(1000);
    int ret = trader->pSpi->pUserApi->ReqQryInstrument(&req, ++trader->pSpi->requestID);
    if (ret != 0) trader->pSpi->EndQuery(4);
    return ret;
}

extern "C" int ConnectMarket(CTPTrader* trader, const char* mdFrontAddr) {
    if (!trader || !trader->pMdSpi) return -1;
    if (trader->pMdApi != NULL) {
        trader->pMdSpi->UpdateStatus("行情已连接或正在连接中...");
        return -1;
    }
    if (!mdFrontAddr || !mdFrontAddr[0]) {
        trader->pMdSpi->UpdateStatus("错误: 行情前置地址为空");
        return -1;
    }
    if (!trader->pSpi) {
        trader->pMdSpi->UpdateStatus("错误: 交易对象未初始化");
        return -1;
    }
    if (!trader->pSpi->brokerID[0] || !trader->pSpi->userID[0] || !trader->pSpi->password[0]) {
        trader->pMdSpi->UpdateStatus("错误: 请先填写并连接交易账户(用于行情登录)");
        return -1;
    }

    // Reuse trading credentials for MD login.
    strncpy(trader->pMdSpi->brokerID, trader->pSpi->brokerID, sizeof(trader->pMdSpi->brokerID) - 1);
    strncpy(trader->pMdSpi->userID, trader->pSpi->userID, sizeof(trader->pMdSpi->userID) - 1);
    strncpy(trader->pMdSpi->password, trader->pSpi->password, sizeof(trader->pMdSpi->password) - 1);

    CreateDirectoryA("mdflow", NULL);
    trader->pMdApi = CThostFtdcMdApi::CreateFtdcMdApi("./mdflow/");
    if (!trader->pMdApi) {
        trader->pMdSpi->UpdateStatus("错误: 创建行情API失败");
        return -1;
    }
    trader->pMdSpi->pMdApi = trader->pMdApi;
    trader->pMdApi->RegisterSpi(trader->pMdSpi);

    // Register front and init.
    char* front = (char*)mdFrontAddr;
    trader->pMdApi->RegisterFront(front);
    trader->pMdApi->Init();
    trader->pMdSpi->UpdateStatus("正在连接行情前置...");
    return 0;
}

extern "C" int SubscribeMarketData(CTPTrader* trader, const char* instrumentsCsv) {
    if (!trader || !trader->pMdSpi || !trader->pMdApi) return -1;
    if (!trader->pMdSpi->isLoggedIn) {
        trader->pMdSpi->UpdateStatus("行情未登录，无法订阅");
        return -1;
    }
    std::vector<std::string> insts;
    SplitInstrumentsCsv(instrumentsCsv, insts);
    if (insts.empty()) {
        trader->pMdSpi->UpdateStatus("请输入要订阅的合约代码(支持逗号/空格分隔)");
        return -1;
    }
    std::vector<char*> ptrs;
    ptrs.reserve(insts.size());
    for (size_t i = 0; i < insts.size(); ++i) {
        ptrs.push_back(const_cast<char*>(insts[i].c_str()));
    }
    int ret = trader->pMdApi->SubscribeMarketData(ptrs.data(), (int)ptrs.size());
    if (ret == 0) trader->pMdSpi->UpdateStatus("订阅请求已发送");
    else trader->pMdSpi->UpdateStatus("订阅请求发送失败");
    return ret;
}

extern "C" int UnsubscribeMarketData(CTPTrader* trader, const char* instrumentsCsv) {
    if (!trader || !trader->pMdSpi || !trader->pMdApi) return -1;
    if (!trader->pMdSpi->isLoggedIn) {
        trader->pMdSpi->UpdateStatus("行情未登录，无法退订");
        return -1;
    }
    std::vector<std::string> insts;
    SplitInstrumentsCsv(instrumentsCsv, insts);
    if (insts.empty()) {
        trader->pMdSpi->UpdateStatus("请输入要退订的合约代码(支持逗号/空格分隔)");
        return -1;
    }
    std::vector<char*> ptrs;
    ptrs.reserve(insts.size());
    for (size_t i = 0; i < insts.size(); ++i) {
        ptrs.push_back(const_cast<char*>(insts[i].c_str()));
    }
    int ret = trader->pMdApi->UnSubscribeMarketData(ptrs.data(), (int)ptrs.size());
    if (ret == 0) trader->pMdSpi->UpdateStatus("退订请求已发送");
    else trader->pMdSpi->UpdateStatus("退订请求发送失败");
    return ret;
}

extern "C" int SendOrder(CTPTrader* trader, const char* instrumentID, char direction, 
                         char offsetFlag, double price, int volume) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    if (!trader->pSpi->isLoggedIn) return -2;
    
    CThostFtdcInputOrderField req = {0};
    strcpy(req.BrokerID, trader->pSpi->brokerID);
    strcpy(req.InvestorID, trader->pSpi->userID);
    strcpy(req.InstrumentID, instrumentID);
    
    // 生成报单引用
    static int orderRef = 0;
    sprintf(req.OrderRef, "%d", ++orderRef);
    
    req.Direction = direction;  // '0'=买, '1'=卖
    req.CombOffsetFlag[0] = offsetFlag;  // '0'=开仓, '1'=平仓, '3'=平今, '4'=平昨
    req.CombHedgeFlag[0] = '1';  // 投机
    req.LimitPrice = price;
    req.VolumeTotalOriginal = volume;
    
    // 限价单
    req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
    req.TimeCondition = THOST_FTDC_TC_GFD;  // 当日有效
    req.VolumeCondition = THOST_FTDC_VC_AV;  // 任意数量
    req.MinVolume = 1;
    req.ContingentCondition = THOST_FTDC_CC_Immediately;  // 立即
    req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;  // 非强平
    req.IsAutoSuspend = 0;
    req.UserForceClose = 0;
    
    char logMsg[512];
    sprintf(logMsg, "SendOrder: %s, Direction=%c, Offset=%c, Price=%.2f, Volume=%d",
            instrumentID, direction, offsetFlag, price, volume);
    LogMessage(logMsg);
    
    int ret = trader->pSpi->pUserApi->ReqOrderInsert(&req, ++trader->pSpi->requestID);
    
    sprintf(logMsg, "ReqOrderInsert returned: %d", ret);
    LogMessage(logMsg);
    
    return ret;
}

extern "C" int CancelOrder(CTPTrader* trader, const char* orderRef, const char* exchangeID, 
                           const char* orderSysID) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    if (!trader->pSpi->isLoggedIn) return -2;
    
    CThostFtdcInputOrderActionField req = {0};
    strcpy(req.BrokerID, trader->pSpi->brokerID);
    strcpy(req.InvestorID, trader->pSpi->userID);
    
    if (orderRef && strlen(orderRef) > 0) {
        strcpy(req.OrderRef, orderRef);
    }
    if (exchangeID && strlen(exchangeID) > 0) {
        strcpy(req.ExchangeID, exchangeID);
    }
    if (orderSysID && strlen(orderSysID) > 0) {
        strcpy(req.OrderSysID, orderSysID);
    }
    
    req.ActionFlag = THOST_FTDC_AF_Delete;  // 删除
    
    char logMsg[256];
    sprintf(logMsg, "CancelOrder: OrderRef=%s, Exchange=%s, OrderSysID=%s",
            orderRef ? orderRef : "", exchangeID ? exchangeID : "", orderSysID ? orderSysID : "");
    LogMessage(logMsg);
    
    int ret = trader->pSpi->pUserApi->ReqOrderAction(&req, ++trader->pSpi->requestID);
    
    sprintf(logMsg, "ReqOrderAction returned: %d", ret);
    LogMessage(logMsg);
    
    return ret;
}




