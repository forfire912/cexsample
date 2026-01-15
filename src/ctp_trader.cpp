// CTP Trading API Wrapper
#pragma execution_character_set("utf-8")
#include "ctp_trader.h"
#include "../api/allapi/ThostFtdcTraderApi.h"
#include <stdio.h>
#include <string.h>
#include <commctrl.h>
#include <time.h>

// Helper function to convert UTF-8 string to GBK string
// Returns a new allocated string that must be deleted by the caller.
char* Utf8ToGbk(const char* utf8Str) {
    if (!utf8Str) return NULL;

    // 1. Convert UTF-8 to Wide Char (Unicode)
    int wideCharLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (wideCharLen == 0) return NULL;
    WCHAR* wideCharStr = new WCHAR[wideCharLen];
    MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wideCharStr, wideCharLen);

    // 2. Convert Wide Char (Unicode) to GBK (ACP)
    int gbkLen = WideCharToMultiByte(CP_ACP, 0, wideCharStr, -1, NULL, 0, NULL, NULL);
    if (gbkLen == 0) {
        delete[] wideCharStr;
        return NULL;
    }
    char* gbkStr = new char[gbkLen];
    WideCharToMultiByte(CP_ACP, 0, wideCharStr, -1, gbkStr, gbkLen, NULL, NULL);

    delete[] wideCharStr;
    return gbkStr;
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
    
    TraderSpi() {
        pUserApi = NULL;
        hListView = NULL;
        statusCallback = NULL;
        requestID = 0;
        isConnected = false;
        isAuthenticated = false;
        isLoggedIn = false;
        memset(brokerID, 0, sizeof(brokerID));
        memset(userID, 0, sizeof(userID));
        memset(password, 0, sizeof(password));
        memset(authCode, 0, sizeof(authCode));
        memset(appID, 0, sizeof(appID));
    }
    
    virtual ~TraderSpi() {
        // 析构函数
    }
    
    void UpdateStatus(const char* msg) {
        if (statusCallback) { statusCallback(msg); }
    }
    
    void ClearListView() {
        if (hListView) {
            ListView_DeleteAllItems(hListView);
            while (ListView_DeleteColumn(hListView, 0));
        }
    }

    // 统一的列头添加函数，直接使用宽字符串和 SendMessage
    void AddColumn(int col, const WCHAR* text, int width) {
        if (!hListView) return;
        LVCOLUMNW lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.fmt = LVCFMT_LEFT;
        lvc.cx = width;
        lvc.pszText = (LPWSTR)text;
        SendMessage(hListView, LVM_INSERTCOLUMNW, col, (LPARAM)&lvc);
    }
    
    // This function now assumes the input text is always GBK (CP_ACP)
    void AddItem(int row, int col, const char* text) {
        if (!hListView) return;
        WCHAR wtext[256];
        MultiByteToWideChar(CP_ACP, 0, text, -1, wtext, 256);
        if (col == 0) {
            LVITEMW lvi = {0};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = row;
            lvi.iSubItem = 0;
            lvi.pszText = wtext;
            SendMessage(hListView, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
        } else {
            LVITEMW lvi2 = {0};
            lvi2.iSubItem = col;
            lvi2.pszText = wtext;
            SendMessage(hListView, LVM_SETITEMTEXTW, row, (LPARAM)&lvi2);
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
    }
    
    virtual void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
};

void TraderSpi::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "查询委托失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        return;
    }
    static int row = 0;
    static int lastRequestID = 0;
    if (lastRequestID != nRequestID) {
        ClearListView();
        AddColumn(0, L"时间", 90);
        AddColumn(1, L"合约", 100);
        AddColumn(2, L"方向", 60);
        AddColumn(3, L"开平", 60);
        AddColumn(4, L"价格", 80);
        AddColumn(5, L"数量", 60);
        AddColumn(6, L"成交", 60);
        AddColumn(7, L"状态", 100);
        AddColumn(8, L"备注", 150);
        row = 0;
        lastRequestID = nRequestID;
    }
    if (pOrder) {
        char buf[256];
        AddItem(row, 0, pOrder->InsertTime);
        AddItem(row, 1, pOrder->InstrumentID);
        
        // Convert UTF-8 literals to GBK
        char* dir = Utf8ToGbk(pOrder->Direction == '0' ? "买" : "卖");
        AddItem(row, 2, dir ? dir : "");
        if (dir) delete[] dir;
        
        char* offset = Utf8ToGbk(pOrder->CombOffsetFlag[0] == '0' ? "开" : "平");
        AddItem(row, 3, offset ? offset : "");
        if (offset) delete[] offset;
        
        sprintf(buf, "%.2f", pOrder->LimitPrice);
        AddItem(row, 4, buf);
        sprintf(buf, "%d", pOrder->VolumeTotalOriginal);
        AddItem(row, 5, buf);
        sprintf(buf, "%d", pOrder->VolumeTraded);
        AddItem(row, 6, buf);
        
        // StatusMsg is already GBK from CTP API
        AddItem(row, 7, pOrder->StatusMsg);
        AddItem(row, 8, "");
        row++;
    }
    if (bIsLast) {
        char msg[128];
        sprintf(msg, "委托查询完成，共 %d 条记录", row);
        UpdateStatus(msg);
    }
}

void TraderSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "查询持仓失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        return;
    }
    static int row = 0;
    static int lastRequestID = 0;
    if (lastRequestID != nRequestID) {
        ClearListView();
        AddColumn(0, L"合约", 100);
        AddColumn(1, L"方向", 60);
        AddColumn(2, L"持仓", 60);
        AddColumn(3, L"今仓", 60);
        AddColumn(4, L"昨仓", 60);
        AddColumn(5, L"均价", 80);
        AddColumn(6, L"今均价", 80);
        AddColumn(7, L"盈亏", 100);
        AddColumn(8, L"今盈亏", 100);
        row = 0;
        lastRequestID = nRequestID;
    }
    if (pInvestorPosition) {
        char buf[256];
        AddItem(row, 0, pInvestorPosition->InstrumentID);
        
        // Convert UTF-8 literals to GBK
        char* posDir = Utf8ToGbk(pInvestorPosition->PosiDirection == '2' ? "多" : "空");
        AddItem(row, 1, posDir ? posDir : "");
        if (posDir) delete[] posDir;
        
        sprintf(buf, "%d", pInvestorPosition->Position);
        AddItem(row, 2, buf);
        sprintf(buf, "%d", pInvestorPosition->TodayPosition);
        AddItem(row, 3, buf);
        sprintf(buf, "%d", pInvestorPosition->YdPosition);
        AddItem(row, 4, buf);
        double avgPrice = pInvestorPosition->Position > 0 ? pInvestorPosition->PositionCost / pInvestorPosition->Position : 0;
        sprintf(buf, "%.2f", avgPrice);
        AddItem(row, 5, buf);
        double todayAvgPrice = pInvestorPosition->TodayPosition > 0 ? pInvestorPosition->OpenCost / pInvestorPosition->TodayPosition : 0;
        sprintf(buf, "%.2f", todayAvgPrice);
        AddItem(row, 6, buf);
        sprintf(buf, "%.2f", pInvestorPosition->PositionProfit);
        AddItem(row, 7, buf);
        sprintf(buf, "%.2f", pInvestorPosition->PositionProfit);
        AddItem(row, 8, buf);
        row++;
    }
    if (bIsLast) {
        char msg[128];
        sprintf(msg, "持仓查询完成，共 %d 条记录", row);
        UpdateStatus(msg);
    }
}

void TraderSpi::OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "查询行情失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        return;
    }
    static int row = 0;
    static int lastRequestID = 0;
    if (lastRequestID != nRequestID) {
        ClearListView();
        AddColumn(0, L"合约代码", 100);
        AddColumn(1, L"最新价", 80);
        AddColumn(2, L"昨收价", 80);
        AddColumn(3, L"开盘价", 80);
        AddColumn(4, L"最高价", 80);
        AddColumn(5, L"最低价", 80);
        AddColumn(6, L"成交量", 80);
        AddColumn(7, L"持仓量", 80);
        AddColumn(8, L"涨跌", 80);
        AddColumn(9, L"涨跌幅", 80);
        row = 0;
        lastRequestID = nRequestID;
    }
    if (pDepthMarketData) {
        char buf[256];
        AddItem(row, 0, pDepthMarketData->InstrumentID);
        sprintf(buf, "%.2f", pDepthMarketData->LastPrice);
        AddItem(row, 1, buf);
        sprintf(buf, "%.2f", pDepthMarketData->PreClosePrice);
        AddItem(row, 2, buf);
        sprintf(buf, "%.2f", pDepthMarketData->OpenPrice);
        AddItem(row, 3, buf);
        sprintf(buf, "%.2f", pDepthMarketData->HighestPrice);
        AddItem(row, 4, buf);
        sprintf(buf, "%.2f", pDepthMarketData->LowestPrice);
        AddItem(row, 5, buf);
        sprintf(buf, "%d", pDepthMarketData->Volume);
        AddItem(row, 6, buf);
        sprintf(buf, "%d", pDepthMarketData->OpenInterest);
        AddItem(row, 7, buf);
        double change = pDepthMarketData->LastPrice - pDepthMarketData->PreClosePrice;
        sprintf(buf, "%.2f", change);
        AddItem(row, 8, buf);
        double changePercent = pDepthMarketData->PreClosePrice > 0 ? (change / pDepthMarketData->PreClosePrice * 100) : 0;
        sprintf(buf, "%.2f%%", changePercent);
        AddItem(row, 9, buf);
        row++;
    }
    if (bIsLast) {
        UpdateStatus("行情查询完成");
    }
}

void TraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        char* gbkErrorMsg = Utf8ToGbk(pRspInfo->ErrorMsg);
        sprintf(msg, "查询合约失败: %s", gbkErrorMsg ? gbkErrorMsg : "");
        UpdateStatus(msg);
        if (gbkErrorMsg) delete[] gbkErrorMsg;
        return;
    }
    
    // 使用静态变量存储合约信息
    struct InstrumentInfo {
        char instrumentID[31];
        char instrumentName[61];
        char exchangeID[9];
        char productID[31];
        int volumeMultiple;
        double priceTick;
        char expireDate[9];
        char openDate[9];
    };
    
    static InstrumentInfo* instruments = NULL;
    static int instrumentCount = 0;
    static int totalCount = 0;
    static int lastRequestID = 0;
    const int MAX_DISPLAY = 200;  // 最多显示200条活跃合约
    
    if (lastRequestID != nRequestID) {
        // 新的查询请求，清空之前的数据
        if (instruments) {
            delete[] instruments;
            instruments = NULL;
        }
        instruments = new InstrumentInfo[2000];  // 预分配空间
        instrumentCount = 0;
        totalCount = 0;
        lastRequestID = nRequestID;
        
        ClearListView();
        AddColumn(0, L"合约代码", 100);
        AddColumn(1, L"合约名称", 150);
        AddColumn(2, L"交易所", 80);
        AddColumn(3, L"品种代码", 100);
        AddColumn(4, L"合约乘数", 80);
        AddColumn(5, L"最小变动", 80);
        AddColumn(6, L"到期日", 100);
        AddColumn(7, L"上市日", 100);
    }
    
    if (pInstrument) {
        totalCount++;
        
        // 过滤：只保存期货合约且未过期的
        if (pInstrument->ProductClass == '1' && instrumentCount < 2000) {
            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            char today[9];
            sprintf(today, "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
            
            // 未过期的合约
            if (strlen(pInstrument->ExpireDate) > 0 && strcmp(pInstrument->ExpireDate, today) >= 0) {
                // 保存到数组
                strcpy(instruments[instrumentCount].instrumentID, pInstrument->InstrumentID);
                strcpy(instruments[instrumentCount].instrumentName, pInstrument->InstrumentName);
                strcpy(instruments[instrumentCount].exchangeID, pInstrument->ExchangeID);
                strcpy(instruments[instrumentCount].productID, pInstrument->ProductID);
                instruments[instrumentCount].volumeMultiple = pInstrument->VolumeMultiple;
                instruments[instrumentCount].priceTick = pInstrument->PriceTick;
                strcpy(instruments[instrumentCount].expireDate, pInstrument->ExpireDate);
                strcpy(instruments[instrumentCount].openDate, pInstrument->OpenDate);
                instrumentCount++;
            }
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
            AddItem(i, 0, instruments[i].instrumentID);
            AddItem(i, 1, instruments[i].instrumentName);
            AddItem(i, 2, instruments[i].exchangeID);
            AddItem(i, 3, instruments[i].productID);
            sprintf(buf, "%d", instruments[i].volumeMultiple);
            AddItem(i, 4, buf);
            sprintf(buf, "%.4f", instruments[i].priceTick);
            AddItem(i, 5, buf);
            AddItem(i, 6, instruments[i].expireDate);
            AddItem(i, 7, instruments[i].openDate);
        }
        
        // 显示统计信息
        char msg[256];
        sprintf(msg, "合约查询完成，总数: %d，活跃合约: %d，已显示: %d（按到期日排序）", 
                totalCount, instrumentCount, displayCount);
        UpdateStatus(msg);
        
        // 清理内存
        delete[] instruments;
        instruments = NULL;
    }
}

struct CTPTrader {
    TraderSpi* pSpi;
};

extern "C" CTPTrader* CreateCTPTrader() {
    LogMessage("CreateCTPTrader called");
    CTPTrader* trader = NULL;
    try {
        trader = new CTPTrader();
        LogMessage("CTPTrader struct allocated");
        
        trader->pSpi = new TraderSpi();
        LogMessage("TraderSpi created");
        
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
        if (trader->pSpi && trader->pSpi->pUserApi) {
            trader->pSpi->pUserApi->RegisterSpi(NULL);
            trader->pSpi->pUserApi->Release();
            trader->pSpi->pUserApi = NULL;
        }
        if (trader->pSpi) { delete trader->pSpi; }
        delete trader;
    }
}

extern "C" void SetListView(CTPTrader* trader, HWND hListView) {
    if (trader && trader->pSpi) { trader->pSpi->hListView = hListView; }
}

extern "C" void SetStatusCallback(CTPTrader* trader, StatusCallback callback) {
    if (trader && trader->pSpi) { trader->pSpi->statusCallback = callback; }
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
    CThostFtdcQryOrderField req = {0};
    strcpy(req.BrokerID, trader->pSpi->brokerID);
    strcpy(req.InvestorID, trader->pSpi->userID);
    Sleep(1000);
    return trader->pSpi->pUserApi->ReqQryOrder(&req, ++trader->pSpi->requestID);
}

extern "C" int QueryPositions(CTPTrader* trader) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    CThostFtdcQryInvestorPositionField req = {0};
    strcpy(req.BrokerID, trader->pSpi->brokerID);
    strcpy(req.InvestorID, trader->pSpi->userID);
    Sleep(1000);
    return trader->pSpi->pUserApi->ReqQryInvestorPosition(&req, ++trader->pSpi->requestID);
}

extern "C" int QueryMarketData(CTPTrader* trader, const char* instrumentID) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    CThostFtdcQryDepthMarketDataField req = {0};
    if (instrumentID && strlen(instrumentID) > 0) { strcpy(req.InstrumentID, instrumentID); }
    Sleep(1000);
    return trader->pSpi->pUserApi->ReqQryDepthMarketData(&req, ++trader->pSpi->requestID);
}

extern "C" int QueryInstrument(CTPTrader* trader, const char* instrumentID) {
    if (!trader || !trader->pSpi || !trader->pSpi->pUserApi) return -1;
    CThostFtdcQryInstrumentField req = {0};
    if (instrumentID && strlen(instrumentID) > 0) { strcpy(req.InstrumentID, instrumentID); }
    Sleep(1000);
    return trader->pSpi->pUserApi->ReqQryInstrument(&req, ++trader->pSpi->requestID);
}