// CTP Trading API Wrapper Header

#ifndef CTP_TRADER_H
#define CTP_TRADER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CTPTrader CTPTrader;
typedef void (*StatusCallback)(const char* status);

// 创建和销毁交易对象
CTPTrader* CreateCTPTrader();
void DestroyCTPTrader(CTPTrader* trader);

// 设置ListView控件
void SetListView(CTPTrader* trader, HWND hListView);

// 设置状态回调
void SetStatusCallback(CTPTrader* trader, StatusCallback callback);

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
int QueryInstrument(CTPTrader* trader, const char* instrumentID);

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
