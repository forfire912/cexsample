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
#include <map>
#include <set>
#include <ctype.h>
#include <stdarg.h>

#ifndef IDC_LISTVIEW_QUERY
#define IDC_LISTVIEW_QUERY 1106
#endif

static int g_queryMaxRecords = 200;
static int g_listViewLogCount = 0;

static void SafeFormat(char* buf, size_t size, const char* fmt, ...) {
    if (!buf || size == 0) return;
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, size, _TRUNCATE, fmt, ap);
    va_end(ap);
}

static void MaskId(const char* id, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    if (!id || !id[0]) {
        out[0] = '\0';
        return;
    }
    size_t len = strlen(id);
    if (len <= 2) {
        SafeFormat(out, outSize, "**");
        return;
    }
    const char* tail = id + (len - 2);
    SafeFormat(out, outSize, "***%s", tail);
}

static ListViewOp* AllocListViewOp(int op, HWND hListView) {
    ListViewOp* item = (ListViewOp*)malloc(sizeof(ListViewOp));
    if (!item) return NULL;
    memset(item, 0, sizeof(ListViewOp));
    item->op = op;
    item->hListView = hListView;
    return item;
}

static int ClampQueryMaxRecords(int value) {
    if (value < 1) return 1;
    if (value > 10000) return 10000;
    return value;
}

extern "C" void SetQueryMaxRecords(int maxRecords) {
    g_queryMaxRecords = ClampQueryMaxRecords(maxRecords);
}

static int GetQueryMaxRecords() {
    return g_queryMaxRecords;
}

static void NormalizeInstrumentId(std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') {
            s[i] = (char)tolower(c);
        }
    }
}

// Helper function: 将 GBK 编码转换为 UTF-8
// CTP API 返回的 ErrorMsg 是 GBK 编码，需要转换为 UTF-8
// 编码转换：将 CTP 返回的 GBK 字符串转换为 UTF-8
char* GbkToUtf8(const char* gbkStr) {
    if (!gbkStr) return NULL;
    
    // GBK 转 UNICODE
    int wlen = MultiByteToWideChar(CP_ACP, 0, gbkStr, -1, NULL, 0);
    // 转换失败则返回原字符串副本
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
// 兼容旧接口：保持函数名但内部走统一转换
char* Utf8ToGbk(const char* str) {
    return GbkToUtf8(str);
}

// 格式化辅助：char -> C 字符串
static void FormatChar(char* buf, char v) {
    if (!buf) return;
    if (v == 0) {
        buf[0] = '\0';
        return;
    }
    buf[0] = v;
    buf[1] = '\0';
}

// 格式化辅助：64 位整数 -> 字符串
static void FormatInt64(char* buf, long long v) {
    if (!buf) return;
    sprintf(buf, "%lld", v);
}

// 格式化辅助：浮点数 -> 字符串（处理 NaN/无效值）
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

// 前置声明（用于 CSV 流式导出）
static std::wstring Utf8ToWide(const char* s);
static void EnsureDir(const char* path);
static void GetNowYYYYMMDDHHMMSS(char* buf, size_t len);

// 安全拷贝：固定长度字段 -> C 字符串
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


// CSV 转义：处理逗号/引号/换行
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

// 追加写入 CSV 表头/行（UTF-8）
static void WriteCsvHeader(FILE* f, const std::vector<std::string>& headers) {
    if (!f || headers.empty()) return;
    std::string line;
    for (size_t i = 0; i < headers.size(); ++i) {
        if (i) line.append(",");
        line.append(CsvEscape(headers[i]));
    }
    line.append("\n");
    fwrite(line.data(), 1, line.size(), f);
}

static void WriteCsvRow(FILE* f, const std::vector<std::string>& row) {
    if (!f) return;
    std::string line;
    for (size_t i = 0; i < row.size(); ++i) {
        if (i) line.append(",");
        line.append(CsvEscape(row[i]));
    }
    line.append("\n");
    fwrite(line.data(), 1, line.size(), f);
}

static FILE* OpenCsvStreamUtf8(const char* userID, const char* contentName, char* outPath, size_t outSize) {
    if (!userID || !contentName || !outPath || outSize == 0) return NULL;
    EnsureDir("export");
    char datetimeStr[15];
    GetNowYYYYMMDDHHMMSS(datetimeStr, sizeof(datetimeStr));
    SafeFormat(outPath, outSize, "export\\%s_%s_%s.csv", userID, contentName, datetimeStr);
    std::wstring wpath = Utf8ToWide(outPath);
    FILE* f = _wfopen(wpath.c_str(), L"wb");
    if (!f) return NULL;
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, 1, 3, f);
    return f;
}

static void WriteInstrumentRow(FILE* f, const CThostFtdcInstrumentField* p, const char* nameUtf8) {
    if (!f || !p) return;
    std::vector<std::string> row;
    char buf[128];
    row.push_back(p->InstrumentID);
    row.push_back(nameUtf8 ? nameUtf8 : "");
    row.push_back(p->ExchangeID);
    row.push_back(p->ExchangeInstID);
    row.push_back(p->ProductID);
    FormatChar(buf, p->ProductClass);
    row.push_back(buf);
    SafeFormat(buf, sizeof(buf), "%d", p->DeliveryYear);
    row.push_back(buf);
    SafeFormat(buf, sizeof(buf), "%d", p->DeliveryMonth);
    row.push_back(buf);
    SafeFormat(buf, sizeof(buf), "%d", p->MaxMarketOrderVolume);
    row.push_back(buf);
    SafeFormat(buf, sizeof(buf), "%d", p->MinMarketOrderVolume);
    row.push_back(buf);
    SafeFormat(buf, sizeof(buf), "%d", p->MaxLimitOrderVolume);
    row.push_back(buf);
    SafeFormat(buf, sizeof(buf), "%d", p->MinLimitOrderVolume);
    row.push_back(buf);
    SafeFormat(buf, sizeof(buf), "%d", p->VolumeMultiple);
    row.push_back(buf);
    SafeFormat(buf, sizeof(buf), "%.4f", p->PriceTick);
    row.push_back(buf);
    row.push_back(p->CreateDate);
    row.push_back(p->OpenDate);
    row.push_back(p->ExpireDate);
    row.push_back(p->StartDelivDate);
    row.push_back(p->EndDelivDate);
    FormatChar(buf, p->InstLifePhase);
    row.push_back(buf);
    FormatChar(buf, p->IsTrading);
    row.push_back(buf);
    FormatChar(buf, p->PositionType);
    row.push_back(buf);
    FormatChar(buf, p->PositionDateType);
    row.push_back(buf);
    FormatDouble(buf, p->LongMarginRatio, 6);
    row.push_back(buf);
    FormatDouble(buf, p->ShortMarginRatio, 6);
    row.push_back(buf);
    FormatChar(buf, p->MaxMarginSideAlgorithm);
    row.push_back(buf);
    FormatDouble(buf, p->StrikePrice, 2);
    row.push_back(buf);
    FormatChar(buf, p->OptionsType);
    row.push_back(buf);
    FormatDouble(buf, p->UnderlyingMultiple, 2);
    row.push_back(buf);
    FormatChar(buf, p->CombinationType);
    row.push_back(buf);
    row.push_back(p->UnderlyingInstrID);
    WriteCsvRow(f, row);
}

// 获取当前日期（YYYYMMDD）
static void GetTodayYYYYMMDD(char* buf, size_t len) {
    if (!buf || len < 9) return;
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    SafeFormat(buf, len, "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

// 获取当前时间（YYYYMMDDHHMMSS）
static void GetNowYYYYMMDDHHMMSS(char* buf, size_t len) {
    if (!buf || len < 15) return;
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    SafeFormat(buf, len, "%04d%02d%02d%02d%02d%02d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
}

static void EnsureDir(const char* path) {
    if (!path || !path[0]) return;
    _mkdir(path);
}

// 校验日期字符串是否是8位数字 (YYYYMMDD)
// 校验日期字符串（YYYYMMDD）是否合法
static bool IsValidDate8(const char* s) {
    if (!s) return false;
    for (int i = 0; i < 8; ++i) {
        if (s[i] == '\0') return false;
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return s[8] == '\0' || s[8] == '\r' || s[8] == '\n';
}

// UTF-8 转宽字符串（用于 Windows 宽接口）
static std::wstring Utf8ToWide(const char* s) {
    if (!s) return std::wstring();
// 按 UTF-8 转换
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (wlen <= 0) return std::wstring();
    std::wstring ws;
    ws.resize(wlen - 1);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &ws[0], wlen);
    return ws;
}

// 导出 CSV：写入 BOM + 表头 + 数据行
static void ExportCsv(const char* userID, const char* contentName, const char* dateStr,
                      const std::vector<std::string>& headers,
                      const std::vector< std::vector<std::string> >& rows) {
    if (!userID || !contentName || !dateStr) return;
    if (headers.empty()) return;
    EnsureDir("export");
    char nameUtf8[260];
    SafeFormat(nameUtf8, sizeof(nameUtf8), "export\\%s_%s_%s.csv", userID, contentName, dateStr);
    std::wstring wpath = Utf8ToWide(nameUtf8);
    // 使用宽路径打开文件（UTF-8 转宽字符）
    FILE* f = _wfopen(wpath.c_str(), L"wb");
    if (!f) return;
    // UTF-8 BOM for Excel compatibility
    // 写入 UTF-8 BOM，提升 Excel 兼容性
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

// 写入调试日志到 ctp_debug.log
void LogMessage(const char* msg) {
    static char logPath[MAX_PATH] = {0};
    if (logPath[0] == '\0') {
        char modulePath[MAX_PATH] = {0};
        if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) > 0) {
            char* lastSlash = strrchr(modulePath, '\\');
            if (lastSlash) {
                *lastSlash = '\0';
                SafeFormat(logPath, sizeof(logPath), "%s\\ctp_debug.log", modulePath);
            }
        }
        if (logPath[0] == '\0') {
            strcpy(logPath, "ctp_debug.log");
        }
    }
    char timeStr[64];
    time_t now = time(NULL);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));

    FILE* f = fopen(logPath, "a");
    if (f) {
        fprintf(f, "[%s] %s\n", timeStr, msg);
        fclose(f);
    }

    // 兜底：写到当前目录
    FILE* f2 = fopen("ctp_debug.log", "a");
    if (f2) {
        fprintf(f2, "[%s] %s\n", timeStr, msg);
        fclose(f2);
    }

    // 兜底：写到临时目录，便于排查日志路径问题
    char tempPath[MAX_PATH] = {0};
    if (GetTempPathA(MAX_PATH, tempPath) > 0) {
        char tempLog[MAX_PATH] = {0};
        SafeFormat(tempLog, sizeof(tempLog), "%sctp_debug.log", tempPath);
        FILE* f3 = fopen(tempLog, "a");
        if (f3) {
            fprintf(f3, "[%s] %s\n", timeStr, msg);
            fclose(f3);
        }
    }
}

// 交易 SPI：处理交易通道回调与查询逻辑
class TraderSpi : public CThostFtdcTraderSpi {
public:
    // 交易 API 句柄与 UI/状态回调
    CThostFtdcTraderApi* pUserApi;
    HWND hListView;
    HWND hMainWnd;
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
    bool pauseReconnect;
    ULONGLONG lastDisconnectTick;
    int disconnectBurst;
    CRITICAL_SECTION queryLock;
    CRITICAL_SECTION listViewLock;
    
    // 导出缓存：委托/持仓/行情/合约
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
    ULONGLONG lastMarketQueryTick;
    std::map<std::string, ULONGLONG> marketRetryWaitTick;
    bool marketBatchCleared;
    std::map<int, std::string> marketReqMap;
    int marketRowIndex;
    bool marketQueryAll;
    std::set<std::string> marketQueryExpected;
    std::set<std::string> marketQueryReceived;
    int marketRespLogCount;
    std::map<std::string, int> marketRetryCount;
    bool marketThrottleNotice;
    ULONGLONG lastMarketStatusTick;
    int marketStatusCounter;
    std::map<std::string, int> marketStatus; // 0=pending 1=sent 2=ok 3=failed
    std::string currentReqInst;
    ULONGLONG currentReqSendTick;
    FILE* marketStreamFile;
    FILE* instrumentStreamFile;
    int instrumentStreamRows;
    
    // 初始化 SPI 状态与同步对象
    TraderSpi() {
        pUserApi = NULL;
        hListView = NULL;
        hMainWnd = NULL;
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
        pauseReconnect = false;
        lastDisconnectTick = 0;
        disconnectBurst = 0;
        marketQueryIndex = 0;
        marketBatchActive = false;
        marketBatchStartRequestID = 0;
        lastMarketQueryTick = 0;
        marketBatchCleared = false;
        marketReqMap.clear();
        marketRowIndex = 0;
        marketQueryAll = false;
        marketQueryExpected.clear();
        marketQueryReceived.clear();
        marketRespLogCount = 0;
        marketRetryCount.clear();
        marketRetryWaitTick.clear();
        marketThrottleNotice = false;
        lastMarketStatusTick = 0;
        marketStatusCounter = 0;
        marketStatus.clear();
        currentReqInst.clear();
        currentReqSendTick = 0;
        marketStreamFile = NULL;
        instrumentStreamFile = NULL;
        instrumentStreamRows = 0;
        marketStatus.clear();
        currentReqInst.clear();
        currentReqSendTick = 0;
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
    
    // 统一状态上报（UTF-8 透传到 UI）
    // 状态回调输出到 UI
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
        if (marketStreamFile) {
            fclose(marketStreamFile);
            marketStreamFile = NULL;
        }
        marketQueryQueue.clear();
        marketQueryIndex = 0;
        marketBatchActive = false;
        marketBatchStartRequestID = 0;
        lastMarketQueryTick = 0;
        marketBatchCleared = false;
        marketReqMap.clear();
        marketRowIndex = 0;
        marketQueryAll = false;
        marketQueryExpected.clear();
        marketQueryReceived.clear();
        marketRespLogCount = 0;
        marketRetryCount.clear();
        marketThrottleNotice = false;
        lastMarketStatusTick = 0;
        marketStatusCounter = 0;
        marketStatus.clear();
        currentReqInst.clear();
        currentReqSendTick = 0;
        lastMarketQueryTick = 0;
    }

    bool HasPendingMarketQuery() const {
        if (!marketBatchActive) return false;
        // Prefer queue as source of truth; status map can be out of sync on retries.
        for (size_t i = 0; i < marketQueryQueue.size(); ++i) {
            const std::string& cand = marketQueryQueue[i];
            std::map<std::string,int>::const_iterator it = marketStatus.find(cand);
            int st = (it == marketStatus.end()) ? 0 : it->second;
            if (st == 0 || st == 1) return true;
        }
        return false;
    }

    bool IsMarketQueryActive() const {
        return marketBatchActive;
    }

    void TrimListViewToMax(int maxRows) {
        if (!hListView && hMainWnd) {
            hListView = GetDlgItem(hMainWnd, IDC_LISTVIEW_QUERY);
        }
        HWND target = hListView;
        if (!hMainWnd && target) {
            hMainWnd = GetAncestor(target, GA_ROOT);
        }
        HWND host = hMainWnd;
        if (!target || !host) return;
        ListViewOp* op = AllocListViewOp(LV_OP_TRIM_TO_MAX, target);
        if (!op) return;
        op->row = maxRows;
        if (!PostMessage(host, WM_APP_LISTVIEW_OP, 0, (LPARAM)op)) {
            free(op);
        }
    }

    int SendNextMarketQuery() {
        if (!pUserApi) return -1;
        // 找到下一个 pending 合约
        size_t total = marketQueryQueue.size();
        size_t currentIndex = 0;
        std::string inst;
        bool found = false;
        ULONGLONG now = GetTickCount64();
        ULONGLONG earliestWait = 0;
        for (size_t i = 0; i < marketQueryQueue.size(); ++i) {
            const std::string& cand = marketQueryQueue[i];
            int st = 0;
            std::map<std::string,int>::iterator sit = marketStatus.find(cand);
            if (sit != marketStatus.end()) st = sit->second;
            if (st != 0) continue; // only pending
            ULONGLONG waitUntil = 0;
            std::map<std::string, ULONGLONG>::iterator wit = marketRetryWaitTick.find(cand);
            if (wit != marketRetryWaitTick.end()) waitUntil = wit->second;
            if (waitUntil != 0 && waitUntil > now) {
                if (earliestWait == 0 || waitUntil < earliestWait) earliestWait = waitUntil;
                continue; // not ready yet
            }
            inst = cand;
            currentIndex = i;
            found = true;
            break;
        }
        if (!found) {
            if (earliestWait != 0) {
                char msg[128];
                SafeFormat(msg, sizeof(msg), "行情查询等待退避窗口，下一次可发送于+%u ms", (unsigned)(earliestWait - now));
                LogMessage(msg);
            }
            return 0;
        }

        const int kMinIntervalMs = 500;
        if (lastMarketQueryTick != 0) {
            ULONGLONG delta = now - lastMarketQueryTick;
            if (delta < (ULONGLONG)kMinIntervalMs) {
                // 不要在 UI 线程 sleep；用退避窗口延后重试
                marketRetryWaitTick[inst] = now + ((ULONGLONG)kMinIntervalMs - delta);
                return 0;
            }
        }
        marketRetryWaitTick.erase(inst);
        CThostFtdcQryDepthMarketDataField req = {0};
        if (!inst.empty()) {
            strncpy(req.InstrumentID, inst.c_str(), sizeof(req.InstrumentID) - 1);
            req.InstrumentID[sizeof(req.InstrumentID) - 1] = '\0';
        }
        int nextRequestID = requestID + 1;
        if (marketBatchStartRequestID == 0) marketBatchStartRequestID = nextRequestID;
        {
            ULONGLONG nowStatus = GetTickCount64();
            if ((nowStatus - lastMarketStatusTick) > 600 || (marketStatusCounter++ % 3) == 0) {
                char msg[256];
                size_t sent = currentIndex + 1;
                size_t totalCount = total;
                if (inst.empty()) {
                    SafeFormat(msg, sizeof(msg), "正在发送行情查询(全部)");
                } else {
                    SafeFormat(msg, sizeof(msg), "正在发送行情查询(%Iu/%Iu): %s", (size_t)sent, (size_t)totalCount, inst.c_str());
                }
                UpdateStatus(msg);
                lastMarketStatusTick = nowStatus;
            }
        }
        int reqId = ++requestID;
        marketReqMap[reqId] = inst;
        {
            char msg[256];
            SafeFormat(msg, sizeof(msg), "发送行情查询 req=%d, inst=%s", reqId, inst.empty() ? "全部" : inst.c_str());
            LogMessage(msg);
        }
        lastMarketQueryTick = GetTickCount64();
        int ret = pUserApi->ReqQryDepthMarketData(&req, reqId);
        if (ret != 0) {
            // -2/-3 通常是请求频率或流量受限，稍后重试当前合约
            if (ret == -2 || ret == -3) {
                int& retry = marketRetryCount[inst];
                retry++;
                // 指数退避，避免一直触发 -3（流控）
                ULONGLONG waitMs = 2000;
                int exp = retry - 1;
                if (exp < 0) exp = 0;
                if (exp > 4) exp = 4;
                waitMs = waitMs << exp; // 2s,4s,8s,16s,32s
                if (waitMs > 30000) waitMs = 30000;
                marketRetryWaitTick[inst] = GetTickCount64() + waitMs;
                if (!marketThrottleNotice) {
                    char msg[256];
                    if (inst.empty()) {
                        SafeFormat(msg, sizeof(msg), "行情查询过于频繁，稍后自动重试(全部)，错误码=%d", ret);
                    } else {
                        SafeFormat(msg, sizeof(msg), "行情查询过于频繁，稍后自动重试(%s)，错误码=%d", inst.c_str(), ret);
                    }
                    UpdateStatus(msg);
                    LogMessage(msg);
                    marketThrottleNotice = true;
                }
                if (retry > 5) {
                    // 重试过多则跳过该合约
                    marketRetryCount.erase(inst);
                    marketStatus[inst] = 3;
                    marketRetryWaitTick.erase(inst);
                    currentReqInst.clear();
                    currentReqSendTick = 0;
                    if (HasPendingMarketQuery()) return SendNextMarketQuery();
                    return 0;
                }
                return 0;
            }
            {
                char msg[256];
                if (inst.empty()) {
                    SafeFormat(msg, sizeof(msg), "行情查询请求发送失败，合约=全部，错误码=%d", ret);
                } else {
                    SafeFormat(msg, sizeof(msg), "行情查询请求发送失败，合约=%s，错误码=%d", inst.c_str(), ret);
                }
                UpdateStatus(msg);
                LogMessage(msg);
            }
            marketStatus[inst] = 3;
            marketRetryWaitTick.erase(inst);
            currentReqInst.clear();
            currentReqSendTick = 0;
            if (HasPendingMarketQuery()) return SendNextMarketQuery();
            return 0;
        }
        // 发送成功后推进索引
        marketRetryCount.erase(inst);
        marketThrottleNotice = false;
        marketStatus[inst] = 1;
        currentReqInst = inst;
        currentReqSendTick = GetTickCount64();
        return 0;
    }

    void FinishMarketBatch() {
        marketBatchActive = false;
        currentReqInst.clear();
        currentReqSendTick = 0;
        size_t expected = marketQueryExpected.size();
        size_t received = marketQueryReceived.size();
        size_t failed = 0;
        for (std::map<std::string,int>::const_iterator it = marketStatus.begin(); it != marketStatus.end(); ++it) {
            if (it->second != 2) failed++;
        }
        if (marketQueryAll) {
            UpdateStatus("行情查询完成");
            LogMessage("行情查询完成");
        } else {
            char msg[256];
            SafeFormat(msg, sizeof(msg), "行情查询完成，期望=%u，成功=%u，失败/缺失=%u", (unsigned)expected, (unsigned)received, (unsigned)failed);
            UpdateStatus(msg);
            LogMessage(msg);
        }
        if (!marketStreamFile) {
            ExportMarketIfReady();
        } else {
            fclose(marketStreamFile);
            marketStreamFile = NULL;
        }
        // 仅保留配置条数显示（默认 200）
        TrimListViewToMax(GetQueryMaxRecords());
        ClearMarketQueryQueue();
        EndQuery(3);
    }

    int StartMarketQueryBatch(const std::vector<std::string>& instruments) {
        if (instruments.empty()) return -1;
        if (!BeginQuery(3)) {
            UpdateStatus("行情查询进行中，请稍后重试");
            return -3;
        }
        marketQueryAll = (instruments.size() == 1 && instruments[0].empty());
        marketQueryExpected.clear();
        marketQueryReceived.clear();
        if (!marketQueryAll) {
            for (size_t i = 0; i < instruments.size(); ++i) {
                if (instruments[i].empty()) continue;
                std::string norm = instruments[i];
                NormalizeInstrumentId(norm);
                marketQueryExpected.insert(norm);
            }
            {
                char msg[256];
                std::string summary;
                size_t shown = 0;
                for (size_t i = 0; i < instruments.size() && shown < 4; ++i) {
                    if (instruments[i].empty()) continue;
                    if (!summary.empty()) summary += ", ";
                    summary += instruments[i];
                    shown++;
                }
                if (marketQueryExpected.size() > shown) summary += " ...";
                SafeFormat(msg, sizeof(msg), "行情查询列表: %s (共%u)", summary.c_str(), (unsigned)marketQueryExpected.size());
                UpdateStatus(msg);
                LogMessage(msg);
            }
        } else {
            UpdateStatus("行情查询列表: 全部");
            LogMessage("行情查询列表: 全部");
        }
        marketQueryQueue = instruments;
        marketBatchActive = true;
        marketBatchStartRequestID = 0;
        // 点击查询时先清空列表，列头在回调里统一初始化
        ClearListView();
        marketBatchCleared = false;
        marketRowIndex = 0;
        lastMarketQueryTick = 0;
        marketRetryWaitTick.clear();
        marketStatus.clear();
        for (size_t i = 0; i < marketQueryQueue.size(); ++i) {
            marketStatus[marketQueryQueue[i]] = 0;
        }
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
    
    // 若有委托数据则导出 CSV
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
    
    // 若有合约/期权数据则导出 CSV
    void ExportInstrumentIfReady(bool isOption) {
        if (instrumentHeaders.empty()) return;
        char datetimeStr[15];
        GetNowYYYYMMDDHHMMSS(datetimeStr, sizeof(datetimeStr));
        const char* contentName = isOption ? "期权" : "合约";
        ExportCsv(userID, contentName, datetimeStr, instrumentHeaders, instrumentRows);
    }
    
    // 清空 ListView 内容与列头
    void ClearListView() {
        if (!hListView && hMainWnd) {
            hListView = GetDlgItem(hMainWnd, IDC_LISTVIEW_QUERY);
        }
        HWND target = hListView;
        if (!hMainWnd && target) {
            hMainWnd = GetAncestor(target, GA_ROOT);
        }
        HWND host = hMainWnd;
        if (!target || !host) {
            if (g_listViewLogCount < 5) {
                LogMessage("ListView Clear skipped: missing target/host");
                g_listViewLogCount++;
            }
            return;
        }
        ListViewOp* op = AllocListViewOp(LV_OP_CLEAR, target);
        if (!op) return;
        if (!PostMessage(host, WM_APP_LISTVIEW_OP, 0, (LPARAM)op)) {
            free(op);
            if (g_listViewLogCount < 5) {
                LogMessage("ListView Clear PostMessage failed");
                g_listViewLogCount++;
            }
        }
    }

    // 统一的列头添加函数，直接使用宽字符串和 SendMessage
    // 清空 ListView 内容与列头
    void AddColumn(int col, const WCHAR* text, int width) {
        if (!hListView && hMainWnd) {
            hListView = GetDlgItem(hMainWnd, IDC_LISTVIEW_QUERY);
        }
        HWND target = hListView;
        if (!hMainWnd && target) {
            hMainWnd = GetAncestor(target, GA_ROOT);
        }
        HWND host = hMainWnd;
        if (!target || !host || !text) {
            if (g_listViewLogCount < 5) {
                LogMessage("ListView AddColumn skipped: missing target/host/text");
                g_listViewLogCount++;
            }
            return;
        }
        ListViewOp* op = AllocListViewOp(LV_OP_ADD_COLUMN, target);
        if (!op) return;
        op->col = col;
        op->width = width;
        size_t len = wcslen(text) + 1;
        op->wtext = (WCHAR*)malloc(len * sizeof(WCHAR));
        if (!op->wtext) {
            free(op);
            return;
        }
        memcpy(op->wtext, text, len * sizeof(WCHAR));
        if (!PostMessage(host, WM_APP_LISTVIEW_OP, 0, (LPARAM)op)) {
            free(op->wtext);
            free(op);
            if (g_listViewLogCount < 5) {
                LogMessage("ListView AddColumn PostMessage failed");
                g_listViewLogCount++;
            }
        }
    }
    
    // 统一添加列表项的函数
    // 输入的文本应该是 UTF-8 编码（源文件和 CTP 消息都已转为 UTF-8）
    // 添加/更新 ListView 单元格（UTF-8->宽字符）
    void AddItem(int row, int col, const char* text) {
        if (!hListView && hMainWnd) {
            hListView = GetDlgItem(hMainWnd, IDC_LISTVIEW_QUERY);
        }
        HWND target = hListView;
        if (!hMainWnd && target) {
            hMainWnd = GetAncestor(target, GA_ROOT);
        }
        HWND host = hMainWnd;
        if (!target || !host) {
            if (g_listViewLogCount < 5) {
                LogMessage("ListView AddItem skipped: missing target/host");
                g_listViewLogCount++;
            }
            return;
        }
        const char* safeText = text ? text : "";
        ListViewOp* op = AllocListViewOp(LV_OP_ADD_ITEM, target);
        if (!op) return;
        op->row = row;
        op->col = col;
        size_t len = strlen(safeText) + 1;
        op->text = (char*)malloc(len);
        if (!op->text) {
            free(op);
            return;
        }
        memcpy(op->text, safeText, len);
        if (!PostMessage(host, WM_APP_LISTVIEW_OP, 0, (LPARAM)op)) {
            free(op->text);
            free(op);
            if (g_listViewLogCount < 5) {
                LogMessage("ListView AddItem PostMessage failed");
                g_listViewLogCount++;
            }
        }
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
        if (!pauseReconnect) {
            ULONGLONG now = GetTickCount64();
            if (lastDisconnectTick != 0 && (now - lastDisconnectTick) <= 3000) {
                disconnectBurst++;
            } else {
                disconnectBurst = 1;
            }
            lastDisconnectTick = now;
            if (disconnectBurst >= 3) {
                pauseReconnect = true;
                if (pUserApi) {
                    pUserApi->RegisterSpi(NULL);
                    pUserApi->Release();
                    pUserApi = NULL;
                }
                UpdateStatus("登录未成功且连接频繁断开，已暂停自动重连，请检查账号/前置后手动重连。");
            }
        }
        char msg[128];
        SafeFormat(msg, sizeof(msg), "OnFrontDisconnected: reason=%d", nReason);
        LogMessage(msg);
        SafeFormat(msg, sizeof(msg), "连接断开，原因代码: %d", nReason);
        UpdateStatus(msg);
    }
    
    virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, 
                                   CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            char msg[256];
            char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
            SafeFormat(msg, sizeof(msg), "认证失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
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
            SafeFormat(msg, sizeof(msg), "登录失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
            return;
        }
        isLoggedIn = true;
        char msg[256];
        SafeFormat(msg, sizeof(msg), "登录成功！交易日: %s", pRspUserLogin ? pRspUserLogin->TradingDay : "未知");
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
            SafeFormat(msg, sizeof(msg), "结算单确认失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
        } else {
            UpdateStatus("结算单确认成功，可以开始交易。");
        }
    }
    
    virtual void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    // 合约/期权查询响应：填充列表与导出缓存
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
        SafeFormat(msg, sizeof(msg), "查询委托失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        EndQuery(1);
        return;
    }
    int& row = marketRowIndex;
    int maxRecords = GetQueryMaxRecords();
    static int lastRequestID = 0;
    if (lastRequestID != nRequestID) {
        ClearListView();
        row = 0;
        int col = 0;
        AddColumn(col++, L"交易日", 80);
        AddColumn(col++, isOptionQuery ? L"期权合约" : L"合约", 90);
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
        headers.push_back(isOptionQuery ? "期权合约" : "合约");
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
    if (pOrder && row < maxRecords) {
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
        SafeFormat(msg, sizeof(msg), "查询持仓失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        EndQuery(2);
        return;
    }
    static int row = 0;
    int maxRecords = GetQueryMaxRecords();
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
    if (pInvestorPosition && row < maxRecords) {
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
        SafeFormat(msg, sizeof(msg), "查询行情失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        if (!currentReqInst.empty()) {
            marketStatus[currentReqInst] = 3;
            currentReqInst.clear();
            currentReqSendTick = 0;
        }
        if (HasPendingMarketQuery()) {
            int ret = SendNextMarketQuery();
            if (ret != 0) {
                ClearMarketQueryQueue();
                EndQuery(3);
            }
        } else {
            FinishMarketBatch();
        }
        return;
    }
    // 若批量已结束且状态已清空，忽略迟到回报，避免清空列表
    if (!marketBatchActive && marketBatchStartRequestID == 0) {
        if (marketRespLogCount < 10) {
            char msg[256];
            SafeFormat(msg, sizeof(msg), "忽略迟到行情回报 req=%d", nRequestID);
            LogMessage(msg);
            marketRespLogCount++;
        }
        return;
    }
    static int row = 0;
    int maxRecords = GetQueryMaxRecords();
    static int lastRequestID = 0;
    if (lastRequestID != nRequestID) {
        bool isBatchStart = (!marketBatchActive) || (nRequestID == marketBatchStartRequestID);
        if (isBatchStart && !marketBatchCleared) {
            ClearListView();
            marketBatchCleared = true;
            row = 0;
            int col = 0;
            AddColumn(col++, L"交易日", 80);
            AddColumn(col++, L"请求ID", 70);
            AddColumn(col++, L"查询合约", 90);
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
            headers.push_back("请求ID");
            headers.push_back("查询合约");
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
            // 行情查询：流式写入文件（所有返回写入）
            if (!marketStreamFile) {
                char path[260];
                marketStreamFile = OpenCsvStreamUtf8(userID, "行情", path, sizeof(path));
                if (marketStreamFile) {
                    WriteCsvHeader(marketStreamFile, headers);
                }
            }
        }
        lastRequestID = nRequestID;
    }
    bool acceptMarketRow = true;
    const char* expectedInst = "";
    if (pDepthMarketData) {
        std::map<int, std::string>::const_iterator it = marketReqMap.find(nRequestID);
        if (it == marketReqMap.end()) {
            acceptMarketRow = false;
        } else {
            const std::string& expect = it->second;
            if (expect.empty()) {
                expectedInst = "全部";
            } else {
                expectedInst = expect.c_str();
            }
            if (!expect.empty()) {
                char got[64];
                CopyFixedField(got, sizeof(got), pDepthMarketData->InstrumentID, sizeof(pDepthMarketData->InstrumentID));
                if (_stricmp(got, expect.c_str()) != 0) {
                    acceptMarketRow = false;
                    if (marketRespLogCount < 20) {
                        char msg[256];
                        SafeFormat(msg, sizeof(msg), "行情回报不匹配: req=%d, expect=%s, got=%s", nRequestID, expect.c_str(), got);
                        LogMessage(msg);
                        marketRespLogCount++;
                    }
                }
            }
        }
    }

    if (pDepthMarketData && acceptMarketRow && row < maxRecords) {
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

        if (!marketQueryAll) {
            std::string norm = instrumentID;
            NormalizeInstrumentId(norm);
            marketQueryReceived.insert(norm);
        }

        AddItem(row, col++, tradingDay);
        rowData.push_back(tradingDay);
        {
            char reqBuf[16];
            SafeFormat(reqBuf, sizeof(reqBuf), "%d", nRequestID);
            AddItem(row, col++, reqBuf);
            rowData.push_back(reqBuf);
        }
        AddItem(row, col++, expectedInst);
        rowData.push_back(expectedInst ? expectedInst : "");
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
        if (marketStreamFile) {
            int maxRows = GetQueryMaxRecords();
            if (maxRows < 1) maxRows = 1;
            if (marketRows.size() > (size_t)maxRows) {
                marketRows.erase(marketRows.begin());
            }
        }
        if (marketStreamFile) {
            WriteCsvRow(marketStreamFile, rowData);
        }
        row++;
    }
    if (bIsLast) {
        // 标记当前请求状态
        std::string inst;
        std::map<int, std::string>::const_iterator mit = marketReqMap.find(nRequestID);
        if (mit != marketReqMap.end()) inst = mit->second;
        {
            char msg[256];
            SafeFormat(msg, sizeof(msg), "行情响应结束 req=%d inst=%s accept=%d", nRequestID, inst.empty() ? "?" : inst.c_str(), acceptMarketRow ? 1 : 0);
            LogMessage(msg);
        }
        if (acceptMarketRow) {
            if (!inst.empty()) marketStatus[inst] = 2;
            else marketStatus[""] = 2;
        } else {
            if (!inst.empty()) marketStatus[inst] = 3;
            else marketStatus[""] = 3;
        }
        marketRetryWaitTick.erase(inst);
        if (!currentReqInst.empty() && currentReqInst == inst) {
            currentReqInst.clear();
            currentReqSendTick = 0;
        }
        if (HasPendingMarketQuery()) {
            int ret = SendNextMarketQuery();
            if (ret != 0) {
                ClearMarketQueryQueue();
                EndQuery(3);
            }
        } else {
            FinishMarketBatch();
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
        if (isOptionQuery) {
            SafeFormat(msg, sizeof(msg), "查询期权失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        } else {
            SafeFormat(msg, sizeof(msg), "查询合约失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        }
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
    const int MAX_INSTRUMENTS = 10000; // 支持更多期权
    int maxRecords = GetQueryMaxRecords();
    
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
        // 合约/期权查询：流式写入文件（所有返回写入）
        if (instrumentStreamFile) {
            fclose(instrumentStreamFile);
            instrumentStreamFile = NULL;
        }
        {
            char path[260];
            const char* contentName = isOptionQuery ? "期权" : "合约";
            instrumentStreamFile = OpenCsvStreamUtf8(userID, contentName, path, sizeof(path));
            if (instrumentStreamFile) {
                WriteCsvHeader(instrumentStreamFile, headers);
                instrumentStreamRows = 0;
            }
        }
    }
    
    if (pInstrument) {
        totalCount++;

        // 转换合约名称从 GBK 到 UTF-8（用于导出和展示）
        char* utf8Name = GbkToUtf8(pInstrument->InstrumentName);
        const char* nameUtf8 = utf8Name ? utf8Name : pInstrument->InstrumentName;

        // 合约/期权：流式写入时写入全部返回数据
        if (instrumentStreamFile) {
            WriteInstrumentRow(instrumentStreamFile, pInstrument, nameUtf8);
            instrumentStreamRows++;
            if ((instrumentStreamRows % 2000) == 0) {
                fflush(instrumentStreamFile);
            }
        }

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
                SafeFormat(today, sizeof(today), "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
                if (strlen(pInstrument->ExpireDate) > 0 && 
                    strcmp(pInstrument->ExpireDate, today) >= 0 &&
                    IsMainContract(pInstrument->InstrumentID)) {
                    accept = true;
                }
            }
        }

        if (accept) {
            // 保存到数组（用于界面展示）
            strcpy(instruments[instrumentCount].instrumentID, pInstrument->InstrumentID);
            if (nameUtf8) {
                strncpy(instruments[instrumentCount].instrumentName, nameUtf8, 60);
                instruments[instrumentCount].instrumentName[60] = '\0';
            } else {
                instruments[instrumentCount].instrumentName[0] = '\0';
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

        if (utf8Name) delete[] utf8Name;
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
        int maxDisplay = maxRecords < MAX_INSTRUMENTS ? maxRecords : MAX_INSTRUMENTS;
        int displayCount = instrumentCount < maxDisplay ? instrumentCount : maxDisplay;
        int maxRows = GetQueryMaxRecords();
        if (maxRows < 1) maxRows = 1;
        int startIndex = displayCount > maxRows ? (displayCount - maxRows) : 0;
        int uiRow = 0;
        for (int i = 0; i < displayCount; i++) {
            char buf[256];
            int col = 0;
            std::vector<std::string> rowData;
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].instrumentID);
            rowData.push_back(instruments[i].instrumentID);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].instrumentName);
            rowData.push_back(instruments[i].instrumentName);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].exchangeID);
            rowData.push_back(instruments[i].exchangeID);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].exchangeInstID);
            rowData.push_back(instruments[i].exchangeInstID);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].productID);
            rowData.push_back(instruments[i].productID);
            FormatChar(buf, instruments[i].productClass);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].deliveryYear);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].deliveryMonth);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].maxMarketOrderVolume);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].minMarketOrderVolume);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].maxLimitOrderVolume);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatInt64(buf, instruments[i].minLimitOrderVolume);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            SafeFormat(buf, sizeof(buf), "%d", instruments[i].volumeMultiple);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            SafeFormat(buf, sizeof(buf), "%.4f", instruments[i].priceTick);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].createDate);
            rowData.push_back(instruments[i].createDate);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].openDate);
            rowData.push_back(instruments[i].openDate);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].expireDate);
            rowData.push_back(instruments[i].expireDate);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].startDelivDate);
            rowData.push_back(instruments[i].startDelivDate);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].endDelivDate);
            rowData.push_back(instruments[i].endDelivDate);
            FormatChar(buf, instruments[i].instLifePhase);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].isTrading);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].positionType);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].positionDateType);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatDouble(buf, instruments[i].longMarginRatio, 6);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatDouble(buf, instruments[i].shortMarginRatio, 6);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].maxMarginSideAlgorithm);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatDouble(buf, instruments[i].strikePrice, 2);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].optionsType);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatDouble(buf, instruments[i].underlyingMultiple, 2);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            FormatChar(buf, instruments[i].combinationType);
            if (i >= startIndex) AddItem(uiRow, col++, buf);
            rowData.push_back(buf);
            if (i >= startIndex) AddItem(uiRow, col++, instruments[i].underlyingInstrID);
            rowData.push_back(instruments[i].underlyingInstrID);

            if (instrumentStreamFile) {
                if (i >= startIndex) {
                    instrumentRows.push_back(rowData);
                    if (instrumentRows.size() > (size_t)maxRows) {
                        instrumentRows.erase(instrumentRows.begin());
                    }
                    uiRow++;
                }
            } else {
                instrumentRows.push_back(rowData);
                if (i >= startIndex) {
                    uiRow++;
                }
            }
        }
        
        // 显示统计信息
        char msg[256];
        int shown = displayCount > maxRows ? maxRows : displayCount;
        if (isOptionQuery) {
            SafeFormat(msg, sizeof(msg), "期权查询完成，总合约数: %d，已显示: %d（按到期日排序）",
                    totalCount, shown);
        } else {
            SafeFormat(msg, sizeof(msg), "主力合约查询完成，总合约数: %d，主力合约: %d，已显示: %d（按到期日排序）",
                    totalCount, instrumentCount, shown);
        }
        UpdateStatus(msg);
        if (instrumentStreamFile) {
            fclose(instrumentStreamFile);
            instrumentStreamFile = NULL;
            if (instrumentStreamRows > 0) {
                char msgStream[256];
                SafeFormat(msgStream, sizeof(msgStream), "合约/期权导出写入行数: %d", instrumentStreamRows);
                LogMessage(msgStream);
            }
        } else {
            ExportInstrumentIfReady(isOptionQuery);
        }
        // 仅保留配置条数显示（默认 200）
        TrimListViewToMax(maxRows);
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
        SafeFormat(msg, sizeof(msg), "报单失败: ErrorID=%d, %s", pRspInfo->ErrorID, gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        LogMessage(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
    } else {
        char msg[256];
        SafeFormat(msg, sizeof(msg), "报单请求已提交: %s", pInputOrder ? pInputOrder->InstrumentID : "");
        UpdateStatus(msg);
        LogMessage(msg);
    }
}

// 撤单响应
void TraderSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[512];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        SafeFormat(msg, sizeof(msg), "撤单失败: ErrorID=%d, %s", pRspInfo->ErrorID, gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        LogMessage(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
    } else {
        char msg[256];
        SafeFormat(msg, sizeof(msg), "撤单请求已提交");
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
    SafeFormat(msg, sizeof(msg), "报单回报: %s %s %s, 价格:%.2f, 数量:%d, 状态:%s", 
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
    SafeFormat(msg, sizeof(msg), "成交回报: %s %s %s, 价格:%.2f, 数量:%d, 时间:%s", 
            pTrade->InstrumentID,
            pTrade->Direction == '0' ? "买" : "卖",
            pTrade->OffsetFlag == '0' ? "开仓" : "平仓",
            pTrade->Price,
            pTrade->Volume,
            pTrade->TradeTime);
    
    UpdateStatus(msg);
    LogMessage(msg);
}

// 报单录入错误回报
void TraderSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo) {
    if (pRspInfo) {
        char msg[512];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        SafeFormat(msg, sizeof(msg), "报单错误: ErrorID=%d, %s", pRspInfo->ErrorID, gbkErrorMsg ? gbkErrorMsg : "");
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
        SafeFormat(msg, sizeof(msg), "撤单错误: ErrorID=%d, %s", pRspInfo->ErrorID, gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        LogMessage(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
    }
}

class MdSpi : public CThostFtdcMdSpi {
public:
    // 行情 API 句柄与状态
    CThostFtdcMdApi* pMdApi;
    StatusCallback statusCallback;
    HWND hMainWnd;
    int requestID;
    bool isConnected;
    bool isLoggedIn;
    char brokerID[11];
    char userID[16];
    char password[41];
    std::vector<std::string> pendingSubs;
    int mdUpdateCount;
    int subExpected;
    int subAck;
    int subErr;
    int unsubExpected;
    int unsubAck;
    int unsubErr;

    // 初始化行情 SPI 状态
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
        pendingSubs.clear();
        mdUpdateCount = 0;
        subExpected = 0;
        subAck = 0;
        subErr = 0;
        unsubExpected = 0;
        unsubAck = 0;
        unsubErr = 0;
    }

    void UpdateStatus(const char* msg) {
        if (statusCallback) statusCallback(msg);
    }

    int SubscribeList(const std::vector<std::string>& insts) {
        if (!pMdApi) return -1;
        if (insts.empty()) return -1;
        subExpected = (int)insts.size();
        subAck = 0;
        subErr = 0;
        std::vector<char*> ptrs;
        ptrs.reserve(insts.size());
        for (size_t i = 0; i < insts.size(); ++i) {
            ptrs.push_back(const_cast<char*>(insts[i].c_str()));
        }
        int ret = pMdApi->SubscribeMarketData(ptrs.data(), (int)ptrs.size());
        char msg[256];
        SafeFormat(msg, sizeof(msg), "订阅请求发送%s，数量=%d，ret=%d", ret == 0 ? "成功" : "失败", (int)insts.size(), ret);
        UpdateStatus(msg);
        LogMessage(msg);
        return ret;
    }

    void StartUnsubscribeTracking(int count) {
        unsubExpected = count;
        unsubAck = 0;
        unsubErr = 0;
    }

    virtual void OnFrontConnected() {
        isConnected = true;
        LogMessage("行情前置连接成功，开始登录...");
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
            SafeFormat(msg, sizeof(msg), "行情登录失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            LogMessage(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
            return;
        }
        isLoggedIn = true;
        UpdateStatus("行情登录成功");
        LogMessage("行情登录成功");
        if (!pendingSubs.empty()) {
            SubscribeList(pendingSubs);
            pendingSubs.clear();
        }
    }

    virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument,
                                    CThostFtdcRspInfoField* pRspInfo,
                                    int nRequestID, bool bIsLast) {
        (void)nRequestID;
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            subErr++;
            char msg[256];
            char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
            SafeFormat(msg, sizeof(msg), "订阅回报失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            LogMessage(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
            return;
        }
        subAck++;
        if (pSpecificInstrument) {
            char msg[256];
            SafeFormat(msg, sizeof(msg), "订阅回报成功: %s%s", pSpecificInstrument->InstrumentID, bIsLast ? " (last)" : "");
            UpdateStatus(msg);
            LogMessage(msg);
        } else {
            char msg[128];
            SafeFormat(msg, sizeof(msg), "订阅回报成功: (null)%s", bIsLast ? " (last)" : "");
            UpdateStatus(msg);
            LogMessage(msg);
        }
        if (bIsLast) {
            char msg[256];
            SafeFormat(msg, sizeof(msg), "订阅回报汇总: 期望=%d, 成功=%d, 失败=%d", subExpected, subAck, subErr);
            UpdateStatus(msg);
            LogMessage(msg);
        }
    }

    virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument,
                                      CThostFtdcRspInfoField* pRspInfo,
                                      int nRequestID, bool bIsLast) {
        (void)nRequestID;
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            unsubErr++;
            char msg[256];
            char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
            SafeFormat(msg, sizeof(msg), "退订回报失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            LogMessage(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
            return;
        }
        unsubAck++;
        if (pSpecificInstrument) {
            char msg[256];
            SafeFormat(msg, sizeof(msg), "退订回报成功: %s%s", pSpecificInstrument->InstrumentID, bIsLast ? " (last)" : "");
            UpdateStatus(msg);
            LogMessage(msg);
        } else {
            char msg[128];
            SafeFormat(msg, sizeof(msg), "退订回报成功: (null)%s", bIsLast ? " (last)" : "");
            UpdateStatus(msg);
            LogMessage(msg);
        }
        if (bIsLast) {
            char msg[256];
            SafeFormat(msg, sizeof(msg), "退订回报汇总: 期望=%d, 成功=%d, 失败=%d", unsubExpected, unsubAck, unsubErr);
            UpdateStatus(msg);
            LogMessage(msg);
        }
    }

    virtual void OnRspError(CThostFtdcRspInfoField* pRspInfo,
                            int nRequestID, bool bIsLast) {
        (void)nRequestID;
        if (!bIsLast) return;
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            char msg[256];
            char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
            SafeFormat(msg, sizeof(msg), "行情错误回报: %s", gbkErrorMsg ? gbkErrorMsg : "");
            UpdateStatus(msg);
            LogMessage(msg);
            if (gbkErrorMsg) delete[] gbkErrorMsg;
        }
    }

    // 行情推送：通过消息投递到 UI 线程
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

        if (mdUpdateCount < 5) {
            char msg[256];
            char inst[64];
            CopyFixedField(inst, sizeof(inst), pDepthMarketData->InstrumentID, sizeof(pDepthMarketData->InstrumentID));
            SafeFormat(msg, sizeof(msg), "收到行情推送: %s", inst);
            LogMessage(msg);
            mdUpdateCount++;
        }

        PostMessage(hMainWnd, WM_APP_MD_UPDATE, 0, (LPARAM)u);
    }
};

// 解析合约列表（支持逗号/空格/换行/分号）
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

// 对外句柄：组合交易/行情 SPI 与 UI 句柄
struct CTPTrader {
    TraderSpi* pSpi;
    MdSpi* pMdSpi;
    CThostFtdcMdApi* pMdApi;
    HWND hMainWnd;
};

// 创建交易对象并初始化 SPI
extern "C" CTPTrader* CreateCTPTrader() {
    LogMessage("CreateCTPTrader called");
    {
        char buildMsg[128];
        SafeFormat(buildMsg, sizeof(buildMsg), "Build: %s %s", __DATE__, __TIME__);
        LogMessage(buildMsg);
    }
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
            // 解除 SPI 绑定
            trader->pMdApi->RegisterSpi(NULL);
            trader->pMdApi->Release();
            trader->pMdApi = NULL;
        }
        if (trader->pMdSpi) {
            delete trader->pMdSpi;
            trader->pMdSpi = NULL;
        }
        if (trader->pSpi && trader->pSpi->pUserApi) {
            // 解除 SPI 绑定
            trader->pSpi->pUserApi->RegisterSpi(NULL);
            trader->pSpi->pUserApi->Release();
            trader->pSpi->pUserApi = NULL;
        }
        if (trader->pSpi) { delete trader->pSpi; }
        delete trader;
    }
}

// 断开连接：释放 API 并重置状态
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
        trader->pSpi->ClearMarketQueryQueue();
        trader->pSpi->EndQuery(1);
        trader->pSpi->EndQuery(2);
        trader->pSpi->EndQuery(3);
        trader->pSpi->EndQuery(4);
        trader->pSpi->UpdateStatus("已断开连接");
    }
}


extern "C" void SetMainWindow(CTPTrader* trader, HWND hMainWnd) {
    if (!trader) return;
    trader->hMainWnd = hMainWnd;
    if (trader->pSpi) trader->pSpi->hMainWnd = hMainWnd;
    if (trader->pMdSpi) trader->pMdSpi->hMainWnd = hMainWnd;
}

extern "C" void SetListView(CTPTrader* trader, HWND hListView) {
    if (trader && trader->pSpi) {
        EnterCriticalSection(&trader->pSpi->listViewLock);
        trader->pSpi->hListView = hListView;
        if (!trader->pSpi->hMainWnd && hListView) {
            trader->pSpi->hMainWnd = GetAncestor(hListView, GA_ROOT);
        }
        LeaveCriticalSection(&trader->pSpi->listViewLock);
    }
}

// 设置状态回调（交易/行情共用）

extern "C" void CancelMarketQuery(CTPTrader* trader) {
    if (!trader || !trader->pSpi) return;
    trader->pSpi->ClearMarketQueryQueue();
    trader->pSpi->EndQuery(3);
    trader->pSpi->UpdateStatus("行情查询已取消，可重新查询");
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
    pSpi->pauseReconnect = false;
    pSpi->disconnectBurst = 0;
    pSpi->lastDisconnectTick = 0;

    {
        const char* ver = CThostFtdcTraderApi::GetApiVersion();
        char verMsg[128];
        SafeFormat(verMsg, sizeof(verMsg), "Trader API Version: %s", ver ? ver : "unknown");
        LogMessage(verMsg);
        pSpi->UpdateStatus(verMsg);
    }
    
    // Check if already initialized
    if (pSpi->pUserApi != NULL) {
        pSpi->UpdateStatus("已连接或正在连接中...");
        LogMessage("WARNING: Already connected");
        return -1;
    }
    
    // Create flow directory if not exists
    CreateDirectoryA("flow", NULL);
    LogMessage("flow directory created");
    
    // 保存登录参数到 SPI
    strncpy(pSpi->brokerID, brokerID, sizeof(pSpi->brokerID) - 1);
    strncpy(pSpi->userID, userID, sizeof(pSpi->userID) - 1);
    strncpy(pSpi->password, password, sizeof(pSpi->password) - 1);
    strncpy(pSpi->authCode, authCode, sizeof(pSpi->authCode) - 1);
    strncpy(pSpi->appID, appID, sizeof(pSpi->appID) - 1);
    
    char logMsg[256];
    char maskedUser[32];
    MaskId(userID, maskedUser, sizeof(maskedUser));
    SafeFormat(logMsg, sizeof(logMsg), "Credentials set: BrokerID=%s, UserID=%s, Front=%s", brokerID, maskedUser, frontAddr);
    LogMessage(logMsg);
    
    try {
        LogMessage("Creating API instance...");
        // 创建交易 API 实例
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
            // 启动 API，进入连接流程
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
    if (instrumentID && instrumentID[0]) {
        insts.push_back(instrumentID);
        return trader->pSpi->StartMarketQueryBatch(insts);
    }
    insts.push_back(std::string());
    trader->pSpi->UpdateStatus("正在查询全部行情...");
    return trader->pSpi->StartMarketQueryBatch(insts);
}

extern "C" int QueryMarketDataBatch(CTPTrader* trader, const char* instrumentsCsv) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    std::vector<std::string> insts;
    SplitInstrumentsCsv(instrumentsCsv, insts);
    if (!insts.empty()) {
        return trader->pSpi->StartMarketQueryBatch(insts);
    }
    insts.push_back(std::string());
    trader->pSpi->UpdateStatus("正在查询全部行情...");
    return trader->pSpi->StartMarketQueryBatch(insts);
}

extern "C" int MarketQueryTick(CTPTrader* trader) {
    if (!trader || !trader->pSpi) return 0;
    TraderSpi* pSpi = trader->pSpi;
    if (!pSpi->HasPendingMarketQuery()) return 0;
    ULONGLONG now = GetTickCount64();
    const ULONGLONG kTimeoutMs = 3000;
    // 尚无在途请求，直接发送下一条
    if (pSpi->currentReqInst.empty()) {
        LogMessage("MarketQueryTick: no in-flight, send next");
        pSpi->SendNextMarketQuery();
        return 1;
    }
    // 有在途请求，检测超时
    if (pSpi->currentReqSendTick != 0 && (now - pSpi->currentReqSendTick) < kTimeoutMs) {
        return 0;
    }
    // 超时：标记失败并继续下一条
    std::string inst = pSpi->currentReqInst;
    pSpi->marketStatus[inst] = 3;
    pSpi->marketRetryWaitTick.erase(inst);
    pSpi->currentReqInst.clear();
    pSpi->currentReqSendTick = 0;
    char msg[256];
    if (inst.empty()) {
        SafeFormat(msg, sizeof(msg), "行情查询超时，跳过: 全部");
    } else {
        SafeFormat(msg, sizeof(msg), "行情查询超时，跳过: %s", inst.c_str());
    }
    pSpi->UpdateStatus(msg);
    LogMessage(msg);
    if (pSpi->HasPendingMarketQuery()) {
        pSpi->SendNextMarketQuery();
    } else {
        pSpi->FinishMarketBatch();
    }
    return 1;
}

extern "C" int IsMarketQueryActive(CTPTrader* trader) {
    if (!trader || !trader->pSpi) return 0;
    return trader->pSpi->IsMarketQueryActive() ? 1 : 0;
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
        trader->pSpi->UpdateStatus("期权查询进行中，请稍后重试");
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
        return 1;
    }
    if (!mdFrontAddr || !mdFrontAddr[0]) {
        trader->pMdSpi->UpdateStatus("错误: 行情前置地址为空");
        return -1;
    }
    if (!trader->pSpi) {
        trader->pMdSpi->UpdateStatus("错误: 交易对象未初始化");
        return -1;
    }

    {
        const char* ver = CThostFtdcMdApi::GetApiVersion();
        char verMsg[128];
        SafeFormat(verMsg, sizeof(verMsg), "MD API Version: %s", ver ? ver : "unknown");
        LogMessage(verMsg);
        trader->pMdSpi->UpdateStatus(verMsg);
    }
    if (!trader->pSpi->brokerID[0] || !trader->pSpi->userID[0] || !trader->pSpi->password[0]) {
        trader->pMdSpi->UpdateStatus("错误: 请先填写并连接交易账户(用于行情登录)");
        return -1;
    }

    // Reuse trading credentials for MD login.
    strncpy(trader->pMdSpi->brokerID, trader->pSpi->brokerID, sizeof(trader->pMdSpi->brokerID) - 1);
    strncpy(trader->pMdSpi->userID, trader->pSpi->userID, sizeof(trader->pMdSpi->userID) - 1);
    strncpy(trader->pMdSpi->password, trader->pSpi->password, sizeof(trader->pMdSpi->password) - 1);
    {
        char msg[256];
        char maskedUser[32];
        MaskId(trader->pMdSpi->userID, maskedUser, sizeof(maskedUser));
        SafeFormat(msg, sizeof(msg), "行情连接参数: front=%s, broker=%s, user=%s", mdFrontAddr, trader->pMdSpi->brokerID, maskedUser);
        LogMessage(msg);
    }

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
    std::vector<std::string> insts;
    SplitInstrumentsCsv(instrumentsCsv, insts);
    if (insts.empty()) {
        trader->pMdSpi->UpdateStatus("请输入要订阅的合约代码(支持逗号/空格分隔)");
        return -1;
    }
    for (size_t i = 0; i < insts.size(); ++i) {
        NormalizeInstrumentId(insts[i]);
    }
    {
        char msg[256];
        if (insts.size() == 1) {
            SafeFormat(msg, sizeof(msg), "订阅合约列表: %s (共%u)", insts[0].c_str(), (unsigned)insts.size());
        } else {
            SafeFormat(msg, sizeof(msg), "订阅合约列表: %s, %s%s (共%u)", insts[0].c_str(), insts[1].c_str(), insts.size() > 2 ? " ..." : "", (unsigned)insts.size());
        }
        LogMessage(msg);
    }
    if (!trader->pMdSpi->isLoggedIn) {
        trader->pMdSpi->pendingSubs = insts;
        trader->pMdSpi->UpdateStatus("行情未登录，已缓存订阅，登录后自动发送");
        return -2;
    }
    return trader->pMdSpi->SubscribeList(insts);
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
    for (size_t i = 0; i < insts.size(); ++i) {
        NormalizeInstrumentId(insts[i]);
    }
    {
        char msg[256];
        if (insts.size() == 1) {
            SafeFormat(msg, sizeof(msg), "退订合约列表: %s (共%u)", insts[0].c_str(), (unsigned)insts.size());
        } else {
            SafeFormat(msg, sizeof(msg), "退订合约列表: %s, %s%s (共%u)", insts[0].c_str(), insts[1].c_str(), insts.size() > 2 ? " ..." : "", (unsigned)insts.size());
        }
        LogMessage(msg);
    }
    trader->pMdSpi->StartUnsubscribeTracking((int)insts.size());
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
    SafeFormat(req.OrderRef, sizeof(req.OrderRef), "%d", ++orderRef);
    
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
    SafeFormat(logMsg, sizeof(logMsg), "SendOrder: %s, Direction=%c, Offset=%c, Price=%.2f, Volume=%d",
            instrumentID, direction, offsetFlag, price, volume);
    LogMessage(logMsg);
    
    int ret = trader->pSpi->pUserApi->ReqOrderInsert(&req, ++trader->pSpi->requestID);
    
    SafeFormat(logMsg, sizeof(logMsg), "ReqOrderInsert returned: %d", ret);
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
    SafeFormat(logMsg, sizeof(logMsg), "CancelOrder: OrderRef=%s, Exchange=%s, OrderSysID=%s",
            orderRef ? orderRef : "", exchangeID ? exchangeID : "", orderSysID ? orderSysID : "");
    LogMessage(logMsg);
    
    int ret = trader->pSpi->pUserApi->ReqOrderAction(&req, ++trader->pSpi->requestID);
    
    SafeFormat(logMsg, sizeof(logMsg), "ReqOrderAction returned: %d", ret);
    LogMessage(logMsg);
    
    return ret;
}
