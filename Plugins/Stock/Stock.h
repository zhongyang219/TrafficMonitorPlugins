#pragma once
#include "Stock.h"
#include "StockItem.h"
#include <string>
#include "PluginInterface.h"
#include "ManagerDialog.h"
#include <map>
#include <vector>
#include <mutex>
#include <set>

constexpr auto kSH = L"sh";    // 上海
constexpr auto kSZ = L"sz";    // 深圳
constexpr auto kHK = L"rt_hk"; // 香港
constexpr auto kMG = L"gb_";   // 美国
constexpr auto kBJ = L"bj";    // 北京
constexpr auto kNF = L"nf";    // 国内期货
constexpr auto kHF = L"hf";    // 海外期货

const std::vector<CString> StockTypeSet{ kSH, kSZ, kHK, kMG, kBJ };

#define Stock_ITEM_MAX 20

class Stock : public ITMPlugin
{
private:
	Stock();
	virtual ~Stock();

public:
	static Stock& Instance();

	virtual IPluginItem* GetItem(int index) override;
	virtual const wchar_t* GetTooltipInfo() override;
	virtual void DataRequired() override;
	virtual OptionReturn ShowOptionsDialog(void* hParent) override;
	virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
	virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
	virtual int GetCommandCount() override;
	virtual const wchar_t* GetCommandName(int command_index) override;
	virtual void OnPluginCommand(int command_index, void* hWnd, void* para) override;
	virtual void* GetPluginIcon() override;
	virtual void OnInitialize(ITrafficMonitor* pApp) override;

	INT_PTR ShowStockManageDlg(CWnd* pWnd);
	void SendStockInfoRequest();
	void ShowContextMenu(CWnd* pWnd);
	void DisableUpdateCommand();
	void EnableUpdateCommand();

	void ShowFloatingWnd(void* hWnd, CPoint ptScreen, std::wstring stock_id);
	void DestroyFloatingWnd();
	void UpdateKLine();
	void PreloadAllKLineData();

	bool IsPriceInSafeZone(double current_price, double bid1_price, double ask1_price, double low, double high);
	void CheckPriceAlertForStock(const std::wstring& code);

public:
	std::mutex m_stockDataMutex;

private:
	static UINT ThreadCallback(LPVOID dwUser);
	void LoadContextMenu();
	void updateItems();

private:
	static Stock m_instance;
	vector<StockItem> m_items;

	bool m_is_thread_runing{};
	CManagerDialog* m_option_dlg{};         // 保存选项设置对话框的句柄
	unsigned __int64 m_last_request_time{}; // 上次请求的时间
	CMenu m_menu;
	std::mutex m_wndMutex;
	CFloatingWnd* m_pFloatingWnd;

	ITrafficMonitor* m_pMonitor{};          // 主程序接口指针

	// 价格关注弹窗状态：存储上一次弹窗时的价格
	std::map<std::wstring, double> m_last_alert_price;
	std::mutex m_alert_mutex;

	// 百分比告警状态：存储每只股票最近一次告警的百分比级别（0=未告警，正数=上涨，负数=下跌）
	std::map<std::wstring, int> m_percentage_alert_levels;
	std::mutex m_pct_alert_mutex;

	void CheckPercentageAlertForStock(const std::wstring& code);
	void CheckPercentageAlerts(double current_price, double prev_close, const std::wstring& code, const std::wstring& display_name);

	// 成本价告警状态：存储每只股票最近一次告警的百分比级别（0=未告警，正数=上涨，负数=下跌）
	std::map<std::wstring, int> m_cost_price_alert_levels;
	std::mutex m_cost_alert_mutex;

	void CheckCostPriceAlertForStock(const std::wstring& code);

	enum class HistoryPeriod {
		YEAR_1,
		YEAR_2,
		YEAR_3
	};

	enum class HistoryAlertType {
		HIGH,
		LOW
	};

	std::map<std::wstring, std::map<HistoryPeriod, std::set<HistoryAlertType>>> m_history_alert_status;
	std::mutex m_history_alert_mutex;

	void CheckHistoricalPriceAlertForStock(const std::wstring& code, const struct tm& now_tm, time_t now);

	// 趋势预警相关
	enum class TrendState {
		OSCILLATING,
		RISING,
		FALLING
	};

	enum class AlertType {
		TREND_RISING,
		TREND_FALLING,
		REVERSAL_DOWN,
		REVERSAL_UP
	};

	struct TrendAlertData {
		std::vector<double> price_history;
		static constexpr size_t MAX_HISTORY = 20;
		static constexpr size_t MIN_ANALYSIS_SIZE = 3;
		static constexpr int TREND_CONTINUOUS_COUNT = 4;
		static constexpr int REVERSE_COUNT = 3;
		static constexpr int REVERSE_WINDOW = 5;
		static constexpr int COOLDOWN_SECONDS = 120;
		static constexpr int STATE_HOLD_SECONDS = 30;

		TrendState trend_state = TrendState::OSCILLATING;
		int consecutive_rising_count = 0;
		int consecutive_falling_count = 0;

		double cached_max_price = 0.0;
		double cached_min_price = 0.0;
		bool cache_valid = false;

		time_t state_enter_time = 0;
		std::map<AlertType, time_t> last_alert_time;

		void add_price(double price);
		void recalculate_cache();
		double get_max_price() const;
		double get_min_price() const;
		double calculate_amplitude() const;
		double get_max_price_excluding_last() const;
		double get_min_price_excluding_last() const;
		double get_max_price_recent(int count) const;
		double get_min_price_recent(int count) const;
		double get_first_half_average() const;
		double get_second_half_average() const;
		bool can_alert(AlertType type) const;
		void record_alert(AlertType type);
	};

private:
	double GetTickValue(const std::wstring& code);
	double CalculateMinorFluctuationThreshold(double price, double tick);
	double CalculateTrendThreshold(double price, double tick);
	double CalculateBreakoutThreshold(double price, double tick);
	double CalculateAmplitudeThreshold(double price, double tick);
	bool IsPriceAtLimit(double current_price, double prev_close, const std::wstring& code);
	void CheckTrendAlertForStock(const std::wstring& code);

	std::map<std::wstring, TrendAlertData> m_trend_alert_data;
	std::mutex m_trend_mutex;

	bool m_kline_preloaded{ false };
	std::mutex m_preload_mutex;
};

#ifdef __cplusplus
extern "C"
{
#endif
	__declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
