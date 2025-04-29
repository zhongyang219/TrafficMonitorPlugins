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

    if (item_arr.size() < 2)
    {
      stockData->info.displayName = stockData->info.code + L" 获取失败";

      CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
      continue;
    }

    std::string data = item_arr[1];

    std::vector<std::string> data_arr = CCommon::split(data, ",");

    stockData->info.displayName = CCommon::StrToUnicode(data_arr[0].c_str());

    stockData->realTimeData.Load(key, data_arr);
  }
}

void STOCK::RealTimeData::Load(std::wstring key, std::vector<std::string> data_arr)
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

  if (currentPrice > 0 && prevClosePrice > 0)
  {
    char buff[32];
    sprintf_s(buff, "%.2f", currentPrice);
    displayPrice = CCommon::StrToUnicode(buff);

    sprintf_s(buff, "%.2f%%", ((currentPrice - prevClosePrice) / prevClosePrice * 100));
    displayFluctuation = CCommon::StrToUnicode(buff);
  }
}

void STOCK::RealTimeData::LoadMG(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_MG)
  {
    return;
  }
  openPrice = {convert<Price>(data[5])};
  prevClosePrice = {convert<Price>(data[26])};
  currentPrice = {convert<Price>(data[1])};
  highPrice = {convert<Price>(data[6])};
  lowPrice = {convert<Price>(data[7])};
  volume = {convert<Volume>(data[10])};
  // turnover = {convert<Price>(data[5])};
}

void STOCK::RealTimeData::LoadAG(std::vector<std::string> data, size_t size)
{
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

void STOCK::RealTimeData::LoadSH(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_SH)
  {
    return;
  }
  LoadAG(data, size);
}

void STOCK::RealTimeData::LoadSZ(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_SZ)
  {
    return;
  }
  LoadAG(data, size);
}

void STOCK::RealTimeData::LoadBJ(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_BJ)
  {
    return;
  }
  LoadAG(data, size);
}

void STOCK::RealTimeData::LoadHK(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_HK)
  {
    return;
  }
  openPrice = {convert<Price>(data[2])};
  prevClosePrice = {convert<Price>(data[3])};
  currentPrice = {convert<Price>(data[6])};
  highPrice = {convert<Price>(data[4])};
  lowPrice = {convert<Price>(data[5])};
  volume = {convert<Volume>(data[12])};
  turnover = {convert<Price>(data[11])};
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
  if (include_name)
    wss << info.displayName << ": ";
  wss << realTimeData.displayPrice << ' ' << realTimeData.displayFluctuation;
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
