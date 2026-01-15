// CTP Trading API Wrapper
#pragma execution_character_set("utf-8")
#include "ctp_trader.h"
#include "../api/allapi/ThostFtdcTraderApi.h"
#include <stdio.h>
#include <string.h>
#include <commctrl.h>
#include <time.h>
#include <windows.h> // 确保包含 windows.h

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
        if (statusCallback) {
            // 直接传递 UTF-8 编码的消息，由 main.c 的 UpdateStatus 统一处理
            statusCallback(msg);
        }
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
    
    // 统一添加列表项的函数
    // 输入的文本应该是 UTF-8 编码（源文件和 CTP 消息都已转为 UTF-8）
    void AddItem(int row, int col, const char* text) {
        if (!hListView) return;
        WCHAR wtext[256];
        // 使用 CP_UTF8 转换，因为现在所有字符串都是 UTF-8 编码
        MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 256);
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
        
        // 直接使用 UTF-8 字符串常量
        AddItem(row, 2, pOrder->Direction == '0' ? "买" : "卖");
        AddItem(row, 3, pOrder->CombOffsetFlag[0] == '0' ? "开" : "平");
        
        sprintf(buf, "%.2f", pOrder->LimitPrice);
        AddItem(row, 4, buf);
        sprintf(buf, "%d", pOrder->VolumeTotalOriginal);
        AddItem(row, 5, buf);
        sprintf(buf, "%d", pOrder->VolumeTraded);
        AddItem(row, 6, buf);
        
        // StatusMsg 来自 CTP API，是 GBK 编码，需要转换为 UTF-8
        char* utf8Status = GbkToUtf8(pOrder->StatusMsg);
        AddItem(row, 7, utf8Status ? utf8Status : pOrder->StatusMsg);
        if (utf8Status) delete[] utf8Status;
        
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
        
        // 直接使用 UTF-8 字符串常量
        AddItem(row, 1, pInvestorPosition->PosiDirection == '2' ? "多" : "空");
        
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
        
        // 过滤：只保存期货合约且未过期的主力合约
        if (pInstrument->ProductClass == '1' && instrumentCount < 2000) {
            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            char today[9];
            sprintf(today, "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
            
            // 未过期的主力合约
            if (strlen(pInstrument->ExpireDate) > 0 && 
                strcmp(pInstrument->ExpireDate, today) >= 0 &&
                IsMainContract(pInstrument->InstrumentID)) {
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
        sprintf(msg, "主力合约查询完成，总合约数: %d，主力合约: %d，已显示: %d（按到期日排序）", 
                totalCount, instrumentCount, displayCount);
        UpdateStatus(msg);
        
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