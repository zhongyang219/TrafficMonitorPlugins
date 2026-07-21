#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include "Stock.h"
#include <vector>
#include <sstream>
#include "../utilities/IniHelper.h"
#include <iomanip>
#include <afxinet.h>
#include "sqlite3.h"
#include "utilities/yyjson/yyjson.h"
#include "utilities/JsonHelper.h"
#include <algorithm>
#include <cmath>

constexpr auto WEB_USERAGENT = _T("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 Edg/135.0.0.0");

static double GetJsonDoubleValue(yyjson_val* val);

// 仅供 DataManager 业务逻辑使用的日期工具（数据库相关的同名工具已在 StockDbManager.cpp 内）
static std::string GetLocalDateString(time_t t)
{
	tm localTm = {};
	localtime_s(&localTm, &t);
	char buf[16];
	sprintf_s(buf, "%04d-%02d-%02d", localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday);
	return buf;
}

static std::string GetTodayDateString()
{
	return GetLocalDateString(time(nullptr));
}

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
	// 初始化DPI
	HDC hDC = ::GetDC(HWND_DESKTOP);
	m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
	::ReleaseDC(HWND_DESKTOP, hDC);
}

CDataManager::~CDataManager()
{
	SaveConfig();
	// m_db_mgr 自动析构会关闭数据库连接
}

CDataManager& CDataManager::Instance()
{
	return m_instance;
}

bool CDataManager::IsMarketOpen()
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	// 周六日休市
	if (now.wDayOfWeek == 0 || now.wDayOfWeek == 6)
		return false;
	// A股交易时间：9:30-11:30, 13:00-15:00
	int minutes = now.wHour * 60 + now.wMinute;
	if (minutes < 9 * 60 + 30)          // 9:30之前
		return false;
	if (minutes > 11 * 60 + 30 && minutes < 13 * 60)  // 11:30-13:00午休
		return false;
	if (minutes > 15 * 60)              // 15:00之后
		return false;
	return true;
}

bool CDataManager::IsTradingDaySession()
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	// 周六日休市
	if (now.wDayOfWeek == 0 || now.wDayOfWeek == 6)
		return false;
	// 交易日时段：9:30-15:00（含午休）
	int minutes = now.wHour * 60 + now.wMinute;
	if (minutes < 9 * 60 + 30)          // 9:30之前
		return false;
	if (minutes > 15 * 60)              // 15:00之后
		return false;
	return true;
}

bool CDataManager::IsCallAuctionSession()
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	// 周六日休市
	if (now.wDayOfWeek == 0 || now.wDayOfWeek == 6)
		return false;
	// 集合竞价时段：9:15-9:30
	int minutes = now.wHour * 60 + now.wMinute;
	if (minutes < 9 * 60 + 15)          // 9:15之前
		return false;
	if (minutes >= 9 * 60 + 30)         // 9:30及之后（竞价结束）
		return false;
	return true;
}

int CDataManager::GetTradingMinute(int hour, int minute)
{
	int totalMinutes = hour * 60 + minute;
	if (totalMinutes < 9 * 60 + 30)
		return -1;
	if (totalMinutes <= 11 * 60 + 30)
		return totalMinutes - (9 * 60 + 30);
	if (totalMinutes < 13 * 60)
		return -1;  // 午休期间不采样
	if (totalMinutes <= 15 * 60)
		return 120 + (totalMinutes - 13 * 60);
	return -1;
}

int CDataManager::GetTradingMinute(time_t t)
{
	std::tm tm = {};
	localtime_s(&tm, &t);
	return GetTradingMinute(tm.tm_hour, tm.tm_min);
}

void CDataManager::ResetText()
{
	stockMarket.ClearRealtimeData();
}

static void WritePrivateProfileInt(const wchar_t* app_name, const wchar_t* key_name, int value, const wchar_t* file_path)
{
	wchar_t buff[16];
	swprintf_s(buff, L"%d", value);
	WritePrivateProfileString(app_name, key_name, buff, file_path);
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
	// 获取模块的路径
	HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(hModule, path, MAX_PATH);
	std::wstring module_path = path;
	m_config_path = module_path;
	m_log_path = module_path;
	if (!config_dir.empty())
	{
		size_t index = module_path.find_last_of(L"\\/");
		// 模块的文件名
		std::wstring module_file_name = module_path.substr(index + 1);
		module_file_name = module_file_name.substr(0, module_file_name.find_last_of(L"."));
		m_config_path = config_dir + module_file_name;
		m_log_path = config_dir + module_file_name;
	}
	m_config_path += L".ini";
	m_log_path += L".log";

	utilities::CIniHelper ini(m_config_path);
	ini.GetStringList(L"config", L"stock_code", m_setting_data.m_stock_codes, std::vector<std::wstring>{});
	m_setting_data.m_full_day = ini.GetBool(L"config", L"full_day", true);
	m_setting_data.m_show_stock_name = ini.GetBool(L"config", L"show_stock_name", true);
	m_setting_data.m_show_fluctuation = ini.GetBool(L"config", L"show_fluctuation", true);
	m_setting_data.m_color_with_price = ini.GetBool(L"config", L"color_with_price", true);
	m_setting_data.m_kline_width = ini.GetInt(L"config", L"kline_width", 450);
	m_setting_data.m_kline_height = ini.GetInt(L"config", L"kline_height", 210);

	// 加载每个股票的关注价格
	m_stock_alert_prices.clear();
	// 加载每个股票的持仓配置
	m_stock_positions.clear();
	// 加载每个股票的状态栏展示配置
	m_stock_statusbar.clear();
	// 加载每个股票的关联股票配置
	m_stock_related.clear();
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		std::wstring low_str = ini.GetString(code.c_str(), L"alert_low", L"");
		std::wstring high_str = ini.GetString(code.c_str(), L"alert_high", L"");
		double low = 0.0, high = 0.0;
		if (!low_str.empty()) low = std::stod(low_str);
		if (!high_str.empty()) high = std::stod(high_str);
		m_stock_alert_prices[code] = std::make_pair(low, high);

		std::wstring cost_str = ini.GetString(code.c_str(), L"cost_price", L"");
		std::wstring count_str = ini.GetString(code.c_str(), L"holding_count", L"");
		std::wstring buy_date = ini.GetString(code.c_str(), L"buy_date", L"");
		double cost = 0.0, count = 0.0;
		if (!cost_str.empty()) cost = std::stod(cost_str);
		if (!count_str.empty()) count = std::stod(count_str);
		m_stock_positions[code] = std::make_tuple(cost, count, buy_date);

		m_stock_statusbar[code] = ini.GetBool(code.c_str(), L"show_in_statusbar", false);

		std::vector<std::wstring> related_codes;
		ini.GetStringList(code.c_str(), L"related_stocks", related_codes, std::vector<std::wstring>{});
		if (!related_codes.empty())
			m_stock_related[code] = related_codes;
	}

	m_db_mgr.Init(m_config_path);
	m_db_mgr.CleanExpiredData();
	m_db_mgr.CleanExpiredAvgDiffStats();
	LoadTodayInnerOuterSnapshots();
	LoadChipDistributions();
	LoadStockBasicData();
	LoadTimelineCache();
	LoadKLineCache(STOCK::Period::DAY);
	LoadKLineCache(STOCK::Period::MIN5);
	LoadKLineCache(STOCK::Period::MIN30);
	LoadFundNavCache();

	// 从数据库加载关联股票的均幅统计
	for (const auto& item : m_stock_related)
	{
		auto stats = m_db_mgr.LoadAvgDiffStats(item.first);
		if (stats.minVal != 0.0 || stats.maxVal != 0.0 || stats.currentVal != 0.0)
			m_avg_diff_stats[item.first] = stats;
	}
}

// ===== 以下数据库 CRUD 方法转发至 CStockDbManager =====

bool CDataManager::SaveTradeRecord(const std::wstring& stockCode, const std::wstring& stockName, int tradeType, const std::wstring& time, double price, double amount, double totalAmount, double fee, double total)
{
	return m_db_mgr.SaveTradeRecord(stockCode, stockName, tradeType, time, price, amount, totalAmount, fee, total);
}

bool CDataManager::SaveInnerOuterSnapshot(const std::wstring& stockCode, time_t timestamp, STOCK::Volume innerVolume, STOCK::Volume outerVolume)
{
	return m_db_mgr.SaveInnerOuterSnapshot(stockCode, timestamp, innerVolume, outerVolume);
}

bool CDataManager::SaveTimelineCache(const std::wstring& stockCode, const std::vector<STOCK::TimelinePoint>& data)
{
	return m_db_mgr.SaveTimelineCache(stockCode, data);
}

bool CDataManager::SaveKLineCache(const std::wstring& stockCode, STOCK::Period period, const std::vector<STOCK::KLinePoint>& data)
{
	return m_db_mgr.SaveKLineCache(stockCode, period, data);
}

bool CDataManager::SaveFundNavCache(const std::wstring& stockCode, const std::vector<STOCK::TimelinePoint>& data)
{
	return m_db_mgr.SaveFundNavCache(stockCode, data);
}

bool CDataManager::HasTimelineCache(const std::wstring& stockCode)
{
	return m_db_mgr.HasTimelineCache(stockCode);
}

bool CDataManager::HasKLineCache(const std::wstring& stockCode, STOCK::Period period)
{
	return m_db_mgr.HasKLineCache(stockCode, period);
}

void CDataManager::LoadTimelineCache()
{
	if (!m_db_mgr.IsOpen()) return;
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		auto stockData = GetStockData(code);
		if (!stockData) continue;
		// 今天没有缓存时，CStockDbManager 内部自动回退到最近交易日
		auto points = m_db_mgr.LoadLatestTimelineCache(code);
		if (!points.empty())
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			stockData->clearTimelinePoint();
			for (const auto& point : points)
				stockData->addTimelinePoint(point);
		}
	}
}

void CDataManager::LoadKLineCache(STOCK::Period period)
{
	if (!m_db_mgr.IsOpen()) return;
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		auto stockData = GetStockData(code);
		if (!stockData) continue;
		auto points = m_db_mgr.LoadKLineCache(code, period);
		if (points.empty()) continue;

		// 5分钟/30分钟K线：如果缓存最新数据超过7天，跳过加载（等网络请求更新）
		if (period == STOCK::Period::MIN5 || period == STOCK::Period::MIN30)
		{
			std::string todayStr = GetTodayDateString();
			// 7天前的日期字符串
			time_t cutoffTime = time(nullptr) - 7 * 24 * 60 * 60;
			tm cutoffTm = {};
			localtime_s(&cutoffTm, &cutoffTime);
			char cutoffBuf[16];
			sprintf_s(cutoffBuf, "%04d-%02d-%02d", cutoffTm.tm_year + 1900, cutoffTm.tm_mon + 1, cutoffTm.tm_mday);
			std::string cutoffStr(cutoffBuf);
			const auto& lastPoint = points.back();
			if (lastPoint.day.length() < 10 || lastPoint.day.substr(0, 10) < cutoffStr)
				continue;
		}

		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		if (period == STOCK::Period::DAY)
		{
			stockData->clearKLineData();
			for (const auto& point : points)
				stockData->addKLinePoint(point);
		}
		else if (period == STOCK::Period::MIN5)
		{
			stockData->clearMin5KLineData();
			for (const auto& point : points)
				stockData->addMin5KLinePoint(point);
		}
		else if (period == STOCK::Period::MIN30)
		{
			stockData->clearMin30KLineData();
			for (const auto& point : points)
				stockData->addMin30KLinePoint(point);
		}
	}
}

void CDataManager::LoadFundNavCache()
{
	if (!m_db_mgr.IsOpen()) return;
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		// 仅对基金代码加载净值缓存
		if (!CCommon::IsFundCode(code)) continue;
		auto stockData = GetStockData(code);
		if (!stockData) continue;
		auto navPoints = m_db_mgr.LoadLatestFundNavCache(code);
		if (navPoints.empty()) continue;

		// 将基金净值数据合并到分时数据的iopv字段
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto timelineData = stockData->getTimelineData();
		if (timelineData && !timelineData->data.empty())
		{
			// 按时间匹配，将缓存的净值填入分时点的iopv字段
			// 时间格式：分时数据为"HH:MM:SS"，缓存为"HH:MM"，用前缀匹配
			size_t navIdx = 0;
			for (auto& tp : timelineData->data)
			{
				if (navIdx < navPoints.size() && tp.time.find(navPoints[navIdx].time) == 0)
				{
					tp.iopv = navPoints[navIdx].iopv;
					navIdx++;
				}
			}
		}
	}
}

bool CDataManager::SaveStockBasicData(const std::wstring& stockCode, STOCK::Volume circulatingAShares)
{
	return m_db_mgr.SaveStockBasicData(stockCode, circulatingAShares);
}

void CDataManager::LoadStockBasicData()
{
	if (!m_db_mgr.IsOpen()) return;
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		STOCK::Volume circulatingAShares = 0;
		if (m_db_mgr.LoadStockBasicData(code, circulatingAShares) && circulatingAShares > 0)
		{
			auto stockData = GetStockData(code);
			if (stockData)
			{
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				stockData->info.circulatingAShares = circulatingAShares;
			}
		}
	}
}

bool CDataManager::SaveChipDistribution(const std::wstring& stockCode, const STOCK::ChipDistribution& chipData)
{
	return m_db_mgr.SaveChipDistribution(stockCode, chipData);
}

bool CDataManager::LoadLatestChipDistribution(const std::wstring& stockCode, STOCK::ChipDistribution& chipData)
{
	return m_db_mgr.LoadLatestChipDistribution(stockCode, chipData);
}

void CDataManager::LoadChipDistributions()
{
	if (!m_db_mgr.IsOpen()) return;

	for (const auto& code : m_setting_data.m_stock_codes)
	{
		STOCK::ChipDistribution chipData;
		if (LoadLatestChipDistribution(code, chipData))
		{
			auto stockData = GetStockData(code);
			if (stockData)
			{
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				stockData->chipDistribution = chipData;
			}
		}
	}
}

void CDataManager::LoadTodayInnerOuterSnapshots()
{
	if (!m_db_mgr.IsOpen()) return;

	SYSTEMTIME st;
	GetLocalTime(&st);
	std::tm nowTm = {};
	nowTm.tm_year = st.wYear - 1900;
	nowTm.tm_mon = st.wMonth - 1;
	nowTm.tm_mday = st.wDay;
	time_t nowTime = std::mktime(&nowTm);
	if (nowTime <= 0) return;
	// 加载最近2天的数据，覆盖跨0点场景
	time_t startTime = nowTime - 24 * 60 * 60;

	for (const auto& code : m_setting_data.m_stock_codes)
	{
		auto stockData = GetStockData(code);
		if (!stockData) continue;
		stockData->ClearVolumePools();

		auto snapshots = m_db_mgr.LoadInnerOuterSnapshots(code, startTime);
		for (const auto& snap : snapshots)
		{
			time_t timestamp = std::get<0>(snap);
			STOCK::Volume innerVolume = std::get<1>(snap);
			STOCK::Volume outerVolume = std::get<2>(snap);
			int tradingMinute = GetTradingMinute(timestamp);
			if (tradingMinute >= 0)
				stockData->AddVolumeSample(timestamp, innerVolume, outerVolume);
			stockData->info.innerVolume = innerVolume;
			stockData->info.outerVolume = outerVolume;
		}
	}
}

void CDataManager::SaveConfig()
{
	if (!m_config_path.empty())
	{
		utilities::CIniHelper ini(m_config_path);
		ini.WriteStringList(L"config", L"stock_code", m_setting_data.m_stock_codes);
		ini.WriteBool(L"config", L"full_day", m_setting_data.m_full_day);
		ini.WriteBool(L"config", L"show_stock_name", m_setting_data.m_show_stock_name);
		ini.WriteBool(L"config", L"show_fluctuation", m_setting_data.m_show_fluctuation);
		ini.WriteBool(L"config", L"color_with_price", m_setting_data.m_color_with_price);
		ini.WriteInt(L"config", L"kline_width", m_setting_data.m_kline_width);
		ini.WriteInt(L"config", L"kline_height", m_setting_data.m_kline_height);

		// 保存每个股票的关注价格到 CIniHelper 缓冲区
		for (const auto& alert : m_stock_alert_prices)
		{
			const std::wstring& code = alert.first;
			double low = alert.second.first;
			double high = alert.second.second;
			if (low > 0)
			{
				ini.WriteString(code.c_str(), L"alert_low", std::to_wstring(low));
			}
			else
			{
				ini.WriteString(code.c_str(), L"alert_low", L"");
			}
			if (high > 0)
			{
				ini.WriteString(code.c_str(), L"alert_high", std::to_wstring(high));
			}
			else
			{
				ini.WriteString(code.c_str(), L"alert_high", L"");
			}
		}

		// 保存每个股票的持仓配置到 CIniHelper 缓冲区
		for (const auto& position : m_stock_positions)
		{
			const std::wstring& code = position.first;
			double cost = std::get<0>(position.second);
			double count = std::get<1>(position.second);
			const std::wstring& buy_date = std::get<2>(position.second);
			if (cost > 0)
			{
				ini.WriteString(code.c_str(), L"cost_price", std::to_wstring(cost));
			}
			else
			{
				ini.WriteString(code.c_str(), L"cost_price", L"");
			}
			if (count > 0)
			{
				ini.WriteString(code.c_str(), L"holding_count", std::to_wstring(count));
			}
			else
			{
				ini.WriteString(code.c_str(), L"holding_count", L"");
			}
			if (!buy_date.empty())
			{
				ini.WriteString(code.c_str(), L"buy_date", buy_date);
			}
			else
			{
				ini.WriteString(code.c_str(), L"buy_date", L"");
			}
		}

		// 保存每个股票的状态栏展示配置
		for (const auto& item : m_stock_statusbar)
		{
			ini.WriteBool(item.first.c_str(), L"show_in_statusbar", item.second);
		}

		// 保存每个股票的关联股票配置
		for (const auto& item : m_stock_related)
		{
			ini.WriteStringList(item.first.c_str(), L"related_stocks", item.second);
		}

		ini.Save();
	}
}

const CString& CDataManager::StringRes(UINT id)
{
	auto iter = m_string_table.find(id);
	if (iter != m_string_table.end())
	{
		return iter->second;
	}
	else
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		m_string_table[id].LoadString(id);
		return m_string_table[id];
	}
}

void CDataManager::DPIFromWindow(CWnd* pWnd)
{
	CWindowDC dc(pWnd);
	HDC hDC = dc.GetSafeHdc();
	m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
}

int CDataManager::DPI(int pixel)
{
	return m_dpi * pixel / 96;
}

float CDataManager::DPIF(float pixel)
{
	return m_dpi * pixel / 96;
}

int CDataManager::RDPI(int pixel)
{
	return pixel * 96 / m_dpi;
}

HICON CDataManager::GetIcon(UINT id)
{
	auto iter = m_icons.find(id);
	if (iter != m_icons.end())
	{
		return iter->second;
	}
	else
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
		m_icons[id] = hIcon;
		return hIcon;
	}
}

// std::wstring StockInfo::ToString(bool include_name) const
// {
//     std::wstringstream wss;
//     if (include_name)
//         wss << name << ": ";
//     wss << p << ' ' << pc;
//     return wss.str();
// }

// bool StockInfo::IsEmpty() const
// {
//     return (pc.empty() || pc == L"--%")
//         && (p.empty() || p == L"--")
//         && name.empty();
// }

std::shared_ptr<StockData> CDataManager::GetStockData(const std::wstring& code)
{
	return stockMarket.getStock(code);
}

const STOCK::ChipDistribution* CDataManager::GetChipDistribution(const std::wstring& code)
{
	auto stockData = GetStockData(code);
	if (!stockData || !stockData->chipDistribution.IsValid())
		return nullptr;
	return &stockData->chipDistribution;
}

STOCK::Volume CDataManager::GetCirculatingAShares(const std::wstring& code)
{
	auto stockData = GetStockData(code);
	return stockData ? stockData->info.circulatingAShares : 0;
}

double CDataManager::GetAlertLowPrice(const std::wstring& code)
{
	auto it = m_stock_alert_prices.find(code);
	Log1("GetAlertLowPrice: code=%s, found=%d\n", code.c_str(), it != m_stock_alert_prices.end());
	if (it != m_stock_alert_prices.end())
	{
		return it->second.first;
	}
	return 0.0;
}

double CDataManager::GetAlertHighPrice(const std::wstring& code)
{
	auto it = m_stock_alert_prices.find(code);
	Log1("GetAlertHighPrice: code=%s, found=%d\n", code.c_str(), it != m_stock_alert_prices.end());
	if (it != m_stock_alert_prices.end())
	{
		return it->second.second;
	}
	return 0.0;
}

void CDataManager::SetAlertPrice(const std::wstring& code, double low, double high)
{
	if (low > 0 || high > 0)
	{
		m_stock_alert_prices[code] = std::make_pair(low, high);
	}
	else
	{
		auto it = m_stock_alert_prices.find(code);
		if (it != m_stock_alert_prices.end())
		{
			m_stock_alert_prices.erase(it);
		}
	}
}

double CDataManager::GetCostPrice(const std::wstring& code)
{
	auto it = m_stock_positions.find(code);
	if (it != m_stock_positions.end())
	{
		return std::get<0>(it->second);
	}
	return 0.0;
}

double CDataManager::GetHoldingCount(const std::wstring& code)
{
	auto it = m_stock_positions.find(code);
	if (it != m_stock_positions.end())
	{
		return std::get<1>(it->second);
	}
	return 0.0;
}

std::wstring CDataManager::GetBuyDate(const std::wstring& code)
{
	auto it = m_stock_positions.find(code);
	if (it != m_stock_positions.end())
	{
		return std::get<2>(it->second);
	}
	return L"";
}

void CDataManager::SetPosition(const std::wstring& code, double cost, double count, const std::wstring& buy_date)
{
	if (cost > 0 || count > 0 || !buy_date.empty())
	{
		std::wstring existing_date = L"";
		auto it = m_stock_positions.find(code);
		if (it != m_stock_positions.end())
		{
			existing_date = std::get<2>(it->second);
		}
		if (buy_date.empty())
		{
			m_stock_positions[code] = std::make_tuple(cost, count, existing_date);
		}
		else
		{
			m_stock_positions[code] = std::make_tuple(cost, count, buy_date);
		}
	}
	else
	{
		// 保留记录为0值，确保SaveConfig时能写入空字符串覆盖ini中的旧值
		m_stock_positions[code] = std::make_tuple(0.0, 0.0, L"");
	}
}

bool CDataManager::GetShowInStatusBar(const std::wstring& code)
{
	auto it = m_stock_statusbar.find(code);
	if (it != m_stock_statusbar.end())
		return it->second;
	return false;
}

void CDataManager::SetShowInStatusBar(const std::wstring& code, bool show)
{
	m_stock_statusbar[code] = show;
}

std::vector<std::wstring> CDataManager::GetStatusBarStockCodes()
{
	std::vector<std::wstring> result;
	// 按照股票列表中的先后顺序遍历，保持状态栏显示顺序与配置一致
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		auto it = m_stock_statusbar.find(code);
		if (it != m_stock_statusbar.end() && it->second)
			result.push_back(code);
	}
	return result;
}

std::vector<std::wstring> CDataManager::GetRelatedStocks(const std::wstring& code)
{
	auto it = m_stock_related.find(code);
	if (it != m_stock_related.end())
		return it->second;
	return std::vector<std::wstring>();
}

void CDataManager::SetRelatedStocks(const std::wstring& code, const std::vector<std::wstring>& related_codes)
{
	if (related_codes.empty())
	{
		m_stock_related.erase(code);
		m_avg_diff_stats.erase(code);
	}
	else
	{
		m_stock_related[code] = related_codes;
	}
}

AvgDiffStats CDataManager::GetAvgDiffData(const std::wstring& code)
{
	auto it = m_avg_diff_stats.find(code);
	if (it != m_avg_diff_stats.end())
		return it->second;
	return { 0.0, 0.0, 0.0 };
}

void CDataManager::UpdateAvgDiffStats(const std::wstring& code, double avgDiff)
{
	auto it = m_avg_diff_stats.find(code);
	if (it != m_avg_diff_stats.end())
	{
		auto& data = it->second;
		if (avgDiff < data.minVal)
			data.minVal = avgDiff;
		if (avgDiff > data.maxVal)
			data.maxVal = avgDiff;
		data.currentVal = avgDiff;
	}
	else
	{
		m_avg_diff_stats[code] = { avgDiff, avgDiff, avgDiff };
	}
}

void CDataManager::SetAvgDiffStats(const std::wstring& code, double minVal, double maxVal, double currentVal)
{
	auto it = m_avg_diff_stats.find(code);
	if (it != m_avg_diff_stats.end())
	{
		auto& data = it->second;
		if (minVal < data.minVal)
			data.minVal = minVal;
		if (maxVal > data.maxVal)
			data.maxVal = maxVal;
		data.currentVal = currentVal;
	}
	else
	{
		m_avg_diff_stats[code] = { minVal, maxVal, currentVal };
	}
}

void CDataManager::ResetAvgDiffStats(const std::wstring& code)
{
	m_avg_diff_stats.erase(code);
}

bool CDataManager::SaveAvgDiffStatsDb(const std::wstring& stockCode)
{
	auto it = m_avg_diff_stats.find(stockCode);
	if (it == m_avg_diff_stats.end())
		return false;
	return m_db_mgr.SaveAvgDiffStats(stockCode, it->second.minVal, it->second.maxVal, it->second.currentVal);
}

void CDataManager::CheckAndResetAvgDiffDaily()
{
	// 获取今天日期字符串 YYYY-MM-DD
	time_t now = time(nullptr);
	struct tm t;
	localtime_s(&t, &now);
	char dateStr[16];
	sprintf_s(dateStr, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

	if (m_avg_diff_last_date.empty())
	{
		m_avg_diff_last_date = dateStr;
		return;
	}

	if (m_avg_diff_last_date != dateStr)
	{
		// 跨天了，清零所有均幅记录和历史队列
		m_avg_diff_stats.clear();
		m_avg_diff_history.clear();
		m_avg_diff_last_date = dateStr;
	}
}

void CDataManager::PushAvgDiffHistory(const std::wstring& code, double avgDiff)
{
	auto& queue = m_avg_diff_history[code];
	queue.push_back(avgDiff);
	if (queue.size() > 60)
		queue.pop_front();
}

const std::deque<double>& CDataManager::GetAvgDiffHistory(const std::wstring& code)
{
	static const std::deque<double> emptyQueue;
	auto it = m_avg_diff_history.find(code);
	if (it != m_avg_diff_history.end())
		return it->second;
	return emptyQueue;
}

// 核心线性回归计算（纯计算，无IO）
static RegResult CalcLinearReg(const std::deque<double>& win)
{
	RegResult result;
	result.valid = false;
	result.slope = 0.0;
	result.r2 = 0.0;

	int n = static_cast<int>(win.size());
	if (n < 6)
		return result;

	double Sx = 0, Sy = 0, Sxx = 0, Syy = 0, Sxy = 0;
	for (int i = 0; i < n; i++)
	{
		double x = static_cast<double>(i);
		double y = win[i];
		Sx += x;
		Sy += y;
		Sxx += x * x;
		Syy += y * y;
		Sxy += x * y;
	}

	double dn = static_cast<double>(n);
	double denom = dn * Sxx - Sx * Sx;
	if (denom == 0.0)
		return result;

	double numer = dn * Sxy - Sx * Sy;
	result.slope = numer / denom;

	double SStotal = dn * Syy - Sy * Sy;
	if (SStotal == 0.0)
	{
		// 所有y值相同，完美拟合
		result.r2 = 1.0;
	}
	else
	{
		double SSres = SStotal - numer * numer / denom;
		result.r2 = 1.0 - SSres / SStotal;
	}

	result.valid = true;
	return result;
}

RegResult CDataManager::Get1MinAvgTrend(const std::wstring& code)
{
	auto it = m_avg_diff_history.find(code);
	if (it == m_avg_diff_history.end())
	{
		RegResult r;
		r.valid = false;
		r.slope = 0.0;
		r.r2 = 0.0;
		return r;
	}

	const auto& queue = it->second;
	// 取最近6个点（30秒：6×5秒=30秒）
	int startIdx = max(0, static_cast<int>(queue.size()) - 6);
	std::deque<double> win(queue.begin() + startIdx, queue.end());
	return CalcLinearReg(win);
}

RegResult CDataManager::Get5MinAvgTrend(const std::wstring& code)
{
	auto it = m_avg_diff_history.find(code);
	if (it == m_avg_diff_history.end())
	{
		RegResult r;
		r.valid = false;
		r.slope = 0.0;
		r.r2 = 0.0;
		return r;
	}

	// 5分钟：全部60个点
	return CalcLinearReg(it->second);
}

double CDataManager::CalculateMA(const std::wstring& code, double currentPrice, int N)
{
	auto stockData = GetStockData(code);
	if (!stockData)
		return 0.0;

	auto klineObj = stockData->getKLineData();
	if (!klineObj || klineObj->data.empty())
		return 0.0;

	return klineObj->CalculateMA(N);
}

double CDataManager::CalculateAverageAmplitude(const std::wstring& code, int days)
{
	auto stockData = GetStockData(code);
	if (!stockData)
		return 0.0;

	auto klineObj = stockData->getKLineData();
	if (!klineObj || klineObj->data.empty())
		return 0.0;

	return klineObj->CalculateAverageAmplitude(days);
}

static double generateRandomDouble()
{
	srand(time(nullptr)); // 设置随机种子
	double random = (double)rand() / RAND_MAX;
	std::cout << std::fixed << std::setprecision(16);
	return random;
}

static double GetJsonDoubleValue(yyjson_val* val)
{
	if (val == nullptr) return 0.0;
	if (yyjson_is_real(val)) return yyjson_get_real(val);
	if (yyjson_is_sint(val)) return static_cast<double>(yyjson_get_sint(val));
	if (yyjson_is_uint(val)) return static_cast<double>(yyjson_get_uint(val));
	if (yyjson_is_str(val))
	{
		try
		{
			return std::stod(yyjson_get_str(val));
		}
		catch (...)
		{
			return 0.0;
		}
	}
	return 0.0;
}

static bool TryParseDouble(const std::string& value, double& result)
{
	try
	{
		result = std::stod(value);
		return true;
	}
	catch (...)
	{
		result = 0.0;
		return false;
	}
}

static bool IsSameLocalDate(time_t lhs, time_t rhs)
{
	if (lhs <= 0 || rhs <= 0) return false;
	tm lhsTm = {};
	tm rhsTm = {};
	localtime_s(&lhsTm, &lhs);
	localtime_s(&rhsTm, &rhs);
	return lhsTm.tm_year == rhsTm.tm_year && lhsTm.tm_mon == rhsTm.tm_mon && lhsTm.tm_mday == rhsTm.tm_mday;
}

static std::wstring GetEastMoneySecId(const std::wstring& stockId)
{
	std::wstring code = stockId;
	if (code.rfind(kSH, 0) == 0 || code.rfind(kSZ, 0) == 0 || code.rfind(kBJ, 0) == 0)
		code = code.substr(2);

	if (code.size() != 6)
		return L"";

	if (code[0] == L'6' || code[0] == L'5')
		return L"1." + code;
	if (code[0] == L'0' || code[0] == L'3' || code[0] == L'1')
		return L"0." + code;
	if (code[0] == L'8' || code[0] == L'4')
		return L"0." + code;
	return L"";
}

struct ChipKLinePoint
{
	std::string date;
	double open{ 0.0 };
	double close{ 0.0 };
	double high{ 0.0 };
	double low{ 0.0 };
	double turnoverRate{ 0.0 };
	STOCK::Volume volume{ 0 };
};

static double GetCostByChip(const std::vector<double>& chips, double minPrice, double accuracy, double targetChip)
{
	double sum = 0.0;
	for (size_t i = 0; i < chips.size(); ++i)
	{
		if (sum + chips[i] > targetChip)
			return minPrice + i * accuracy;
		sum += chips[i];
	}
	return minPrice + (chips.empty() ? 0 : chips.size() - 1) * accuracy;
}

static void ComputePercentChips(const std::vector<double>& chips, double minPrice, double accuracy, double totalChips, double percent, double& low, double& high, double& concentration)
{
	double lowPercent = (1.0 - percent) / 2.0;
	double highPercent = (1.0 + percent) / 2.0;
	low = GetCostByChip(chips, minPrice, accuracy, totalChips * lowPercent);
	high = GetCostByChip(chips, minPrice, accuracy, totalChips * highPercent);
	concentration = (low + high == 0.0) ? 0.0 : (high - low) / (low + high);
}

static void FillChipDistributionFromShares(const std::vector<double>& chips, double minPrice, double accuracy, double totalShares, double currentPrice, const std::string& tradeDate, STOCK::ChipDistribution& chipData)
{
	chipData.Clear();
	chipData.tradeDate = tradeDate;
	if (chips.empty() || totalShares <= 0.0) return;

	double benefitChips = 0.0;
	double weightSum = 0.0;
	for (size_t i = 0; i < chips.size(); ++i)
	{
		double price = minPrice + accuracy * i;
		weightSum += price * chips[i];
		if (currentPrice >= price)
			benefitChips += chips[i];
		STOCK::ChipPoint point;
		point.price = round(price * 100.0) / 100.0;
		point.percent = chips[i] / totalShares;
		chipData.points.push_back(point);
	}

	chipData.avgCost = weightSum / totalShares;
	ComputePercentChips(chips, minPrice, accuracy, totalShares, 0.7, chipData.cost70Low, chipData.cost70High, chipData.cost70Concentration);
	ComputePercentChips(chips, minPrice, accuracy, totalShares, 0.9, chipData.cost90Low, chipData.cost90High, chipData.cost90Concentration);
	chipData.benefitRatio = benefitChips / totalShares;
}

static void AddChipByRange(std::vector<double>& chips, double minPrice, double accuracy, double low, double high, double addShares)
{
	if (chips.empty() || addShares <= 0.0) return;
	int lowIndex = static_cast<int>(ceil((low - minPrice) / accuracy));
	int highIndex = static_cast<int>(floor((high - minPrice) / accuracy));
	lowIndex = min(static_cast<int>(chips.size()) - 1, max(0, lowIndex));
	highIndex = min(static_cast<int>(chips.size()) - 1, max(0, highIndex));
	if (highIndex < lowIndex)
		std::swap(highIndex, lowIndex);
	int rangeCnt = highIndex - lowIndex + 1;
	if (rangeCnt <= 0) return;
	double perPriceShare = addShares / rangeCnt;
	for (int i = lowIndex; i <= highIndex; ++i)
		chips[i] += perPriceShare;
}

static void UpdateChipByKLine(std::vector<double>& chips, double minPrice, double accuracy, double totalShares, const ChipKLinePoint& bar)
{
	if (chips.empty() || totalShares <= 0.0 || bar.volume <= 0) return;
	const double CHIP_ATTRITION_N = 1.3;
	const double MAX_EFFECT_TURN = 0.85;
	const double FLOAT_CORRECT_THRESHOLD = 0.01;

	double minuteTurn = static_cast<double>(bar.volume) / totalShares;
	double effTurn = min(MAX_EFFECT_TURN, minuteTurn * CHIP_ATTRITION_N);
	double retainRate = 1.0 - effTurn;
	double addTotalShare = effTurn * totalShares;
	for (auto& val : chips)
		val *= retainRate;
	AddChipByRange(chips, minPrice, accuracy, bar.low, bar.high, addTotalShare);

	double sumAll = 0.0;
	for (double val : chips)
		sumAll += val;
	if (sumAll > 0.0 && fabs(sumAll - totalShares) > FLOAT_CORRECT_THRESHOLD)
	{
		double scale = totalShares / sumAll;
		for (auto& val : chips)
			val *= scale;
	}
}

static bool CalculateEtfChipDistribution(const std::vector<ChipKLinePoint>& klines, STOCK::Volume totalShares, STOCK::ChipDistribution& chipData)
{
	if (klines.empty() || totalShares <= 0) return false;

	const double PRICE_STEP = 0.01;
	double minPrice = 999999.0;
	double maxPrice = 0.0;
	for (const auto& item : klines)
	{
		if (item.low > 0.0) minPrice = min(minPrice, item.low);
		if (item.high > 0.0) maxPrice = max(maxPrice, item.high);
	}
	if (minPrice <= 0.0 || maxPrice < minPrice) return false;

	minPrice = floor(minPrice * 100.0) / 100.0;
	maxPrice = ceil(maxPrice * 100.0) / 100.0;
	int gridCount = static_cast<int>(floor((maxPrice - minPrice) / PRICE_STEP + 0.5)) + 1;
	if (gridCount <= 0) return false;
	std::vector<double> chips(gridCount, 0.0);

	const double CHIP_ATTRITION_N = 1.3;
	const double MAX_EFFECT_TURN = 0.85;
	const auto& first = klines.front();
	double firstTurn = static_cast<double>(first.volume) / static_cast<double>(totalShares);
	double firstEffTurn = min(MAX_EFFECT_TURN, firstTurn * CHIP_ATTRITION_N);
	AddChipByRange(chips, minPrice, PRICE_STEP, first.low, first.high, firstEffTurn * totalShares);

	for (size_t i = 1; i < klines.size(); ++i)
		UpdateChipByKLine(chips, minPrice, PRICE_STEP, static_cast<double>(totalShares), klines[i]);

	FillChipDistributionFromShares(chips, minPrice, PRICE_STEP, static_cast<double>(totalShares), klines.back().close, klines.back().date, chipData);
	return chipData.IsValid();
}

static bool CalculateChipDistribution(const std::vector<ChipKLinePoint>& klines, STOCK::ChipDistribution& chipData)
{
	if (klines.empty()) return false;

	const int factor = 150;
	int index = static_cast<int>(klines.size()) - 1;
	int start = max(0, index - 120 + 1);
	double maxPrice = 0.0;
	double minPrice = 0.0;
	for (int i = start; i <= index; ++i)
	{
		maxPrice = maxPrice == 0.0 ? klines[i].high : max(maxPrice, klines[i].high);
		minPrice = minPrice == 0.0 ? klines[i].low : min(minPrice, klines[i].low);
	}
	if (maxPrice <= 0.0 || minPrice <= 0.0 || maxPrice < minPrice) return false;

	double accuracy = max(0.01, (maxPrice - minPrice) / (factor - 1));
	std::vector<double> chips(factor, 0.0);
	for (int i = start; i <= index; ++i)
	{
		const auto& item = klines[i];
		double high = item.high;
		double low = item.low;
		double avg = (item.open + item.close + item.high + item.low) / 4.0;
		double turnoverRate = min(1.0, item.turnoverRate / 100.0);
		int highIndex = static_cast<int>(floor((high - minPrice) / accuracy));
		int lowIndex = static_cast<int>(ceil((low - minPrice) / accuracy));
		highIndex = min(factor - 1, max(0, highIndex));
		lowIndex = min(factor - 1, max(0, lowIndex));
		int avgIndex = min(factor - 1, max(0, static_cast<int>(floor((avg - minPrice) / accuracy))));
		double gPoint = high == low ? factor - 1.0 : 2.0 / (high - low);

		for (double& chip : chips)
			chip *= (1.0 - turnoverRate);

		if (high == low)
		{
			chips[avgIndex] += gPoint * turnoverRate / 2.0;
		}
		else
		{
			for (int j = lowIndex; j <= highIndex; ++j)
			{
				double curPrice = minPrice + accuracy * j;
				if (curPrice <= avg)
				{
					chips[j] += fabs(avg - low) < 1e-8 ? gPoint * turnoverRate : (curPrice - low) / (avg - low) * gPoint * turnoverRate;
				}
				else
				{
					chips[j] += fabs(high - avg) < 1e-8 ? gPoint * turnoverRate : (high - curPrice) / (high - avg) * gPoint * turnoverRate;
				}
			}
		}
	}

	double totalChips = 0.0;
	for (double chip : chips)
		totalChips += chip;
	if (totalChips <= 0.0) return false;

	chipData.Clear();
	chipData.tradeDate = klines.back().date;
	chipData.avgCost = GetCostByChip(chips, minPrice, accuracy, totalChips * 0.5);
	ComputePercentChips(chips, minPrice, accuracy, totalChips, 0.7, chipData.cost70Low, chipData.cost70High, chipData.cost70Concentration);
	ComputePercentChips(chips, minPrice, accuracy, totalChips, 0.9, chipData.cost90Low, chipData.cost90High, chipData.cost90Concentration);

	double benefitChips = 0.0;
	double currentPrice = klines.back().close;
	for (int i = 0; i < factor; ++i)
	{
		double price = minPrice + accuracy * i;
		if (currentPrice >= price)
			benefitChips += chips[i];
		STOCK::ChipPoint point;
		point.price = round(price * 100.0) / 100.0;
		point.percent = chips[i] / totalChips;
		chipData.points.push_back(point);
	}
	chipData.benefitRatio = benefitChips / totalChips;
	return true;
}

void CDataManager::RequestRealtimeData()
{
	TRACE(L"RequestRealtimeData...\n");
	std::vector<std::wstring> codes = m_setting_data.m_stock_codes;

	// 分离A股和非A股代码：A股用腾讯API（行情+内外盘+IOPV一次获取），非A股用新浪API
	std::vector<std::wstring> agCodes;   // A股(SH/SZ/BJ)
	std::vector<std::wstring> otherCodes; // 港股/美股/期货

	for (const auto& code : codes)
	{
		if (code.find(kSH) == 0 || code.find(kSZ) == 0 || code.find(kBJ) == 0)
			agCodes.push_back(code);
		else
			otherCodes.push_back(code);
	}

	// A股：腾讯API一次获取行情+内外盘+IOPV
	if (!agCodes.empty())
	{
		std::vector<std::wstring> tencentCodes = agCodes;
		// 腾讯API对港股使用 r_hk 前缀（A股不需要转换）
		for (auto& code : tencentCodes)
		{
			if (code.find(kHK) == 0)
				code = L"r_" + code.substr(2);
		}

		std::wstring url{ L"http://qt.gtimg.cn/q=" };
		url += CCommon::vectorJoinString(tencentCodes, L",");
		CString strHeaders = _T("Referer: https://finance.qq.com");

		std::string stock_data;
		if (CCommon::GetURL(url, stock_data, false, WEB_USERAGENT, strHeaders, strHeaders.GetLength()))
		{
			stockMarket.LoadInnerOuterData(stock_data);
		}
	}

	// 非A股：新浪API获取行情
	if (!otherCodes.empty())
	{
		std::wstring url{ L"https://hq.sinajs.cn/?" };
		std::vector<std::wstring> params;
		params.push_back(L"_=" + std::to_wstring(generateRandomDouble()));
		params.push_back(L"list=" + CCommon::vectorJoinString(otherCodes, L","));

		url += CCommon::vectorJoinString(params, L"&");
		CString strHeaders = _T("Referer: https://finance.sina.com.cn");

		std::string Stock_data;
		if (CCommon::GetURL(url, Stock_data, false, WEB_USERAGENT, strHeaders, strHeaders.GetLength()))
		{
			stockMarket.LoadRealtimeDataByJson(Stock_data, otherCodes);
		}

		// 非A股的内外盘数据仍需腾讯API
		RequestInnerOuterData();
	}
}

void CDataManager::RequestInnerOuterData()
{
	TRACE(L"RequestInnerOuterData... 使用腾讯API获取非A股内外盘\n");
	std::vector<std::wstring> codes;
	// 仅非A股需要单独获取内外盘（A股已在RequestRealtimeData中通过腾讯API一次获取）
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		if (code.find(kSH) != 0 && code.find(kSZ) != 0 && code.find(kBJ) != 0)
			codes.push_back(code);
	}
	if (codes.empty()) return;

	// 腾讯API对港股使用 r_hk 前缀（不是 rt_hk），需要转换
	for (auto& code : codes)
	{
		if (code.find(kHK) == 0)
			code = L"r_" + code.substr(2);  // rt_hk00700 → r_hk00700
	}

	std::wstring url{ L"http://qt.gtimg.cn/q=" };
	url += CCommon::vectorJoinString(codes, L",");

	CString strHeaders = _T("Referer: https://finance.qq.com");

	std::string stock_data;
	if (CCommon::GetURL(url, stock_data, false, WEB_USERAGENT, strHeaders, strHeaders.GetLength()))
	{
		stockMarket.LoadInnerOuterData(stock_data);
	}
}

void CDataManager::RequestFundIOPV(const std::wstring& stock_id)
{
	// 仅对ETF基金代码获取IOPV
	if (!CCommon::IsFundCode(stock_id))
		return;

	// 提取纯数字代码（sh513770 → 513770）
	std::wstring pureCode = stock_id;
	if (pureCode.size() >= 8 && iswalpha(pureCode[0]) && iswalpha(pureCode[1]))
		pureCode = pureCode.substr(2);

	// 上交所ETF使用上交所实时行情接口（含IOPV）
	// 返回JSONP: jQuery...({"code":"513060","snap":["恒生医疗",0.5220,...,0.5201,...]})
	// snap[12] = IOPV值
	// 深交所ETF使用天天基金实时估值接口
	std::wstring url;
	CString strHeaders;

	if (stock_id.find(L"sh") == 0)
	{
		// 上交所ETF：yunhq.sse.com.cn接口，含真实IOPV
		// 添加时间戳参数避免CDN缓存，确保获取实时数据
		time_t now = time(nullptr);
		url = L"https://yunhq.sse.com.cn:32042/v1/sh1/snap/" + pureCode
			+ L"?callback=jQuery&select=name,last,chg_rate,change,open,prev_close,high,low,volume,amount,iopv&_="
			+ std::to_wstring(now);
		strHeaders = _T("Referer: https://etf.sse.com.cn");
	}
	else
	{
		// 深交所ETF：天天基金实时估值接口（JSONP格式）
		// 返回: jsonpgz({"fundcode":"159920","gsz":"0.3601","dwjz":"0.3441",...})
		time_t now = time(nullptr);
		url = L"http://fundgz.1234567.com.cn/js/" + pureCode + L".js?_=" + std::to_wstring(now);
		strHeaders = _T("Referer: http://fund.eastmoney.com");
	}

	std::string stock_data;
	if (CCommon::GetURL(url, stock_data, false, WEB_USERAGENT, strHeaders, strHeaders.GetLength()))
	{
		CString strData(stock_data.c_str());
		// 调试日志：输出API返回数据前200字符
		/*{
			std::string logMsg = "FundIOPV OK: " + CCommon::UnicodeToStr(stock_id.c_str()) + " len=" + std::to_string(stock_data.size()) + " data=" + stock_data.substr(0, 200);
			CCommon::WriteLog(logMsg.c_str(), g_data.m_log_path.c_str());
		}*/
		stockMarket.LoadFundIOPVData(stock_id, strData);

		// 将当前IOPV值按分钟保存到数据库
		auto stockData = GetStockData(stock_id);
		if (stockData && stockData->info.iopv > 0)
		{
			// 获取当前时间的分钟字符串（HH:MM）
			time_t now = time(nullptr);
			tm localTm = {};
			localtime_s(&localTm, &now);
			char timeBuf[16];
			sprintf_s(timeBuf, "%02d:%02d", localTm.tm_hour, localTm.tm_min);

			STOCK::TimelinePoint navPoint;
			navPoint.time = timeBuf;
			navPoint.iopv = stockData->info.iopv;
			std::vector<STOCK::TimelinePoint> navData = { navPoint };
			SaveFundNavCache(stock_id, navData);

			// 同时更新分时数据中对应时间点的iopv字段，供净值曲线绘制使用
			auto* timelineObj = stockData->getTimelineData();
			if (timelineObj && !timelineObj->data.empty())
			{
				std::string curTime(timeBuf);
				for (auto it = timelineObj->data.rbegin(); it != timelineObj->data.rend(); ++it)
				{
					if (it->time == curTime || it->time.find(curTime) == 0)
					{
						it->iopv = stockData->info.iopv;
						break;
					}
				}
			}
		}
	}
	else
	{
		CCommon::WriteLog("FundIOPV FAIL: GetURL failed", g_data.m_log_path.c_str());
	}
}

void CDataManager::RequestCallAuctionData()
{
	TRACE(L"RequestCallAuctionData... 使用腾讯API获取集合竞价数据\n");
	// 仅获取A股代码（sh/sz/bj），过滤掉港股、美股等不支持集合竞价的代码
	std::vector<std::wstring> codes;
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		if (code.find(L"sh") == 0 || code.find(L"sz") == 0 || code.find(L"bj") == 0)
			codes.push_back(code);
	}
	if (codes.empty()) return;

	std::wstring url{ L"http://qt.gtimg.cn/q=" };
	url += CCommon::vectorJoinString(codes, L",");

	CString strHeaders = _T("Referer: https://finance.qq.com");

	std::string stock_data;
	if (CCommon::GetURL(url, stock_data, false, WEB_USERAGENT, strHeaders, strHeaders.GetLength()))
	{
		stockMarket.LoadCallAuctionData(stock_data);
	}
}

void CDataManager::RequestAllStockBasicData()
{
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		RequestStockBasicData(code);
	}
}

bool CDataManager::RequestStockBasicData(std::wstring stock_id)
{
	std::wstring secId = GetEastMoneySecId(stock_id);
	if (secId.empty()) return false;

	try
	{
		TRACE(L"RequestStockBasicData...\n");
		std::wstring url{ L"https://push2.eastmoney.com/api/qt/stock/get?" };
		std::vector<std::wstring> params;
		params.push_back(L"secid=" + secId);
		params.push_back(L"fields=f43,f44,f45,f46,f47,f48,f49,f50,f51,f57,f58,f60,f85,f116,f117");
		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: https://quote.eastmoney.com");
		std::string response;
		if (!CCommon::GetURL(url, response, true, WEB_USERAGENT, strHeaders, strHeaders.GetLength()) || response.empty())
			return false;

		yyjson_doc* doc = yyjson_read(response.c_str(), response.size(), 0);
		if (doc == nullptr) return false;

		STOCK::Volume circulatingAShares = 0;
		yyjson_val* root = yyjson_doc_get_root(doc);
		yyjson_val* data = root ? yyjson_obj_get(root, "data") : nullptr;
		if (data != nullptr)
		{
			circulatingAShares = static_cast<STOCK::Volume>(GetJsonDoubleValue(yyjson_obj_get(data, "f85")));
		}
		yyjson_doc_free(doc);

		if (circulatingAShares <= 0)
			return false;

		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = GetStockData(stock_id);
			if (!stockData) return false;
			stockData->info.circulatingAShares = circulatingAShares;
		}
		SaveStockBasicData(stock_id, circulatingAShares);
		return true;
	}
	catch (CInternetException* e)
	{
		e->Delete();
	}
	catch (...)
	{
	}
	return false;
}

void CDataManager::RequestAllChipDistributionData()
{
	for (const auto& code : m_setting_data.m_stock_codes)
	{
		RequestChipDistributionData(code);
	}
}

bool CDataManager::RequestChipDistributionData(std::wstring stock_id)
{
	std::wstring secId = GetEastMoneySecId(stock_id);
	if (secId.empty()) return false;

	STOCK::ChipDistribution cachedData;
	if (LoadLatestChipDistribution(stock_id, cachedData) && IsSameLocalDate(cachedData.updatedAt, time(nullptr)))
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(stock_id);
		if (stockData)
		{
			stockData->chipDistribution = cachedData;
			return true;
		}
	}

	try
	{
		TRACE(L"RequestChipDistributionData...\n");
		std::wstring url{ L"https://push2his.eastmoney.com/api/qt/stock/kline/get?" };
		std::vector<std::wstring> params;
		params.push_back(L"secid=" + secId);
		params.push_back(L"fields1=f1,f2,f3,f4,f5,f6");
		params.push_back(L"fields2=f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61");
		params.push_back(L"klt=101");
		params.push_back(L"fqt=0");
		SYSTEMTIME st;
		GetLocalTime(&st);
		wchar_t dateBuf[16];
		swprintf_s(dateBuf, L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
		params.push_back(L"end=" + std::wstring(dateBuf));
		params.push_back(L"lmt=750");
		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: https://quote.eastmoney.com");
		std::string response;
		if (!CCommon::GetURL(url, response, true, WEB_USERAGENT, strHeaders, strHeaders.GetLength()) || response.empty())
			return false;

		yyjson_doc* doc = yyjson_read(response.c_str(), response.size(), 0);
		if (doc == nullptr) return false;

		std::vector<ChipKLinePoint> klines;
		yyjson_val* root = yyjson_doc_get_root(doc);
		yyjson_val* data = yyjson_obj_get(root, "data");
		yyjson_val* klineArr = data ? yyjson_obj_get(data, "klines") : nullptr;
		if (klineArr != nullptr && yyjson_is_arr(klineArr))
		{
			yyjson_val* item;
			yyjson_arr_iter iter;
			yyjson_arr_iter_init(klineArr, &iter);
			while ((item = yyjson_arr_iter_next(&iter)))
			{
				const char* line = yyjson_get_str(item);
				if (line == nullptr) continue;
				std::vector<std::string> values = CCommon::split(line, ',');
				if (values.size() < 11) continue;

				double open = 0.0, close = 0.0, high = 0.0, low = 0.0, volumeHands = 0.0, turnoverRate = 0.0;
				if (!TryParseDouble(values[1], open) || !TryParseDouble(values[2], close) || !TryParseDouble(values[3], high) || !TryParseDouble(values[4], low))
					continue;
				TryParseDouble(values[5], volumeHands);
				TryParseDouble(values[10], turnoverRate);
				if (high <= 0.0 || low <= 0.0 || volumeHands <= 0.0)
					continue;

				ChipKLinePoint point;
				point.date = values[0];
				point.open = open;
				point.close = close;
				point.high = high;
				point.low = low;
				point.turnoverRate = turnoverRate;
				point.volume = static_cast<STOCK::Volume>(volumeHands * 100.0);
				klines.push_back(point);
			}
		}
		yyjson_doc_free(doc);

		if (klines.empty())
			return false;

		bool isFund = false;
		STOCK::Volume totalShares = 0;
		{
			std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
			auto stockData = GetStockData(stock_id);
			if (stockData)
			{
				isFund = CCommon::IsFundCode(stock_id);
				totalShares = stockData->info.circulatingAShares;
			}
		}

		STOCK::ChipDistribution chipData;
		if (isFund)
		{
			if (totalShares <= 0)
			{
				RequestStockBasicData(stock_id);
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				auto stockData = GetStockData(stock_id);
				if (stockData)
					totalShares = stockData->info.circulatingAShares;
			}
			if (!CalculateEtfChipDistribution(klines, totalShares, chipData))
				return false;
		}
		else if (!CalculateChipDistribution(klines, chipData))
		{
			if (totalShares <= 0)
			{
				RequestStockBasicData(stock_id);
				std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
				auto stockData = GetStockData(stock_id);
				if (stockData)
					totalShares = stockData->info.circulatingAShares;
			}
			if (!CalculateEtfChipDistribution(klines, totalShares, chipData))
				return false;
		}

		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(stock_id);
		if (!stockData) return false;
		stockData->chipDistribution = chipData;
		SaveChipDistribution(stock_id, chipData);
		return true;
	}
	catch (CInternetException* e)
	{
		e->Delete();
	}
	catch (...)
	{
	}
	return false;
}

void CDataManager::RequestTimelineData(std::wstring stock_id)
{
	try
	{
		TRACE(L"RequestTimelineData...\n");

		std::wstring url{ L"https://cn.finance.sina.com.cn/minline/getMinlineData?" };
		// https://cn.finance.sina.com.cn/minline/getMinlineData?symbol=sz000100&version=7.11.0&dpc=1
		std::vector<std::wstring> params;
		params.push_back(L"symbol=" + stock_id);
		params.push_back(L"version=7.11.0");
		params.push_back(L"dpc=1");

		SYSTEMTIME st;
		GetLocalTime(&st);
		wchar_t dateBuf[20];
		swprintf_s(dateBuf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
		params.push_back(L"date=" + std::wstring(dateBuf));

		url += CCommon::vectorJoinString(params, L"&");
		// CCommon::WriteLog(url.c_str(), g_data.m_log_path.c_str());

		// CString strHeaders = L"Referer: https://finance.sina.com.cn/realstock/company/" + m_stock_id + L"/nc.shtml";
		std::wstring strHeaders{ L"Referer: https://finance.sina.com.cn/realstock/company/" };
		strHeaders += stock_id;
		strHeaders += L"/nc.shtml";
		CString headers = strHeaders.c_str();

		CInternetSession* session = new CInternetSession(WEB_USERAGENT);
		CHttpFile* pFile = (CHttpFile*)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, headers, headers.GetLength());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString strData;
			char szBuffer[1025];
			int nRead;
			while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
			{
				szBuffer[nRead] = 0;
				strData += CString(szBuffer);
			}

			stockMarket.LoadTimelineDataByJson(stock_id, &strData);
			auto stockData = GetStockData(stock_id);
			auto timelineData = stockData ? stockData->getTimelineData() : nullptr;
			if (timelineData && !timelineData->data.empty())
			{
				SaveTimelineCache(stock_id, timelineData->data);
				// 分时数据加载后，合并基金净值缓存到iopv字段
				if (CCommon::IsFundCode(stock_id))
				{
					auto navPoints = m_db_mgr.LoadLatestFundNavCache(stock_id);
					if (!navPoints.empty())
					{
						size_t navIdx = 0;
						for (auto& tp : timelineData->data)
						{
							if (navIdx < navPoints.size() && tp.time.find(navPoints[navIdx].time) == 0)
							{
								tp.iopv = navPoints[navIdx].iopv;
								navIdx++;
							}
						}
					}
				}
			}
		}

		// 清理资源
		pFile->Close();
		delete pFile;
		session->Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		stockMarket.LoadTimelineDataByJson(stock_id, NULL);
	}
}

void CDataManager::RequestKLineData(std::wstring stock_id, int days /*= 750*/)
{
	try
	{
		TRACE(L"RequestKLineData...\n");

		std::wstring url{ L"http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?" };
		std::vector<std::wstring> params;
		params.push_back(L"symbol=" + stock_id);
		params.push_back(L"scale=240");
		params.push_back(L"ma=no");
		params.push_back(L"datalen=" + std::to_wstring(days));

		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: http://finance.sina.com.cn");

		CInternetSession* session = new CInternetSession(WEB_USERAGENT);
		CHttpFile* pFile = (CHttpFile*)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, strHeaders, strHeaders.GetLength());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString strData;
			char szBuffer[1025];
			int nRead;
			while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
			{
				szBuffer[nRead] = 0;
				strData += CString(szBuffer);
			}

			stockMarket.LoadKLineDataByJson(stock_id, &strData);
			auto stockData = GetStockData(stock_id);
			auto klineData = stockData ? stockData->getKLineData() : nullptr;
			if (klineData && !klineData->data.empty())
				SaveKLineCache(stock_id, STOCK::Period::DAY, klineData->data);
		}

		pFile->Close();
		delete pFile;
		session->Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		stockMarket.LoadKLineDataByJson(stock_id, NULL);
	}
}

void CDataManager::RequestMin5KLineData(std::wstring stock_id, int datalen /*= 250*/)
{
	try
	{
		TRACE(L"RequestMin5KLineData...\n");

		std::wstring url{ L"http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?" };
		std::vector<std::wstring> params;
		params.push_back(L"symbol=" + stock_id);
		params.push_back(L"scale=5");
		params.push_back(L"ma=no");
		params.push_back(L"datalen=" + std::to_wstring(datalen));

		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: http://finance.sina.com.cn");

		CInternetSession* session = new CInternetSession(WEB_USERAGENT);
		CHttpFile* pFile = (CHttpFile*)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, strHeaders, strHeaders.GetLength());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString strData;
			char szBuffer[1025];
			int nRead;
			while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
			{
				szBuffer[nRead] = 0;
				strData += CString(szBuffer);
			}

			stockMarket.LoadMin5KLineDataByJson(stock_id, &strData);
			auto stockData = GetStockData(stock_id);
			auto klineData = stockData ? stockData->getMin5KLineData() : nullptr;
			if (klineData && !klineData->data.empty())
				SaveKLineCache(stock_id, STOCK::Period::MIN5, klineData->data);
		}

		pFile->Close();
		delete pFile;
		session->Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		stockMarket.LoadMin5KLineDataByJson(stock_id, NULL);
	}
}

void CDataManager::RequestMin30KLineData(std::wstring stock_id, int datalen /*= 250*/)
{
	try
	{
		TRACE(L"RequestMin30KLineData...\n");

		std::wstring url{ L"http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?" };
		std::vector<std::wstring> params;
		params.push_back(L"symbol=" + stock_id);
		params.push_back(L"scale=30");
		params.push_back(L"ma=no");
		params.push_back(L"datalen=" + std::to_wstring(datalen));

		url += CCommon::vectorJoinString(params, L"&");

		CString strHeaders = _T("Referer: http://finance.sina.com.cn");

		CInternetSession* session = new CInternetSession(WEB_USERAGENT);
		CHttpFile* pFile = (CHttpFile*)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, strHeaders, strHeaders.GetLength());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			CString strData;
			char szBuffer[1025];
			int nRead;
			while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
			{
				szBuffer[nRead] = 0;
				strData += CString(szBuffer);
			}

			stockMarket.LoadMin30KLineDataByJson(stock_id, &strData);
			auto stockData = GetStockData(stock_id);
			auto klineData = stockData ? stockData->getMin30KLineData() : nullptr;
			if (klineData && !klineData->data.empty())
				SaveKLineCache(stock_id, STOCK::Period::MIN30, klineData->data);
		}

		pFile->Close();
		delete pFile;
		session->Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		stockMarket.LoadMin30KLineDataByJson(stock_id, NULL);
	}
}