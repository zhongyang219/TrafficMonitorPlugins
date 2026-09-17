#pragma once
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <ctime>
#include "resource.h"
#include "StockDef.h"
#include "StockDbManager.h"
#include "TdxTcpClient.h"
#include <mutex>
#include "Common.h"

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
	bool m_use_socks5_proxy{};          // 是否启用 SOCKS5 代理
	std::wstring m_socks5_proxy;        // SOCKS5 代理地址，如 127.0.0.1:1080
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

// 关联股票信息
struct RelatedStockInfo
{
	std::wstring code{ };        // 关联股票代码（含市场类型前缀）
	double ratio{ 0.0 };         // 占比，0~1，用户在关联对话框中手动输入
};

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
	int DPI(int pixel);
	int RDPI(int pixel);
	HICON GetIcon(UINT id);
	void ResetText();
	std::shared_ptr<StockData> GetStockData(const std::wstring& code);

	// ===== 数据存储/更新方法 =====
	// 由 CStockFetchThread 获取到数据后调用，DataManager 仅负责解析/存储，不发起任何网络请求
	// 共享内存A股实时行情：将 QuoteItem 列表写入股票数据，返回有效条目数（>0 表示成功）
	int UpdateRealtimeFromQuotes(const std::vector<QuoteItem>& items);
	// 实时行情（新浪）：解析响应并写入
	void ApplyRealtimeData(const std::vector<std::wstring>& codes, const std::string& resp);
	// 内外盘（腾讯）
	void ApplyInnerOuterData(const std::string& resp);
	// 集合竞价（腾讯）
	void ApplyCallAuctionData(const std::string& resp);
	// 分时图：ok=true 解析并缓存，ok=false 清空旧数据（请求失败时）
	void ApplyTimeline(const std::wstring& code, const std::string& resp, bool ok);
	// 日K线
	void ApplyDayKLine(const std::wstring& code, const std::string& resp, bool ok);
	// 5分钟K线
	void ApplyMin5KLine(const std::wstring& code, const std::string& resp, bool ok);
	// 30分钟K线
	void ApplyMin30KLine(const std::wstring& code, const std::string& resp, bool ok);
	// ETF基金IOPV：ok=true 解析并保存净值缓存+更新分时iopv，ok=false 仅记录失败日志
	void ApplyFundIOPV(const std::wstring& code, const std::string& resp, bool ok);
	// 流通股本：ok=true且shares>0 写入并入库；否则回退到内存已有值/数据库缓存
	void ApplyStockBasic(const std::wstring& code, STOCK::Volume circulatingAShares, bool ok);
	// 筹码分布：若数据库有当日缓存则应用并返回 true（避免重复抓取）
	bool TryApplyCachedChipDistribution(const std::wstring& code);
	// 筹码分布：根据K线计算筹码分布并入库
	void ApplyChipDistribution(const std::wstring& code, const std::vector<STOCK::ChipKLinePoint>& klines, STOCK::Volume totalShares);

	bool HasKLineCache(const std::wstring& stockCode, STOCK::Period period);
	STOCK::Volume GetCirculatingAShares(const std::wstring& code);

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

	// 状态栏展示设置
	bool GetShowInStatusBar(const std::wstring& code);
	void SetShowInStatusBar(const std::wstring& code, bool show);
	std::vector<std::wstring> GetStatusBarStockCodes();

	// 关联股票设置
	std::vector<RelatedStockInfo> GetRelatedStocks(const std::wstring& code);   // 返回关联股票信息（含占比）
	std::vector<std::wstring> GetRelatedStockCodes(const std::wstring& code);   // 仅返回关联股票代码
	void SetRelatedStocks(const std::wstring& code, const std::vector<RelatedStockInfo>& related);

	// 关联股票均幅统计（最低、最高、实时）
	AvgDiffStats GetAvgDiffData(const std::wstring& code);
	void UpdateAvgDiffStats(const std::wstring& code, double avgDiff);
	void SetAvgDiffStats(const std::wstring& code, double minVal, double maxVal, double currentVal);
	bool SaveAvgDiffStatsDb(const std::wstring& stockCode);

	// 关联股票均值历史队列
	void PushAvgDiffHistory(const std::wstring& code, double avgDiff);

	// 每日开盘重置均幅记录（跨天时自动清零）
	void CheckAndResetAvgDiffDaily();

	// 更新所有关联股票的均幅统计（计算+采样+存库），在获取到最新行情数据后调用
	void UpdateRelatedStocksAvgDiff();

	// 均值线性回归趋势（基于历史队列）
	RegResult Get1MinAvgTrend(const std::wstring& code);   // 1分钟（最近12个点）
	RegResult Get5MinAvgTrend(const std::wstring& code);   // 5分钟（全部60个点）

	// 交易记录数据库操作
	bool SaveTradeRecord(const std::wstring& stockCode, const std::wstring& stockName, int tradeType, const std::wstring& time, double price, double amount, double totalAmount, double fee, double total);
	bool SaveInnerOuterSnapshot(const std::wstring& stockCode, time_t timestamp, STOCK::Volume innerVolume, STOCK::Volume outerVolume);
	bool SaveOrderBookSnapshot(const std::wstring& stockCode, const STOCK::OrderBookSnapshotRecord& record);
	bool LoadLatestChipDistribution(const std::wstring& stockCode, STOCK::ChipDistribution& chipData);
	bool SaveChipDistribution(const std::wstring& stockCode, const STOCK::ChipDistribution& chipData);
	bool SaveStockBasicData(const std::wstring& stockCode, STOCK::Volume circulatingAShares);
	void LoadTodayInnerOuterSnapshots();
	void LoadChipDistributions();
	void LoadEstimateRules();   // 启动加载当天预估成交量规则到 StockData::estimateRuleMap
	void LoadStockBasicData();

	void SetCurrentViewMode(UIViewMode mode) { m_cur_view_mode = mode; }
	UIViewMode GetCurrentViewMode() const { return m_cur_view_mode; }

private:
	bool SaveTimelineCache(const std::wstring& stockCode, const std::vector<STOCK::TimelinePoint>& data);
	bool SaveKLineCache(const std::wstring& stockCode, STOCK::Period period, const std::vector<STOCK::KLinePoint>& data);
	bool SaveFundNavCache(const std::wstring& stockCode, const std::vector<STOCK::TimelinePoint>& data);
	void LoadTimelineCache();
	void LoadKLineCache(STOCK::Period period);
	void LoadFundNavCache();

	// 数据库管理器：所有 SQLite 操作由此类负责
	CStockDbManager m_db_mgr;
public:
	CStockDbManager& GetDbManager() { return m_db_mgr; }
private:
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

	// 状态栏展示映射表: code -> show_in_statusbar
	std::map<std::wstring, bool> m_stock_statusbar;

	// 关联股票映射表: code -> related_stock_info（含占比）
	std::map<std::wstring, std::vector<RelatedStockInfo>> m_stock_related;

	// 关联股票均幅统计: code -> {min, max, current}
	std::map<std::wstring, AvgDiffStats> m_avg_diff_stats;

	// 关联股票均值历史队列: code -> deque<avg_diff>，每5秒采样一次，最多60个（5分钟）
	std::map<std::wstring, std::deque<double>> m_avg_diff_history;

	// 每个股票的上次采样时间戳，避免static变量导致多股票共享同一时间戳
	std::map<std::wstring, time_t> m_avg_diff_last_sample_time;

	// 每个股票的上次写库时间戳（每分钟保存一次，同样不能是函数级static，否则多股票互相干扰）
	std::map<std::wstring, time_t> m_avg_diff_last_save_time;

	// 每日重置跟踪：记录上次更新日期，跨天时标记待重置
	std::string m_avg_diff_last_date;
	bool m_avg_diff_reset_pending{ false };  // 跨天待重置标识，等交易时段获取到今日数据后才执行

	UIViewMode m_cur_view_mode{ UI_VIEW_MIN5_KLINE }; // 当前界面模式
};
