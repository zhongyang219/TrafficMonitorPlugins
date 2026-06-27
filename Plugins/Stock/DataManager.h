#pragma once
#include <string>
#include <map>
#include "resource.h"
#include "StockDef.h"
#include <mutex>

struct sqlite3;

using namespace STOCK;

#define g_data CDataManager::Instance()

struct SettingData
{
	vector<std::wstring> m_stock_codes; // 代码
	bool m_full_day{};                  // 全天更新
	bool m_show_stock_name{};           // 显示股票名称
	bool m_show_fluctuation{};          // 显示涨跌幅
	bool m_color_with_price{};          // 涨跌颜色标识
	unsigned m_kline_width;             // 走势图宽度
	unsigned m_kline_height;            // 走势图高度
};

// Stock显示数据
// struct StockInfo
// {
//     std::wstring pc = L"--%";
//     std::wstring p = L"--";
//     std::wstring name = L"";
//     std::wstring ToString(bool include_name = true) const;
//     bool IsEmpty() const;
// };

class CDataManager
{
private:
	CDataManager();
	~CDataManager();

public:
	static CDataManager& Instance();

	void LoadConfig(const std::wstring& config_dir);
	void SaveConfig();
	const CString& StringRes(UINT id); // 根据资源id获取一个字符串资源
	void DPIFromWindow(CWnd* pWnd);
	int DPI(int pixel);
	float DPIF(float pixel);
	int RDPI(int pixel);
	HICON GetIcon(UINT id);
	void ResetText();
	std::shared_ptr<StockData> GetStockData(const std::wstring& code);

	// 判断当前是否在交易时间（A股：9:30-11:30, 13:00-15:00，非周末）
	static bool IsMarketOpen();

	// 获取最新数据
	void RequestRealtimeData();
	void RequestTimelineData(std::wstring stock_id);
	void RequestKLineData(std::wstring stock_id, int days = 250);
	void RequestMin5KLineData(std::wstring stock_id, int datalen = 250);
	void RequestInnerOuterData();

	SettingData m_setting_data;
	std::wstring m_log_path;
	bool m_right_align{}; // 数值是否右对齐

	// 关注价格设置
	double GetAlertLowPrice(const std::wstring& code);
	double GetAlertHighPrice(const std::wstring& code);
	void SetAlertPrice(const std::wstring& code, double low, double high);

	// 持仓配置设置
	double GetCostPrice(const std::wstring& code);
	double GetHoldingCount(const std::wstring& code);
	std::wstring GetBuyDate(const std::wstring& code);
	void SetPosition(const std::wstring& code, double cost, double count, const std::wstring& buy_date = L"");

	// 计算N日均线 (当前价格 + 前N-1天收盘价) / N
	double CalculateMA(const std::wstring& code, double currentPrice, int N);

	// 计算N日平均振幅
	double CalculateAverageAmplitude(const std::wstring& code, int days = 5);

	// 交易记录数据库操作
	bool SaveTradeRecord(const std::wstring& stockCode, const std::wstring& stockName, int tradeType, const std::wstring& time, double price, double amount, double totalAmount, double fee, double total);

private:
	void InitDatabase();
	sqlite3* m_db{ nullptr };
	std::wstring m_db_path;
	static CDataManager m_instance;

	std::wstring m_config_path;

	std::map<UINT, CString> m_string_table;
	std::map<UINT, HICON> m_icons;
	int m_dpi{ 96 };

	STOCK::StockMarket stockMarket;

	// 关注价格映射表: code -> (low_price, high_price)
	std::map<std::wstring, std::pair<double, double>> m_stock_alert_prices;

	// 持仓配置映射表: code -> (cost_price, holding_count, buy_date)
	std::map<std::wstring, std::tuple<double, double, std::wstring>> m_stock_positions;
};
