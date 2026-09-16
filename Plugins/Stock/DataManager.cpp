#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include "Stock.h"
#include "SignalAnalyzer.h"
#include <vector>

#include <sstream>
#include "../utilities/IniHelper.h"
#include "sqlite3.h"
#include <algorithm>
#include <cmath>

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

// 将关联股票信息序列化为 "code,ratio"（ratio 保留4位小数，小数点统一用 '.'）
static std::wstring RelatedStockToString(const RelatedStockInfo& info)
{
	wchar_t ratio_buf[32];
	swprintf_s(ratio_buf, L"%.4f", info.ratio);
	std::wstring ratio_str = ratio_buf;
	// 某些区域设置下 swprintf 可能使用逗号作为小数点，统一替换为点
	std::replace(ratio_str.begin(), ratio_str.end(), L',', L'.');
	return info.code + L"," + ratio_str;
}

// 解析 "code,ratio"。旧格式只有 code（无占比），此时 ratio 返回 -1
// 以便调用方按均分处理
static RelatedStockInfo ParseRelatedStock(const std::wstring& str)
{
	RelatedStockInfo info;
	info.code = str;
	info.ratio = -1.0;
	size_t comma = str.find(L',');
	if (comma != std::wstring::npos)
	{
		info.code = str.substr(0, comma);
		std::wstring ratio_str = str.substr(comma + 1);
		std::replace(ratio_str.begin(), ratio_str.end(), L',', L'.');   // 兼容逗号小数点
		try
		{
			double ratio = std::stod(ratio_str);
			if (ratio >= 0.0 && ratio <= 1.0)
				info.ratio = ratio;
		}
		catch (...)
		{
		}
	}
	return info;
}

// 前置声明：筹码分布相关静态函数（定义在文件后方，供 Apply* 方法调用）
static bool IsSameLocalDate(time_t lhs, time_t rhs);
static bool CalculateEtfChipDistribution(const std::vector<STOCK::ChipKLinePoint>& klines, STOCK::Volume totalShares, STOCK::ChipDistribution& chipData);
static bool CalculateChipDistribution(const std::vector<STOCK::ChipKLinePoint>& klines, STOCK::ChipDistribution& chipData);

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
	m_setting_data.m_use_socks5_proxy = ini.GetBool(L"config", L"use_socks5_proxy", false);
	m_setting_data.m_socks5_proxy = ini.GetString(L"config", L"socks5_proxy", L"");

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

		std::vector<std::wstring> related_strs;
		ini.GetStringList(code.c_str(), L"related_stocks", related_strs, std::vector<std::wstring>{});
		std::vector<RelatedStockInfo> related_info;
		for (const auto& rs : related_strs)
			related_info.push_back(ParseRelatedStock(rs));
		// 旧配置没有占比信息（ParseRelatedStock 得到 -1），按均分处理
		if (!related_info.empty())
		{
			double equal_ratio = 1.0 / related_info.size();
			for (auto& ri : related_info)
			{
				if (ri.ratio < 0.0)
					ri.ratio = equal_ratio;
			}
			m_stock_related[code] = related_info;
		}
	}

	m_db_mgr.Init(m_config_path);
	m_db_mgr.CleanExpiredData();
	LoadTodayInnerOuterSnapshots();
	LoadChipDistributions();
	LoadStockBasicData();
	LoadTimelineCache();
	LoadKLineCache(STOCK::Period::DAY);
	LoadKLineCache(STOCK::Period::MIN5);
	LoadKLineCache(STOCK::Period::MIN30);
	LoadFundNavCache();

	// 从数据库加载关联股票的均幅统计（只加载今天的记录）
	for (const auto& item : m_stock_related)
	{
		auto stats = m_db_mgr.LoadAvgDiffStats(item.first);
		if (stats.minVal != 0.0 || stats.maxVal != 0.0 || stats.currentVal != 0.0)
			m_avg_diff_stats[item.first] = stats;
	}

	// 初始化日均幅日期，确保跨天检测生效
	CheckAndResetAvgDiffDaily();
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

bool CDataManager::SaveOrderBookSnapshot(const std::wstring& stockCode, const STOCK::OrderBookSnapshotRecord& record)
{
	return m_db_mgr.SaveOrderBookSnapshot(stockCode, record);
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
		// 数据库快照为10秒间隔，需按2秒间隔线性插值填充，使采样池数据连续
		for (size_t si = 0; si < snapshots.size(); ++si)
		{
			time_t timestamp = std::get<0>(snapshots[si]);
			STOCK::Volume innerVolume = std::get<1>(snapshots[si]);
			STOCK::Volume outerVolume = std::get<2>(snapshots[si]);
			int tradingMinute = CCommon::GetTradingMinute(timestamp);
			if (tradingMinute >= 0)
			{
				// 先入池当前快照
				stockData->AddVolumeSample(timestamp, innerVolume, outerVolume);
				// 如果有下一条快照，在两者之间按2秒间隔插值填充
				if (si + 1 < snapshots.size())
				{
					time_t nextTimestamp = std::get<0>(snapshots[si + 1]);
					STOCK::Volume nextInner = std::get<1>(snapshots[si + 1]);
					STOCK::Volume nextOuter = std::get<2>(snapshots[si + 1]);
					time_t gap = nextTimestamp - timestamp;
					if (gap > 2 && gap <= 600)  // 间隔合理（2秒~10分钟）才插值
					{
						int steps = static_cast<int>((gap - 2) / 2);  // 中间需要插入的点数
						for (int k = 1; k <= steps; ++k)
						{
							time_t t = timestamp + k * 2;
							double frac = static_cast<double>(k * 2) / gap;
							STOCK::Volume interpInner = innerVolume + static_cast<STOCK::Volume>((nextInner - innerVolume) * frac);
							STOCK::Volume interpOuter = outerVolume + static_cast<STOCK::Volume>((nextOuter - outerVolume) * frac);
							stockData->AddVolumeSample(t, interpInner, interpOuter);
						}
					}
				}
			}
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
		ini.WriteBool(L"config", L"use_socks5_proxy", m_setting_data.m_use_socks5_proxy);
		ini.WriteString(L"config", L"socks5_proxy", m_setting_data.m_socks5_proxy);

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

		// 保存每个股票的关联股票配置（每条为 "code,ratio"）
		for (const auto& item : m_stock_related)
		{
			std::vector<std::wstring> related_strs;
			for (const auto& ri : item.second)
				related_strs.push_back(RelatedStockToString(ri));
			ini.WriteStringList(item.first.c_str(), L"related_stocks", related_strs);
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

int CDataManager::DPI(int pixel)
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

// ===== 数据存储/更新方法实现 =====
// 由 CStockFetchThread 获取到数据后调用，仅做解析/存储，不发起网络请求

// 共享内存A股实时行情：将 QuoteItem 列表写入股票数据，返回有效条目数
int CDataManager::UpdateRealtimeFromQuotes(const std::vector<QuoteItem>& items)
{
	if (items.empty())
		return 0;

	int validCount = 0;
	for (const auto& item : items)
	{
		// 跳过无效数据
		if (item.price <= 0.001)
			continue;

		// code已包含市场前缀（如sh000001、sz000001），直接转为wstring
		std::wstring code = CCommon::StrToUnicode(item.code);

		auto stockData = stockMarket.getStock(code);
		if (!stockData)
			continue;

		StockInfo& info = stockData->info;
		info.code = code;
		info.is_ok = true;

		// 首次启动时更新股票名称（displayName为空时才从共享内存写入）
		if (info.displayName.empty() && item.name[0] != '\0')
			info.displayName = CCommon::StrToUnicode(item.name);

		bool isEtf = info.IsETF();
		// 更新价格数据
		info.prevClosePrice = (Price)(isEtf ? item.pre_close / 10 : item.pre_close);
		info.openPrice = (Price)(isEtf ? item.open / 10 : item.open);
		info.highPrice = (Price)(isEtf ? item.high / 10 : item.high);
		info.lowPrice = (Price)(isEtf ? item.low / 10 : item.low);
		info.currentPrice = (Price)(isEtf ? item.price / 10 : item.price);

		// 更新五档数据
		info.bidLevels[0] = OrderLevel((Price)(isEtf ? item.bid1 / 10 : item.bid1), (Volume)item.bid_vol1 * 100);
		info.bidLevels[1] = OrderLevel((Price)(isEtf ? item.bid2 / 10 : item.bid2), (Volume)item.bid_vol2 * 100);
		info.bidLevels[2] = OrderLevel((Price)(isEtf ? item.bid3 / 10 : item.bid3), (Volume)item.bid_vol3 * 100);
		info.bidLevels[3] = OrderLevel((Price)(isEtf ? item.bid4 / 10 : item.bid4), (Volume)item.bid_vol4 * 100);
		info.bidLevels[4] = OrderLevel((Price)(isEtf ? item.bid5 / 10 : item.bid5), (Volume)item.bid_vol5 * 100);
		info.askLevels[0] = OrderLevel((Price)(isEtf ? item.ask1 / 10 : item.ask1), (Volume)item.ask_vol1 * 100);
		info.askLevels[1] = OrderLevel((Price)(isEtf ? item.ask2 / 10 : item.ask2), (Volume)item.ask_vol2 * 100);
		info.askLevels[2] = OrderLevel((Price)(isEtf ? item.ask3 / 10 : item.ask3), (Volume)item.ask_vol3 * 100);
		info.askLevels[3] = OrderLevel((Price)(isEtf ? item.ask4 / 10 : item.ask4), (Volume)item.ask_vol4 * 100);
		info.askLevels[4] = OrderLevel((Price)(isEtf ? item.ask5 / 10 : item.ask5), (Volume)item.ask_vol5 * 100);

		// 更新成交量/额（共享内存中vol已经是手）
		info.volume = (Volume)item.vol * 100;       // 手->股
		info.turnover = (Amount)item.amount;
		info.innerVolume = (Volume)item.inner_vol * 100; // 手->股
		info.outerVolume = (Volume)item.outer_vol * 100; // 手->股

		// 更新内外盘采样（净比计算）
		stockData->UpdateVolumeSample();

		// 更新五档挂单变化量（+N/-N）
		stockData->UpdateOrderPriceAccum();

		// 更新显示字段（价格/涨跌幅/涨跌额）
		info.UpdateDisplayFields();

		validCount++;
	}

	// 更新关联股票均幅统计:当前获取的是A股数据，关联的股票全部是港股
	// if (validCount > 0)
	// 	UpdateRelatedStocksAvgDiff();

	return validCount;
}

void CDataManager::ApplyRealtimeData(const std::vector<std::wstring>& codes, const std::string& resp)
{
	stockMarket.LoadRealtimeDataByJson(resp, codes);

	UpdateRelatedStocksAvgDiff();
}

void CDataManager::ApplyInnerOuterData(const std::string& resp)
{
	stockMarket.LoadInnerOuterData(resp);
}

void CDataManager::ApplyCallAuctionData(const std::string& resp)
{
	stockMarket.LoadCallAuctionData(resp);
}

void CDataManager::ApplyTimeline(const std::wstring& code, const std::string& resp, bool ok)
{
	if (!ok)
	{
		stockMarket.LoadTimelineDataByJson(code, NULL);
		return;
	}

	CString strData(resp.c_str());
	stockMarket.LoadTimelineDataByJson(code, &strData);
	auto stockData = GetStockData(code);
	auto timelineData = stockData ? stockData->getTimelineData() : nullptr;
	if (timelineData && !timelineData->data.empty())
	{
		SaveTimelineCache(code, timelineData->data);
		// 分时数据加载后，合并基金净值缓存到iopv字段
		if (CCommon::IsFundCode(code))
		{
			auto navPoints = m_db_mgr.LoadLatestFundNavCache(code);
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

void CDataManager::ApplyDayKLine(const std::wstring& code, const std::string& resp, bool ok)
{
	if (!ok)
	{
		stockMarket.LoadKLineDataByJson(code, NULL);
		return;
	}

	CString strData(resp.c_str());
	stockMarket.LoadKLineDataByJson(code, &strData);
	auto stockData = GetStockData(code);
	auto klineData = stockData ? stockData->getKLineData() : nullptr;
	if (klineData && !klineData->data.empty())
		SaveKLineCache(code, STOCK::Period::DAY, klineData->data);
}

void CDataManager::ApplyMin5KLine(const std::wstring& code, const std::string& resp, bool ok)
{
	if (!ok)
	{
		stockMarket.LoadMin5KLineDataByJson(code, NULL);
		return;
	}

	CString strData(resp.c_str());
	stockMarket.LoadMin5KLineDataByJson(code, &strData);
	auto stockData = GetStockData(code);
	auto klineData = stockData ? stockData->getMin5KLineData() : nullptr;
	if (klineData && !klineData->data.empty())
		SaveKLineCache(code, STOCK::Period::MIN5, klineData->data);
}

void CDataManager::ApplyMin30KLine(const std::wstring& code, const std::string& resp, bool ok)
{
	if (!ok)
	{
		stockMarket.LoadMin30KLineDataByJson(code, NULL);
		return;
	}

	CString strData(resp.c_str());
	stockMarket.LoadMin30KLineDataByJson(code, &strData);
	auto stockData = GetStockData(code);
	auto klineData = stockData ? stockData->getMin30KLineData() : nullptr;
	if (klineData && !klineData->data.empty())
		SaveKLineCache(code, STOCK::Period::MIN30, klineData->data);
}

void CDataManager::ApplyFundIOPV(const std::wstring& code, const std::string& resp, bool ok)
{
	if (!ok)
	{
		std::string failLog = "[IOPV] FAIL: GetURL failed for " + CCommon::UnicodeToStr(code.c_str());
		CCommon::WriteLog(failLog.c_str(), m_log_path.c_str());
		return;
	}

	CString strData(resp.c_str());
	stockMarket.LoadFundIOPVData(code, strData);

	// 将当前IOPV值按分钟保存到数据库（仅交易时段写入，避免非交易时段写入无效时间戳）
	auto stockData = GetStockData(code);
	if (stockData && stockData->info.iopv > 0 && CCommon::IsMarketSession())
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
		SaveFundNavCache(code, navData);

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

void CDataManager::ApplyStockBasic(const std::wstring& code, STOCK::Volume circulatingAShares, bool ok)
{
	// 1. 东方财富成功：写入并入库
	if (ok && circulatingAShares > 0)
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(code);
		if (stockData)
		{
			stockData->info.circulatingAShares = circulatingAShares;
			SaveStockBasicData(code, circulatingAShares);
			return;
		}
	}

	// 2. 回退：检查内存已有值（来自腾讯接口解析的流通股本）
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(code);
		if (stockData && stockData->info.circulatingAShares > 0)
		{
			SaveStockBasicData(code, stockData->info.circulatingAShares);
			return;
		}
	}

	// 3. 回退：数据库缓存
	STOCK::Volume cached = 0;
	if (m_db_mgr.LoadStockBasicData(code, cached) && cached > 0)
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(code);
		if (stockData)
			stockData->info.circulatingAShares = cached;
	}
}

bool CDataManager::TryApplyCachedChipDistribution(const std::wstring& code)
{
	// 检查数据库是否有当日缓存
	STOCK::ChipDistribution cachedData;
	if (LoadLatestChipDistribution(code, cachedData) && IsSameLocalDate(cachedData.updatedAt, time(nullptr)))
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(code);
		if (stockData)
		{
			stockData->chipDistribution = cachedData;
			return true;
		}
	}
	return false;
}

void CDataManager::ApplyChipDistribution(const std::wstring& code, const std::vector<STOCK::ChipKLinePoint>& klines, STOCK::Volume totalShares)
{
	if (klines.empty())
		return;

	// 判断是否为基金代码
	bool isFund = CCommon::IsFundCode(code);

	// 基金类型或普通股票计算失败时，需要流通股本用于ETF筹码计算
	// 如果调用方未提供 totalShares，从内存获取
	if (totalShares <= 0)
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = GetStockData(code);
		if (stockData)
			totalShares = stockData->info.circulatingAShares;
	}

	STOCK::ChipDistribution chipData;
	bool calcOk = false;
	if (isFund)
	{
		calcOk = CalculateEtfChipDistribution(klines, totalShares, chipData);
	}
	else
	{
		// 普通股票先用换手率计算
		calcOk = CalculateChipDistribution(klines, chipData);
		if (!calcOk)
		{
			// 换手率计算失败，回退到ETF算法（基于成交量/流通股本）
			calcOk = CalculateEtfChipDistribution(klines, totalShares, chipData);
		}
	}

	if (!calcOk || !chipData.IsValid())
		return;

	std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
	auto stockData = GetStockData(code);
	if (!stockData) return;
	stockData->chipDistribution = chipData;
	SaveChipDistribution(code, chipData);
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

std::vector<RelatedStockInfo> CDataManager::GetRelatedStocks(const std::wstring& code)
{
	auto it = m_stock_related.find(code);
	if (it != m_stock_related.end())
		return it->second;
	return std::vector<RelatedStockInfo>();
}

std::vector<std::wstring> CDataManager::GetRelatedStockCodes(const std::wstring& code)
{
	std::vector<std::wstring> codes;
	auto it = m_stock_related.find(code);
	if (it != m_stock_related.end())
	{
		for (const auto& ri : it->second)
			codes.push_back(ri.code);
	}
	return codes;
}

void CDataManager::SetRelatedStocks(const std::wstring& code, const std::vector<RelatedStockInfo>& related)
{
	if (related.empty())
	{
		m_stock_related.erase(code);
		m_avg_diff_stats.erase(code);
	}
	else
	{
		m_stock_related[code] = related;
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
		// 首次启动时，如果已从数据库加载了今日的均幅数据，则不需要重置
		// 只有跨天（日期变化）时才需要清零旧数据
		if (m_avg_diff_stats.empty())
			m_avg_diff_reset_pending = true;
		return;
	}

	// 跨天时标记待重置，不立即清零
	// 避免开盘前获取到昨日数据导致均幅记录被污染
	if (m_avg_diff_last_date != dateStr)
	{
		m_avg_diff_last_date = dateStr;
		m_avg_diff_reset_pending = true;
	}

	// 等到交易时段（获取到今日数据）才执行重置
	if (m_avg_diff_reset_pending && CCommon::IsMarketSession())
	{
		m_avg_diff_stats.clear();
		m_avg_diff_history.clear();
		m_avg_diff_reset_pending = false;
	}
}

void CDataManager::UpdateRelatedStocksAvgDiff()
{
	// 每次行情更新都触发跨天检查，保证运行中跨天时在交易时段清空昨日数据
	CheckAndResetAvgDiffDaily();

	// 遍历所有配置了关联股票的股票，计算并更新均幅统计
	for (const auto& item : m_stock_related)
	{
		const std::wstring& stockId = item.first;
		const std::vector<RelatedStockInfo>& relatedInfo = item.second;

		const int relatedCount = static_cast<int>(relatedInfo.size());
		if (relatedCount < 1) continue;

		double avgDiffPercent = 0.0;

		if (relatedCount == 1)
		{
			// 只有关联1只股票时，直接用该股票的最低价/最高价/实时价计算涨跌幅
			auto stockData = GetStockData(relatedInfo[0].code);
			if (stockData && stockData->info.is_ok && stockData->info.prevClosePrice != 0)
			{
				double prevClose = stockData->info.prevClosePrice;
				double lowPct = (stockData->info.lowPrice - prevClose) / prevClose * 100;
				double highPct = (stockData->info.highPrice - prevClose) / prevClose * 100;
				double curPct = stockData->info.GetChangePercent();
				avgDiffPercent = curPct;

				// 更新最低/最高/实时均幅（CheckAndResetAvgDiffDaily 保证开盘时清零旧数据）
				SetAvgDiffStats(stockId, lowPct, highPct, avgDiffPercent);
			}
			else
			{
				continue;
			}
		}
		else
		{
			// 多只关联股票时，计算平均涨幅
			for (int i = 0; i < relatedCount; i++)
			{
				auto stockData = GetStockData(relatedInfo[i].code);
				if (stockData && stockData->info.is_ok && stockData->info.prevClosePrice != 0)
				{
					double curPct = stockData->info.GetChangePercent();
					avgDiffPercent += curPct * relatedInfo[i].ratio;
				}
			}

			UpdateAvgDiffStats(stockId, avgDiffPercent);
		}

		// 每5秒采样一次均值到历史队列（最多60个，5分钟数据）
		time_t sampleNow = time(nullptr);
		time_t& lastSampleTime = m_avg_diff_last_sample_time[stockId];
		if (sampleNow - lastSampleTime >= 5)
		{
			lastSampleTime = sampleNow;
			PushAvgDiffHistory(stockId, avgDiffPercent);
		}

		// 每分钟保存最低/最高均幅到数据库（交易时段或全天模式；跨天污染由每日重置兜底）
		// 注意：用按股票的时间戳，避免函数级static被多只股票共享导致只有第一只被写库
		time_t now = time(nullptr);
		time_t& lastSaveTime = m_avg_diff_last_save_time[stockId];
		if (now - lastSaveTime >= 60 && CCommon::IsMarketSession())
		{
			lastSaveTime = now;
			SaveAvgDiffStatsDb(stockId);
		}
	}
}

void CDataManager::PushAvgDiffHistory(const std::wstring& code, double avgDiff)
{
	auto& queue = m_avg_diff_history[code];
	queue.push_back(avgDiff);
	if (queue.size() > 60)
		queue.pop_front();
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
	return CSignalAnalyzer::CalcLinearRegFromDeque(win);
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
	return CSignalAnalyzer::CalcLinearRegFromDeque(it->second);
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