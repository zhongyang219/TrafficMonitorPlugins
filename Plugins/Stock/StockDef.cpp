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

    // stockData->info.displayName = CCommon::StrToUnicode(data_arr[0].c_str());

    stockData->info.Load(key, data_arr);
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
    char buff[32];
    sprintf_s(buff, "%.2f", currentPrice);
    displayPrice = CCommon::StrToUnicode(buff);

    sprintf_s(buff, "%.2f%%", ((currentPrice - prevClosePrice) / prevClosePrice * 100));
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
  openPrice = {convert<Price>(data[5])};
  prevClosePrice = {convert<Price>(data[26])};
  currentPrice = {convert<Price>(data[1])};
  highPrice = {convert<Price>(data[6])};
  lowPrice = {convert<Price>(data[7])};
  volume = {convert<Volume>(data[10])};
  // turnover = {convert<Price>(data[5])};
}

void STOCK::StockInfo::LoadAG(std::vector<std::string> data, size_t size)
{
  displayName = CCommon::StrToUnicode(data[0].c_str());
  openPrice = {convert<Price>(data[1])};
  prevClosePrice = {convert<Price>(data[2])};
  currentPrice = {convert<Price>(data[3])};
  highPrice = {convert<Price>(data[4])};
  lowPrice = {convert<Price>(data[5])};
  volume = {convert<Volume>(data[8])};
  turnover = {convert<Amount>(data[9])};

  Price upperLimit = abs(highPrice - prevClosePrice);
  Price lowerLimit = abs(lowPrice - prevClosePrice);

  priceLimit = max(upperLimit, lowerLimit);

  // 设置买卖盘数据
  askLevels[4] = {{convert<Price>(data[29])}, {convert<Volume>(data[28])}};
  askLevels[3] = {{convert<Price>(data[27])}, {convert<Volume>(data[26])}};
  askLevels[2] = {{convert<Price>(data[25])}, {convert<Volume>(data[24])}};
  askLevels[1] = {{convert<Price>(data[23])}, {convert<Volume>(data[22])}};
  askLevels[0] = {{convert<Price>(data[21])}, {convert<Volume>(data[20])}};

  bidLevels[0] = {{convert<Price>(data[11])}, {convert<Volume>(data[10])}};
  bidLevels[1] = {{convert<Price>(data[13])}, {convert<Volume>(data[12])}};
  bidLevels[2] = {{convert<Price>(data[15])}, {convert<Volume>(data[14])}};
  bidLevels[3] = {{convert<Price>(data[17])}, {convert<Volume>(data[16])}};
  bidLevels[4] = {{convert<Price>(data[19])}, {convert<Volume>(data[18])}};
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
  openPrice = {convert<Price>(data[2])};
  prevClosePrice = {convert<Price>(data[3])};
  currentPrice = {convert<Price>(data[6])};
  highPrice = {convert<Price>(data[4])};
  lowPrice = {convert<Price>(data[5])};
  volume = {convert<Volume>(data[12])};
  turnover = {convert<Price>(data[11])};
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

void STOCK::StockMarket::LoadTimelineDataByJson(std::wstring stock_id, CString *pData)
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
    wss << info.displayPrice << ' ' << info.displayFluctuation;
  }
  else
  {
    wss << info.code + L" " + g_data.StringRes(IDS_LOAD_FAIL).GetString();
  }
  return wss.str();
}

static Volume GetJsonVolume(yyjson_val *obj, const char *key)
{
  if (obj != nullptr)
  {
    yyjson_val *val = yyjson_obj_get(obj, key);
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
    catch (const std::exception &e)
    {
      return 0L;
    }
  }
  return 0L;
}

static Price GetJsonPrice(yyjson_val *obj, const char *key)
{
  if (obj != nullptr)
  {
    yyjson_val *val = yyjson_obj_get(obj, key);
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
    catch (const std::invalid_argument &e)
    {
      return 0.0F;
    }
    catch (const std::out_of_range &e)
    {
      return 0.0F;
    }
  }
  return 0.0F;
}

void STOCK::StockData::addTimelinePoint(const CString &json_data)
{
  std::string _json_data = CCommon::UnicodeToStr(json_data);
  yyjson_doc *doc = yyjson_read(_json_data.c_str(), _json_data.size(), 0);
  if (doc != nullptr)
  {
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == nullptr)
    {
      return;
    }

    yyjson_val *result = yyjson_obj_get(root, "result");
    if (result == nullptr)
    {
      return;
    }

    yyjson_val *data = yyjson_obj_get(result, "data");
    if (data != nullptr && yyjson_is_arr(data))
    {
      yyjson_val *item;
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
          addTimelinePoint(point);
        }
      }
    }
  }
}
