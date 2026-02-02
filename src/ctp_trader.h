// CTP Trading API Wrapper Header

#ifndef CTP_TRADER_H
#define CTP_TRADER_H

#include <windows.h>

#define WM_APP_MD_UPDATE (WM_APP + 101)
#define WM_APP_STATUS_UPDATE (WM_APP + 102)
#define WM_APP_LISTVIEW_OP (WM_APP + 103)

typedef enum ListViewOpType {
    LV_OP_CLEAR = 1,
    LV_OP_ADD_COLUMN = 2,
    LV_OP_ADD_ITEM = 3
} ListViewOpType;

typedef struct ListViewOp {
    int op;
    HWND hListView;
    int row;
    int col;
    int width;
    WCHAR* wtext; // for LV_OP_ADD_COLUMN
    char* text;   // UTF-8 for LV_OP_ADD_ITEM
} ListViewOp;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CTPTrader CTPTrader;
typedef void (*StatusCallback)(const char* status);

// Sent to main window via PostMessage(WM_APP_MD_UPDATE).
// Memory is allocated in the API wrapper; receiver must free() it.
typedef struct MdUpdate {
    char instrumentID[32];
    double lastPrice;
    long long volume;
    double bidPrice1;
    long long bidVolume1;
    double askPrice1;
    long long askVolume1;
    char updateTime[16];
    int updateMillisec;
} MdUpdate;


// 创建和销毁交易对象
CTPTrader* CreateCTPTrader();
void DestroyCTPTrader(CTPTrader* trader);

// Provide main window handle for async UI updates (WM_APP_MD_UPDATE).
void SetMainWindow(CTPTrader* trader, HWND hMainWnd);

// ???????API
void Disconnect(CTPTrader* trader);

// 设置ListView控件
void SetListView(CTPTrader* trader, HWND hListView);

// 设置状态回调
void SetStatusCallback(CTPTrader* trader, StatusCallback callback);

// 写入调试日志
void LogMessage(const char* msg);

// 设置每个查询最多输出的记录数（默认100，最大10000）
void SetQueryMaxRecords(int maxRecords);

// ????????????????? 
void CancelMarketQuery(CTPTrader* trader);

// 连接和登录
int ConnectAndLogin(CTPTrader* trader, const char* brokerID, const char* userID, 
                    const char* password, const char* frontAddr, 
                    const char* authCode, const char* appID);

// 检查是否已登录
int IsLoggedIn(CTPTrader* trader);

// 查询功能
int QueryOrders(CTPTrader* trader);
int QueryPositions(CTPTrader* trader);
int QueryMarketData(CTPTrader* trader, const char* instrumentID);
int QueryMarketDataBatch(CTPTrader* trader, const char* instrumentsCsv);
int QueryInstrument(CTPTrader* trader, const char* instrumentID);
int QueryOptions(CTPTrader* trader);

// Market data subscription (MD API)
// mdFrontAddr: market front address (often different from trading front)
int ConnectMarket(CTPTrader* trader, const char* mdFrontAddr);
int SubscribeMarketData(CTPTrader* trader, const char* instrumentsCsv);
int UnsubscribeMarketData(CTPTrader* trader, const char* instrumentsCsv);

// 交易功能
// direction: '0'=买, '1'=卖
// offsetFlag: '0'=开仓, '1'=平仓, '3'=平今, '4'=平昨
int SendOrder(CTPTrader* trader, const char* instrumentID, char direction, 
              char offsetFlag, double price, int volume);
int CancelOrder(CTPTrader* trader, const char* orderRef, const char* exchangeID, 
                const char* orderSysID);

#ifdef __cplusplus
}
#endif

#endif // CTP_TRADER_H
