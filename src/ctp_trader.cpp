// CTP Trading API Wrapper
#include "ctp_trader.h"
#include "../api/traderapi/ThostFtdcTraderApi.h"
#include <stdio.h>
#include <string.h>
#include <commctrl.h>
#include <time.h>

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
    
    void UpdateStatus(const char* msg) {
        if (statusCallback) { statusCallback(msg); }
    }
    
    void ClearListView() {
        if (hListView) {
            ListView_DeleteAllItems(hListView);
            while (ListView_DeleteColumn(hListView, 0));
        }
    }
    
    void AddColumn(int col, const char* text, int width) {
        if (!hListView) return;
        LVCOLUMN lvc = {0};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.fmt = LVCFMT_LEFT;
        lvc.cx = width;
        lvc.pszText = (LPSTR)text;
        ListView_InsertColumn(hListView, col, &lvc);
    }
    
    void AddItem(int row, int col, const char* text) {
        if (!hListView) return;
        if (col == 0) {
            LVITEM lvi = {0};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = row;
            lvi.iSubItem = 0;
            lvi.pszText = (LPSTR)text;
            ListView_InsertItem(hListView, &lvi);
        } else {
            ListView_SetItemText(hListView, row, col, (LPSTR)text);
        }
    }
    
    virtual void OnFrontConnected() {
        isConnected = true;
        LogMessage("OnFrontConnected called");
        UpdateStatus("Connected, authenticating...");
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
        sprintf(msg, "Disconnected, reason: %d", nReason);
        UpdateStatus(msg);
    }
    
    virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, 
                                   CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            char msg[256];
            sprintf(msg, "Auth failed: %s", pRspInfo->ErrorMsg);
            UpdateStatus(msg);
            return;
        }
        isAuthenticated = true;
        UpdateStatus("Auth success, logging in...");
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
            sprintf(msg, "Login failed: %s", pRspInfo->ErrorMsg);
            UpdateStatus(msg);
            return;
        }
        isLoggedIn = true;
        char msg[256];
        sprintf(msg, "Login success! TradingDay: %s", pRspUserLogin ? pRspUserLogin->TradingDay : "N/A");
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
        sprintf(msg, "Query order failed: %s", pRspInfo->ErrorMsg);
        UpdateStatus(msg);
        return;
    }
    static int row = 0;
    if (requestID != nRequestID) {
        ClearListView();
        AddColumn(0, "Time", 90);
        AddColumn(1, "Instrument", 100);
        AddColumn(2, "Direction", 60);
        AddColumn(3, "Offset", 60);
        AddColumn(4, "Price", 80);
        AddColumn(5, "Volume", 60);
        AddColumn(6, "Traded", 60);
        AddColumn(7, "Status", 100);
        AddColumn(8, "Remark", 150);
        row = 0;
    }
    if (pOrder) {
        char buf[256];
        AddItem(row, 0, pOrder->InsertTime);
        AddItem(row, 1, pOrder->InstrumentID);
        AddItem(row, 2, pOrder->Direction == '0' ? "Buy" : "Sell");
        AddItem(row, 3, pOrder->CombOffsetFlag[0] == '0' ? "Open" : "Close");
        sprintf(buf, "%.2f", pOrder->LimitPrice);
        AddItem(row, 4, buf);
        sprintf(buf, "%d", pOrder->VolumeTotalOriginal);
        AddItem(row, 5, buf);
        sprintf(buf, "%d", pOrder->VolumeTraded);
        AddItem(row, 6, buf);
        const char* status = "Unknown";
        if (pOrder->OrderStatus == '0') status = "AllTraded";
        else if (pOrder->OrderStatus == '1') status = "PartTraded";
        else if (pOrder->OrderStatus == '3') status = "NoTrade";
        else if (pOrder->OrderStatus == '5') status = "Canceled";
        AddItem(row, 7, status);
        AddItem(row, 8, pOrder->StatusMsg);
        row++;
    }
    if (bIsLast) {
        char msg[128];
        sprintf(msg, "Query order complete, count: %d", row);
        UpdateStatus(msg);
    }
}

void TraderSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        sprintf(msg, "Query position failed: %s", pRspInfo->ErrorMsg);
        UpdateStatus(msg);
        return;
    }
    static int row = 0;
    if (requestID != nRequestID) {
        ClearListView();
        AddColumn(0, "Instrument", 100);
        AddColumn(1, "Direction", 60);
        AddColumn(2, "Position", 60);
        AddColumn(3, "TodayPos", 60);
        AddColumn(4, "YdPos", 60);
        AddColumn(5, "AvgPrice", 80);
        AddColumn(6, "TodayAvg", 80);
        AddColumn(7, "Profit", 100);
        AddColumn(8, "TodayProfit", 100);
        row = 0;
    }
    if (pInvestorPosition) {
        char buf[256];
        AddItem(row, 0, pInvestorPosition->InstrumentID);
        AddItem(row, 1, pInvestorPosition->PosiDirection == '2' ? "Long" : "Short");
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
        sprintf(msg, "Query position complete, count: %d", row);
        UpdateStatus(msg);
    }
}

void TraderSpi::OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        sprintf(msg, "Query market failed: %s", pRspInfo->ErrorMsg);
        UpdateStatus(msg);
        return;
    }
    static int row = 0;
    if (requestID != nRequestID) {
        ClearListView();
        AddColumn(0, "Instrument", 100);
        AddColumn(1, "LastPrice", 80);
        AddColumn(2, "PreClose", 80);
        AddColumn(3, "Open", 80);
        AddColumn(4, "High", 80);
        AddColumn(5, "Low", 80);
        AddColumn(6, "Volume", 80);
        AddColumn(7, "OpenInt", 80);
        AddColumn(8, "Change", 80);
        AddColumn(9, "Change%", 80);
        row = 0;
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
        UpdateStatus("Query market complete");
    }
}

void TraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        char msg[256];
        sprintf(msg, "Query instrument failed: %s", pRspInfo->ErrorMsg);
        UpdateStatus(msg);
        return;
    }
    static int row = 0;
    if (requestID != nRequestID) {
        ClearListView();
        AddColumn(0, "InstrumentID", 100);
        AddColumn(1, "Name", 150);
        AddColumn(2, "Exchange", 80);
        AddColumn(3, "ProductID", 100);
        AddColumn(4, "Multiplier", 80);
        AddColumn(5, "PriceTick", 80);
        AddColumn(6, "ExpireDate", 100);
        AddColumn(7, "OpenDate", 100);
        row = 0;
    }
    if (pInstrument) {
        char buf[256];
        AddItem(row, 0, pInstrument->InstrumentID);
        AddItem(row, 1, pInstrument->InstrumentName);
        AddItem(row, 2, pInstrument->ExchangeID);
        AddItem(row, 3, pInstrument->ProductID);
        sprintf(buf, "%d", pInstrument->VolumeMultiple);
        AddItem(row, 4, buf);
        sprintf(buf, "%.4f", pInstrument->PriceTick);
        AddItem(row, 5, buf);
        AddItem(row, 6, pInstrument->ExpireDate);
        AddItem(row, 7, pInstrument->OpenDate);
        row++;
    }
    if (bIsLast) {
        char msg[128];
        sprintf(msg, "Query instrument complete, count: %d", row);
        UpdateStatus(msg);
    }
}

struct CTPTrader {
    TraderSpi* pSpi;
};

extern "C" CTPTrader* CreateCTPTrader() {
    CTPTrader* trader = new CTPTrader();
    trader->pSpi = new TraderSpi();
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
        pSpi->UpdateStatus("Already connected or connecting...");
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
            pSpi->UpdateStatus("Failed to create API instance");
            LogMessage("ERROR: Failed to create API instance");
            return -1;
        }
        LogMessage("API instance created");
        
        pSpi->pUserApi->RegisterSpi(pSpi);
        LogMessage("SPI registered");
        
        pSpi->pUserApi->SubscribePublicTopic(THOST_TERT_QUICK);
        pSpi->pUserApi->SubscribePrivateTopic(THOST_TERT_QUICK);
        LogMessage("Topics subscribed");
        
        pSpi->pUserApi->RegisterFront((char*)frontAddr);
        LogMessage("Front registered");
        
        pSpi->pUserApi->Init();
        LogMessage("API Init called - waiting for connection...");
        
        pSpi->UpdateStatus("API initialized, connecting...");
    } catch (...) {
        pSpi->UpdateStatus("Exception occurred during API initialization");
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