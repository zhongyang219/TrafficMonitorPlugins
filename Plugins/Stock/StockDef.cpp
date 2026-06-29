#include "pch.h"
#include "StockDef.h"
#include <iomanip>
#include "Common.h"
#include <DataManager.h>
#include <Stock.h>
#include "utilities/yyjson/yyjson.h"
#include "utilities/Common.h"
#include "utilities/JsonHelper.h"

// 美股数据长度
constexpr auto _DATA_LEN_MG = 36;
// 上证A股 & 指数数据长度
constexpr auto _DATA_LEN_SH = 34;
// 深证A股 & 指数数据长度
constexpr auto _DATA_LEN_SZ = 33;
// 北证A股 & 指数数据长度
constexpr auto _DATA_LEN_BJ = 39;
// 港股数据长度
constexpr auto _DATA_LEN_HK = 25;
// 国内期货数据长度
constexpr auto _DATA_LEN_NF = 16;
// 海外期货数据长度
constexpr auto _DATA_LEN_HF = 14;

using namespace STOCK;

void STOCK::StockMarket::LoadRealtimeDataByJson(std::string json)
{
	ClearRealtimeData();

	if (json == "")
	{
		CCommon::WriteLog("Response is EMPTY!", g_data.m_log_path.c_str());
		return;
	}

	std::vector<std::string> lines = CCommon::split(CCommon::removeChar(json, '\n'), ";");
	if (lines.size() < 1)
	{
		CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
		return;
	}

	for (std::string line : lines)
	{
		if (line.empty())
		{
			continue;
		}
		line = CCommon::removeChar(CCommon::removeStr(line, "var hq_str_"), '\"');

		std::vector<std::string> item_arr = CCommon::split(line, '=');
		if (item_arr.size() <= 0)
		{
			CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
			continue;
		}

		std::wstring key = CCommon::StrToUnicode(item_arr[0].c_str());
		auto stockData = getStock(key);
		stockData->info.code = CCommon::StrToUnicode(item_arr[0].c_str());

		stockData->info.is_ok = item_arr.size() >= 2;

		if (!stockData->info.is_ok)
		{
			CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
			continue;
		}

		std::string data = item_arr[1];

		std::vector<std::string> data_arr = CCommon::split(data, ",");

		stockData->info.Load(key, data_arr);

		// 买一/卖一价格变化时清零累计时间
		{
			Price curAsk1 = stockData->info.askLevels[0].price;
			Price curBid1 = stockData->info.bidLevels[0].price;

			// 卖一价变化则清零
			if (curAsk1 != stockData->prevAsk1Price)
			{
				stockData->ask1AccumSeconds = 0;
				stockData->prevAsk1Price = curAsk1;
			}
			// 买一价变化则清零
			if (curBid1 != stockData->prevBid1Price)
			{
				stockData->bid1AccumSeconds = 0;
				stockData->prevBid1Price = curBid1;
			}
		}
	}
}

void STOCK::StockMarket::LoadInnerOuterData(std::string data)
{
	if (data.empty())
	{
		return;
	}

	std::vector<std::string> lines = CCommon::split(CCommon::removeChar(data, '\n'), ";");
	for (std::string line : lines)
	{
		if (line.empty()) continue;

		size_t pos = line.find("=\"");
		if (pos == std::string::npos) continue;

		std::string key_str = line.substr(0, pos);
		if (key_str.substr(0, 2) == "v_")
			key_str = key_str.substr(2);

		std::wstring key = CCommon::StrToUnicode(key_str.c_str());
		auto stockData = getStock(key);
		if (!stockData) continue;

		std::string values = line.substr(pos + 2);
		if (!values.empty() && values.back() == '\"')
			values.pop_back();

		std::vector<std::string> data_arr = CCommon::split(values, "~");
		if (data_arr.size() >= 9)
		{
			stockData->info.innerVolume = { convert<Volume>(data_arr[8]) * 100 };
			stockData->info.outerVolume = { convert<Volume>(data_arr[7]) * 100 };
			time_t now;
			time(&now);
			if (stockData->addVolumeSnapshot(now, stockData->info.innerVolume, stockData->info.outerVolume))
			{
				g_data.SaveInnerOuterSnapshot(key, stockData->volumeSnapshots.back().timestamp, stockData->info.innerVolume, stockData->info.outerVolume);
			}
		}
		if (data_arr.size() >= 39)
		{
			stockData->info.turnoverRate = { convert<Amount>(data_arr[38]) };
		}
	}
}

void STOCK::StockInfo::Load(std::wstring key, std::vector<std::string> data_arr)
{
	size_t data_size = static_cast<size_t>(data_arr.size());

	if (key.find(kMG) == 0)
	{
		LoadMG(data_arr, data_size);
	}
	else if (key.find(kSH) == 0)
	{
		LoadSH(data_arr, data_size);
	}
	else if (key.find(kSZ) == 0)
	{
		LoadSZ(data_arr, data_size);
	}
	else if (key.find(kBJ) == 0)
	{
		LoadBJ(data_arr, data_size);
	}
	else if (key.find(kHK) == 0)
	{
		LoadHK(data_arr, data_size);
	}
	else if (key.find(kNF) == 0)
	{
		LoadNF(data_arr, data_size);
	}
	else if (key.find(kHF) == 0)
	{
		LoadHF(data_arr, data_size);
	}

	if (currentPrice > 0 && prevClosePrice > 0)
	{
		CString priceStr = CCommon::FormatFloat(currentPrice);
		displayPrice = std::wstring(priceStr.GetString());

		char buff[32];
		float diff = currentPrice - prevClosePrice;
		float change = diff / prevClosePrice * 100;
		if (diff > 0)
		{
			sprintf_s(buff, "(+%g) ", diff);
		}
		else
		{
			sprintf_s(buff, "(%g) ", diff);
		}
		displayFluctuationDiff = CCommon::StrToUnicode(buff);

		sprintf_s(buff, "%.2f%%", std::fabs(change));
		displayFluctuation = CCommon::StrToUnicode(buff);
	}
}

void STOCK::StockInfo::LoadMG(std::vector<std::string> data, size_t size)
{
	if (size < _DATA_LEN_MG)
	{
		return;
	}
	displayName = CCommon::StrToUnicode(data[0].c_str());
	openPrice = { convert<Price>(data[5]) };
	prevClosePrice = { convert<Price>(data[26]) };
	currentPrice = { convert<Price>(data[1]) };
	highPrice = { convert<Price>(data[6]) };
	lowPrice = { convert<Price>(data[7]) };
	volume = { convert<Volume>(data[10]) };
	// turnover = {convert<Price>(data[5])};
}

void STOCK::StockInfo::LoadAG(std::vector<std::string> data, size_t size)
{
	displayName = CCommon::StrToUnicode(data[0].c_str());
	openPrice = { convert<Price>(data[1]) };
	prevClosePrice = { convert<Price>(data[2]) };
	currentPrice = { convert<Price>(data[3]) };
	highPrice = { convert<Price>(data[4]) };
	lowPrice = { convert<Price>(data[5]) };
	volume = { convert<Volume>(data[8]) };
	turnover = { convert<Amount>(data[9]) };

	Price upperLimit = abs(highPrice - prevClosePrice);
	Price lowerLimit = abs(lowPrice - prevClosePrice);

	priceLimit = max(upperLimit, lowerLimit);

	askLevels[4] = { {convert<Price>(data[29])}, {convert<Volume>(data[28])} };
	askLevels[3] = { {convert<Price>(data[27])}, {convert<Volume>(data[26])} };
	askLevels[2] = { {convert<Price>(data[25])}, {convert<Volume>(data[24])} };
	askLevels[1] = { {convert<Price>(data[23])}, {convert<Volume>(data[22])} };
	askLevels[0] = { {convert<Price>(data[21])}, {convert<Volume>(data[20])} };

	bidLevels[0] = { {convert<Price>(data[11])}, {convert<Volume>(data[10])} };
	bidLevels[1] = { {convert<Price>(data[13])}, {convert<Volume>(data[12])} };
	bidLevels[2] = { {convert<Price>(data[15])}, {convert<Volume>(data[14])} };
	bidLevels[3] = { {convert<Price>(data[17])}, {convert<Volume>(data[16])} };
	bidLevels[4] = { {convert<Price>(data[19])}, {convert<Volume>(data[18])} };
}

void STOCK::StockInfo::LoadSH(std::vector<std::string> data, size_t size)
{
	if (size < _DATA_LEN_SH)
	{
		return;
	}
	LoadAG(data, size);
}

void STOCK::StockInfo::LoadSZ(std::vector<std::string> data, size_t size)
{
	if (size < _DATA_LEN_SZ)
	{
		return;
	}
	LoadAG(data, size);
}

void STOCK::StockInfo::LoadBJ(std::vector<std::string> data, size_t size)
{
	if (size < _DATA_LEN_BJ)
	{
		return;
	}
	LoadAG(data, size);
}

void STOCK::StockInfo::LoadHK(std::vector<std::string> data, size_t size)
{
	if (size < _DATA_LEN_HK)
	{
		return;
	}
	displayName = CCommon::StrToUnicode(data[0].c_str());
	openPrice = { convert<Price>(data[2]) };
	prevClosePrice = { convert<Price>(data[3]) };
	currentPrice = { convert<Price>(data[6]) };
	highPrice = { convert<Price>(data[4]) };
	lowPrice = { convert<Price>(data[5]) };
	volume = { convert<Volume>(data[12]) };
	turnover = { convert<Price>(data[11]) };
}

void STOCK::StockInfo::LoadNF(std::vector<std::string> data, size_t size)
{
	if (size < _DATA_LEN_NF)
	{
		return;
	}
	/* 解析格式，与股票略有不同
	var hq_str_V2201="PVC2201,230000,
	8585.00, 8692.00, 8467.00, 8641.00, // params[2,3,4,5] 开，高，低，昨收
	8673.00, 8674.00, // params[6, 7] 买一、卖一价
	8675.00, // 现价 params[8]
	8630.00, // 均价
	8821.00, // 昨日结算价【一般软件的行情涨跌幅按这个价格显示涨跌幅】（后续考虑配置项，设置按收盘价还是结算价显示涨跌幅）
	109, // 买一量
	2, // 卖一量
	289274, // 持仓量
	230643, //总量
	连, // params[8 + 7] 交易所名称 ["连","沪", "郑"]
	PVC,2021-11-26,1,9243.000,8611.000,9243.000,8251.000,9435.000,8108.000,13380.000,8108.000,445.541";
	*/
	displayName = CCommon::StrToUnicode(data[0].c_str());
	openPrice = { convert<Price>(data[2]) };
	highPrice = { convert<Price>(data[3]) };
	lowPrice = { convert<Price>(data[4]) };
	prevClosePrice = { convert<Price>(data[10]) };  // 昨结算
	currentPrice = { convert<Price>(data[8]) };
	volume = { convert<Volume>(data[14]) };
}

void STOCK::StockInfo::LoadHF(std::vector<std::string> data, size_t size)
{
	if (size < _DATA_LEN_HF)
	{
		return;
	}
	// 海外期货格式
	// 0 当前价格
	// '105.306', '',
	//  2  买一价  3 卖一价  4  最高价   5 最低价
	// '105.270', '105.290', '105.540', '102.950',
	//  6 时间   7 昨日结算价  8 开盘价  9 持仓量
	// '15:51:34', '102.410', '103.500', '250168.000',
	// 10 买 11 卖 12 日期      13 名称  14 成交量
	// '5', '2', '2022-05-04', 'WTI纽约原油2206', '28346'
	currentPrice = { convert<Price>(data[0]) };
	highPrice = { convert<Price>(data[4]) };
	lowPrice = { convert<Price>(data[5]) };
	prevClosePrice = { convert<Price>(data[7]) };
	openPrice = { convert<Price>(data[8]) };
	displayName = CCommon::StrToUnicode(data[13].c_str());
	// The overseas futures name field (data[13]) may contain additional information in parentheses,
	// e.g. "伦敦金（现货黄金）". To keep the display name concise and consistent with other formats,
	// we strip any characters after '（', retaining only the main contract name.
	displayName = displayName.substr(0, displayName.find(L"（"));

	if (data.size() >= 15)
		volume = { convert<Volume>(data[14]) };
}

CString StockInfo::GetStockListName() const
{
	// 转成 CString 方便字符串操作
	CString fullName = displayName.c_str();
	CString shortName;

	if (fullName.Find(L"XD", 0) == 0 || fullName.Find(L"XR", 0) == 0 || fullName.Find(L"DR", 0) == 0)
	{
		// 从索引6开始，截取2个字符（红利低波ETF 共6个字符）
		fullName = fullName.Mid(2);
	}
	if (fullName.Find(L"HSTE", 0) == 0)
	{
		fullName = L"恒生科技";
	}

	int pos = fullName.Find(_T("ETF"));
	if (pos != -1)
	{
		shortName = fullName.Mid(pos, 5); // 从pos开始，取5位
	}
	else if (fullName.Find(L"沪深", 0) == 0)
	{
		// 从索引6开始，截取2个字符（红利低波ETF 共6个字符）
		shortName = fullName;
	}
	// 规则3：都不满足 → 取最前面2个字
	else
	{
		shortName = fullName.Left(4);
	}

	// 安全兜底：防止名称长度不足，返回空时给默认值
	if (shortName.IsEmpty())
	{
		shortName = L"--";
	}

	return shortName;
}

CString StockInfo::GetStockShortName()
{
	// 转成 CString 方便字符串操作
	CString fullName = displayName.c_str();
	CString shortName;

	if (fullName.Find(L"XD", 0) == 0 || fullName.Find(L"XR", 0) == 0 || fullName.Find(L"DR", 0) == 0)
	{
		// 从索引6开始，截取2个字符（红利低波ETF 共6个字符）
		fullName = fullName.Mid(2);
	}

	// 规则1：开头是 "中国" → 取后面2个字
	if (fullName.Find(L"中国", 0) == 0)
	{
		// 从索引2开始，截取2个字符
		shortName = fullName.Mid(2, 2);
	}
	// 规则2：开头是 "红利低波ETF" → 取后面2个字
	else if (fullName.Find(L"红利低波ETF", 0) == 0)
	{
		// 从索引6开始，截取2个字符（红利低波ETF 共6个字符）
		shortName = fullName.Mid(7, 2);
	}
	// 规则3：都不满足 → 取最前面2个字
	else
	{
		shortName = fullName.Left(2);
	}

	// 安全兜底：防止名称长度不足，返回空时给默认值
	if (shortName.IsEmpty())
	{
		shortName = L"--";
	}

	return shortName;
}

void STOCK::StockMarket::LoadTimelineDataByJson(std::wstring stock_id, CString* pData)
{
	auto data = g_data.GetStockData(stock_id);
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		data->clearTimelinePoint();
		if (pData)
		{
			data->addTimelinePoint(*pData);
		}
	}
	Stock::Instance().UpdateKLine();
}

std::wstring STOCK::StockData::GetCurrentDisplay(bool include_name) const
{
	std::wstringstream wss;
	if (info.is_ok)
	{
		if (include_name)
			wss << info.displayName << ": ";
		wss << info.displayPrice << ' ' << info.displayFluctuationDiff << info.displayFluctuation;
	}
	else
	{
		wss << info.code + L" " + g_data.StringRes(IDS_LOAD_FAIL).GetString();
	}
	return wss.str();
}

static Volume GetJsonVolume(yyjson_val* obj, const char* key)
{
	if (obj != nullptr)
	{
		yyjson_val* val = yyjson_obj_get(obj, key);
		try
		{
			if (val != nullptr)
			{
				if (yyjson_is_uint(val))
				{
					return static_cast<Volume>(yyjson_get_uint(val));
				}
				else if (yyjson_is_str(val))
				{
					return static_cast<Volume>(std::stoul(yyjson_get_str(val)));
				}
			}
		}
		catch (const std::exception& e)
		{
			return 0L;
		}
	}
	return 0L;
}

static Price GetJsonPrice(yyjson_val* obj, const char* key)
{
	if (obj != nullptr)
	{
		yyjson_val* val = yyjson_obj_get(obj, key);
		try
		{
			if (val != nullptr)
			{
				if (yyjson_is_real(val))
				{
					return static_cast<Price>(yyjson_get_real(val));
				}
				else if (yyjson_is_str(val))
				{
					return static_cast<Price>(std::stod(yyjson_get_str(val)));
				}
			}
		}
		catch (const std::invalid_argument& e)
		{
			return 0.0F;
		}
		catch (const std::out_of_range& e)
		{
			return 0.0F;
		}
	}
	return 0.0F;
}

void STOCK::StockData::addTimelinePoint(const CString& json_data)
{
	std::string _json_data = CCommon::UnicodeToStr(json_data);
	yyjson_doc* doc = yyjson_read(_json_data.c_str(), _json_data.size(), 0);
	if (doc != nullptr)
	{
		yyjson_val* root = yyjson_doc_get_root(doc);
		if (root == nullptr)
		{
			return;
		}

		yyjson_val* result = yyjson_obj_get(root, "result");
		if (result == nullptr)
		{
			return;
		}

		yyjson_val* data = yyjson_obj_get(result, "data");
		if (data != nullptr && yyjson_is_arr(data))
		{
			yyjson_val* item;
			yyjson_arr_iter iter;
			yyjson_arr_iter_init(data, &iter);
			while ((item = yyjson_arr_iter_next(&iter)))
			{
				if (item != nullptr)
				{
					TimelinePoint point = TimelinePoint();
					point.time = utilities::JsonHelper::GetJsonString(item, "m");
					point.volume = GetJsonVolume(item, "v");
					point.price = GetJsonPrice(item, "p");
					point.averagePrice = GetJsonPrice(item, "avg_p");
					point.amount = point.price * point.volume;
					addTimelinePoint(point);
				}
			}
		}
	}
}

void STOCK::StockData::addKLineData(const CString& json_data)
{
	std::string _json_data = CCommon::UnicodeToStr(json_data);
	yyjson_doc* doc = yyjson_read(_json_data.c_str(), _json_data.size(), 0);
	if (doc == nullptr) return;

	yyjson_val* root = yyjson_doc_get_root(doc);
	if (root == nullptr || !yyjson_is_arr(root))
	{
		yyjson_doc_free(doc);
		return;
	}

	clearKLineData();

	yyjson_val* item;
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(root, &iter);
	while ((item = yyjson_arr_iter_next(&iter)))
	{
		if (item != nullptr && yyjson_is_obj(item))
		{
			KLinePoint point;
			point.day = utilities::JsonHelper::GetJsonString(item, "day");
			point.open = GetJsonPrice(item, "open");
			point.high = GetJsonPrice(item, "high");
			point.low = GetJsonPrice(item, "low");
			point.close = GetJsonPrice(item, "close");
			point.volume = GetJsonVolume(item, "volume");
			addKLinePoint(point);
		}
	}
	yyjson_doc_free(doc);
}

void STOCK::StockMarket::LoadKLineDataByJson(std::wstring stock_id, CString* pData)
{
	auto data = g_data.GetStockData(stock_id);
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		data->clearKLineData();
		if (pData)
		{
			data->addKLineData(*pData);
		}
	}
	Stock::Instance().UpdateKLine();
}

void STOCK::StockData::addMin5KLineData(const CString& json_data)
{
	std::string _json_data = CCommon::UnicodeToStr(json_data);
	yyjson_doc* doc = yyjson_read(_json_data.c_str(), _json_data.size(), 0);
	if (doc == nullptr) return;

	yyjson_val* root = yyjson_doc_get_root(doc);
	if (root == nullptr || !yyjson_is_arr(root))
	{
		yyjson_doc_free(doc);
		return;
	}

	clearMin5KLineData();

	yyjson_val* item;
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(root, &iter);
	while ((item = yyjson_arr_iter_next(&iter)))
	{
		if (item != nullptr && yyjson_is_obj(item))
		{
			KLinePoint point;
			point.day = utilities::JsonHelper::GetJsonString(item, "day");
			point.open = GetJsonPrice(item, "open");
			point.high = GetJsonPrice(item, "high");
			point.low = GetJsonPrice(item, "low");
			point.close = GetJsonPrice(item, "close");
			point.volume = GetJsonVolume(item, "volume");
			addMin5KLinePoint(point);
		}
	}
	yyjson_doc_free(doc);
}

void STOCK::StockData::addMin30KLineData(const CString& json_data)
{
	std::string _json_data = CCommon::UnicodeToStr(json_data);
	yyjson_doc* doc = yyjson_read(_json_data.c_str(), _json_data.size(), 0);
	if (doc == nullptr) return;

	yyjson_val* root = yyjson_doc_get_root(doc);
	if (root == nullptr || !yyjson_is_arr(root))
	{
		yyjson_doc_free(doc);
		return;
	}

	clearMin30KLineData();

	yyjson_val* item;
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(root, &iter);
	while ((item = yyjson_arr_iter_next(&iter)))
	{
		if (item != nullptr && yyjson_is_obj(item))
		{
			KLinePoint point;
			point.day = utilities::JsonHelper::GetJsonString(item, "day");
			point.open = GetJsonPrice(item, "open");
			point.high = GetJsonPrice(item, "high");
			point.low = GetJsonPrice(item, "low");
			point.close = GetJsonPrice(item, "close");
			point.volume = GetJsonVolume(item, "volume");
			addMin30KLinePoint(point);
		}
	}
	yyjson_doc_free(doc);
}

void STOCK::StockMarket::LoadMin5KLineDataByJson(std::wstring stock_id, CString* pData)
{
	auto data = g_data.GetStockData(stock_id);
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		data->clearMin5KLineData();
		if (pData)
		{
			data->addMin5KLineData(*pData);
		}
	}
	Stock::Instance().UpdateKLine();
}

void STOCK::StockMarket::LoadMin30KLineDataByJson(std::wstring stock_id, CString* pData)
{
	auto data = g_data.GetStockData(stock_id);
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		data->clearMin30KLineData();
		if (pData)
		{
			data->addMin30KLineData(*pData);
		}
	}
	Stock::Instance().UpdateKLine();
}

// ========== KLineData 计算方法实现 ==========

double STOCK::KLineData::CalculateMA(int period) const
{
	if (data.empty() || period <= 0)
		return 0;

	int startIdx = max(0, static_cast<int>(data.size()) - period);
	double sum = 0;
	int count = 0;
	for (int i = startIdx; i < data.size(); i++)
	{
		sum += data[i].close;
		count++;
	}
	return count > 0 ? sum / count : 0;
}

double STOCK::KLineData::CalculateMAPeriod(int days, int totalDays) const
{
	if (data.empty() || days <= 0)
		return 0;

	const int DAYS_PER_YEAR = 250;
	int rangeEnd = static_cast<int>(data.size()) - (days - 1) * DAYS_PER_YEAR;
	int rangeStart = max(0, static_cast<int>(data.size()) - days * DAYS_PER_YEAR);

	if (rangeStart >= rangeEnd)
		return 0;

	double sum = 0;
	int count = 0;
	for (int i = rangeStart; i < rangeEnd; i++)
	{
		sum += data[i].close;
		count++;
	}
	return count > 0 ? sum / count : 0;
}

double STOCK::KLineData::CalculateAverageAmplitude(int days) const
{
	if (data.empty() || days <= 0)
		return 0;

	int klineSize = static_cast<int>(data.size());
	int startIdx = max(1, klineSize - days);
	double totalAmplitude = 0;
	int count = 0;

	for (int i = startIdx; i < klineSize; i++)
	{
		double prevClose = data[i - 1].close;
		if (prevClose <= 0)
			continue;
		double amplitude = (data[i].high - data[i].low) / prevClose * 100;
		totalAmplitude += amplitude;
		count++;
	}
	return count > 0 ? totalAmplitude / count : 0;
}

double STOCK::KLineData::GetMaxPrice(int days) const
{
	if (data.empty() || days <= 0)
		return 0;

	int startIdx = max(0, static_cast<int>(data.size()) - days);
	double maxPrice = 0;
	for (int i = startIdx; i < data.size(); i++)
	{
		if (data[i].high > maxPrice)
			maxPrice = data[i].high;
	}
	return maxPrice;
}

double STOCK::KLineData::GetMinPrice(int days) const
{
	if (data.empty() || days <= 0)
		return 0;

	int startIdx = max(0, static_cast<int>(data.size()) - days);
	double minPrice = (std::numeric_limits<double>::max)();
	for (int i = startIdx; i < data.size(); i++)
	{
		if (data[i].low < minPrice)
			minPrice = data[i].low;
	}
	return minPrice;
}

double STOCK::KLineData::GetAverageClosePrice(int days) const
{
	if (data.empty() || days <= 0)
		return 0;

	int startIdx = max(0, static_cast<int>(data.size()) - days);
	double sum = 0;
	int count = 0;
	for (int i = startIdx; i < data.size(); i++)
	{
		sum += data[i].close;
		count++;
	}
	return count > 0 ? sum / count : 0;
}

STOCK::KLineData::PeriodStats STOCK::KLineData::GetPeriodStats(int days) const
{
	PeriodStats stats;
	if (data.empty() || days <= 0)
		return stats;

	const int DAYS_PER_YEAR = 250;
	int startIdx = max(0, static_cast<int>(data.size()) - days * DAYS_PER_YEAR);
	int endIdx = static_cast<int>(data.size()) - 1;

	double maxPrice = 0;
	double minPrice = (std::numeric_limits<double>::max)();
	double sumPrice = 0;
	int count = 0;

	for (int i = startIdx; i <= endIdx; i++)
	{
		if (data[i].high > maxPrice)
		{
			maxPrice = data[i].high;
			stats.maxDate = data[i].day;
		}
		if (data[i].low < minPrice)
		{
			minPrice = data[i].low;
			stats.minDate = data[i].day;
		}
		sumPrice += data[i].close;
		count++;
	}

	if (count == 0)
		return stats;

	stats.maxPrice = maxPrice;
	stats.minPrice = minPrice;
	stats.avgPrice = sumPrice / count;
	stats.isValid = true;
	return stats;
}

// ========== StockData 持仓计算方法实现 ==========

STOCK::StockData::PositionInfo STOCK::StockData::CalculatePositionInfo(double costPrice, double holdingCount) const
{
	PositionInfo result;

	if (costPrice <= 0 || holdingCount <= 0 || info.currentPrice <= 0)
		return result;

	result.totalCost = costPrice * holdingCount;
	result.marketValue = info.currentPrice * holdingCount;
	result.profitLoss = result.marketValue - result.totalCost;
	result.profitLossPercent = result.totalCost != 0 ? (result.profitLoss / result.totalCost) * 100 : 0;
	result.isValid = true;

	if (info.prevClosePrice != 0)
	{
		result.todayProfitLoss = (info.currentPrice - info.prevClosePrice) * holdingCount;
		result.todayProfitLossPercent = (info.currentPrice - info.prevClosePrice) / info.prevClosePrice * 100;
	}

	return result;
}

double STOCK::StockData::CalculateAnnualizedReturn(double costPrice, double holdingCount, const std::wstring& buyDate) const
{
	if (costPrice <= 0 || holdingCount <= 0 || info.currentPrice <= 0 || buyDate.empty())
		return 0;

	double totalCost = costPrice * holdingCount;
	double marketValue = info.currentPrice * holdingCount;
	double profitLoss = marketValue - totalCost;
	double profitLossPercent = totalCost != 0 ? (profitLoss / totalCost) * 100 : 0;

	int year = 0, month = 0, day = 0;
	if (swscanf_s(buyDate.c_str(), L"%d-%d-%d", &year, &month, &day) != 3)
		return 0;

	std::tm tm = { 0 };
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;

	std::time_t buyTime = std::mktime(&tm);
	std::time_t now = std::time(nullptr);
	double daysHeld = std::difftime(now, buyTime) / (60 * 60 * 24);

	if (daysHeld <= 0)
		return 0;

	return (profitLossPercent / daysHeld) * 365;
}