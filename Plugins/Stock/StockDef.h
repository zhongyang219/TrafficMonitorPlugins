#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <iostream>

// 使用命名空间避免名称冲突
namespace STOCK
{

  // 价格
  using Price = double;
  // 数量
  using Volume = long long;
  // 金额
  using Amount = double;
  using TimePoint = std::string;
  // using TimePoint = long long;
  // using Duration = std::chrono::duration<std::chrono::system_clock>;

  // 买卖盘信息
  struct OrderLevel
  {
    Price price;   // 价格
    Volume volume; // 数量

    OrderLevel() : price(0.0), volume(0) {}
    OrderLevel(Price p, Volume v) : price(p), volume(v) {}
  };

  // 最新交易数据
  struct RealTimeData
  {
    Price openPrice;      // 今日开盘价
    Price prevClosePrice; // 昨日收盘价
    Price currentPrice;   // 当前价格
    Price highPrice;      // 最高价
    Price lowPrice;       // 最低价
    Volume volume;        // 成交量(股)
    Amount turnover;      // 成交额(元)

    std::wstring displayPrice = L"--";
    std::wstring displayFluctuation = L"--%";

    Price priceLimit; // 价格限制

    // 买卖盘口，使用数组便于遍历
    static const int MAX_LEVEL = 5;

    OrderLevel askLevels[MAX_LEVEL]; // 卖盘(5档)
    OrderLevel bidLevels[MAX_LEVEL]; // 买盘(5档)

    RealTimeData() : openPrice(0.0),
                     prevClosePrice(0.0),
                     currentPrice(0.0),
                     highPrice(0.0),
                     lowPrice(0.0),
                     volume(0),
                     turnover(0.0),
                     priceLimit(0.0)
    {
      // bidLevels.resize(MAX_LEVEL);
      // askLevels.resize(MAX_LEVEL);
    }

    void Load(std::wstring key, std::vector<std::string> data);
    void LoadMG(std::vector<std::string> data, size_t size);
    void LoadAG(std::vector<std::string> data, size_t size);
    void LoadSH(std::vector<std::string> data, size_t size);
    void LoadSZ(std::vector<std::string> data, size_t size);
    void LoadBJ(std::vector<std::string> data, size_t size);
    void LoadHK(std::vector<std::string> data, size_t size);
  };

  // 分时数据点
  struct TimelinePoint
  {
    TimePoint time;     // 时间点
    Volume volume;      // 成交量
    Price price;        // 价格
    Price averagePrice; // 均价

    TimelinePoint() : volume(0), price(0.0), averagePrice(0.0) {}
    TimelinePoint(const TimePoint &time, Volume vol, Price p, Price avg) : time(time), volume(vol), price(p), averagePrice(avg) {}
  };

  // // K线数据点
  // struct KLinePoint
  // {
  //   TimePoint time;   // 时间戳
  //   Price openPrice;  // 开盘价
  //   Price closePrice; // 收盘价
  //   Price highPrice;  // 最高价
  //   Price lowPrice;   // 最低价
  //   Volume volume;    // 成交量
  //   Amount turnover;  // 成交额

  //   KLinePoint() : openPrice(0.0),
  //                  closePrice(0.0),
  //                  highPrice(0.0),
  //                  lowPrice(0.0),
  //                  volume(0),
  //                  turnover(0.0) {}
  // };

  // 定义不同的数据周期
  enum class Period
  {
    TIMELINE, // 分时
    MIN1,     // 1分钟
    MIN5,     // 5分钟
    MIN15,    // 15分钟
    MIN30,    // 30分钟
    HOUR1,    // 1小时
    DAY,      // 日线
    WEEK,     // 周线
    MONTH,    // 月线
    YEAR      // 年线
  };

  // 历史数据基类
  class HistoricalDataBase
  {
  public:
    virtual ~HistoricalDataBase() = default;
    virtual Period GetPeriod() const = 0;
    virtual TimePoint GetStartTime() const = 0;
    virtual TimePoint GetEndTime() const = 0;
  };

  // 分时历史数据
  class TimelineData : public HistoricalDataBase
  {
  public:
    std::vector<TimelinePoint> data;
    TimelineData() {};
    Period GetPeriod() const override { return Period::TIMELINE; }
    TimePoint GetStartTime() const { return data.front().time; }
    TimePoint GetEndTime() const { return data.back().time; }
    void Clear() { data.clear(); }
  };

  // // K线历史数据
  // class KLineData : public HistoricalDataBase
  // {
  // public:
  //   Period period;
  //   std::vector<KLinePoint> data;
  //   Period GetPeriod() const override { return period; }
  //   TimePoint GetStartTime() const override { return data.front().time; }
  //   TimePoint GetEndTime() const override { return data.back().time; }
  // };

  // 股票基础信息
  struct StockInfo
  {
    std::wstring code;        // 股票代码
    std::wstring displayName; // 股票名称
    // std::string industry; // 所属行业
  };

  // 股票数据结构
  class StockData
  {
  public:
    StockInfo info;            // 基础信息
    RealTimeData realTimeData; // 最新数据

    std::wstring GetCurrentDisplay(bool include_name = true) const;

    // 使用智能指针管理历史数据
    std::map<Period, std::shared_ptr<HistoricalDataBase>> historicalData;

    template <typename T>
    std::shared_ptr<T> MakesureHistoricalData(Period period)
    {
      auto it = historicalData.find(period);
      if (it != historicalData.end())
      {
        auto _data = std::dynamic_pointer_cast<T>(it->second);
        if (_data)
        {
          return _data;
        }
      }
      auto _data = std::make_shared<T>();
      historicalData[period] = _data;
      return _data;
    }

    void clearTimelinePoint()
    {
      auto timelineData = MakesureHistoricalData<TimelineData>(Period::TIMELINE);
      timelineData->Clear();
    }

    // 添加分时数据点
    void addTimelinePoint(const TimelinePoint &point)
    {
      auto timelineData = MakesureHistoricalData<TimelineData>(Period::TIMELINE);
      timelineData->data.push_back(point);
    }

    void addTimelinePoint(const CString &json_data);

    // 获取分时走势数据
    STOCK::TimelineData *getTimelineData()
    {
      return MakesureHistoricalData<TimelineData>(Period::TIMELINE).get();
    }
  };

  // 股票市场类，管理多个股票
  class StockMarket
  {
  private:
    std::map<std::wstring, std::shared_ptr<StockData>> stocks; // 以股票代码为键的股票数据映射

  public:
    void LoadRealtimeDataByJson(std::string data);
    void LoadTimelineDataByJson(std::wstring stock_id, CString *data);

    void ClearRealtimeData()
    {
      for (const auto &it : stocks)
      {
        RealTimeData data;
        it.second->realTimeData = data;
      }
    }

    // 添加股票
    std::shared_ptr<StockData> addStock(const std::wstring &code)
    {
      StockData stock;
      stock.info.code = code;
      stocks[code] = std::make_shared<StockData>(stock);
      return stocks[code];
    }

    // 获取股票数据
    std::shared_ptr<StockData> getStock(const std::wstring &code)
    {
      auto it = stocks.find(code);
      if (it != stocks.end())
      {
        return it->second;
      }
      return addStock(code);
    }

    // 更新股票最新数据
    void updateStockData(const std::wstring &code, const RealTimeData &data)
    {
      auto stock = getStock(code);
      if (stock)
      {
        stock->realTimeData = data;
      }
    }

    // 添加分时数据
    void addTimelinePoint(const std::wstring &code, const TimelinePoint &point)
    {
      auto stock = getStock(code);
      if (stock)
      {
        stock->addTimelinePoint(point);
      }
    }

    void addTimelinePoint(const std::wstring &code, const CString &json_data)
    {
      auto stock = getStock(code);
      if (stock)
      {
        stock->addTimelinePoint(json_data);
      }
    }
  };

  // 数据更新接口
  // class IDataUpdateHandler
  // {
  // public:
  //   virtual ~IDataUpdateHandler() = default;
  //   virtual void OnRealTimeUpdate(const RealTimeData &data) = 0;
  //   virtual void OnTimelineUpdate(const TimelinePoint &point) = 0;
  //   virtual void OnKLineUpdate(Period period, const KLinePoint &point) = 0;
  // };

} // namespace STOCK

// // 使用示例
// int main()
// {
//   using namespace STOCK;

//   // 创建股票市场
//   STOCK::StockMarket market;

//   // 添加股票
//   auto stock = market.addStock("000759");
//   if (!stock)
//   {
//     std::cerr << "获取股票数据失败" << std::endl;
//     return 1;
//   }

//   // 创建并更新最新数据
//   RealTimeData realTimeData;
//   realTimeData.openPrice = 10.5;
//   realTimeData.prevClosePrice = 10.4;
//   realTimeData.currentPrice = 10.6;
//   realTimeData.highPrice = 10.7;
//   realTimeData.lowPrice = 10.5;
//   realTimeData.volume = 12345678;
//   realTimeData.turnover = 130010000.0;

//   // 设置买卖盘数据
//   realTimeData.bidLevels[0] = OrderLevel(10.59, 10000);
//   realTimeData.bidLevels[1] = OrderLevel(10.58, 20000);
//   realTimeData.bidLevels[2] = OrderLevel(10.57, 30000);
//   realTimeData.bidLevels[3] = OrderLevel(10.56, 40000);
//   realTimeData.bidLevels[4] = OrderLevel(10.55, 50000);

//   realTimeData.askLevels[0] = OrderLevel(10.61, 10000);
//   realTimeData.askLevels[1] = OrderLevel(10.62, 20000);
//   realTimeData.askLevels[2] = OrderLevel(10.63, 30000);
//   realTimeData.askLevels[3] = OrderLevel(10.64, 40000);
//   realTimeData.askLevels[4] = OrderLevel(10.65, 50000);

//   // 更新股票数据
//   stock->updateRealtimeData(realTimeData);

//   // 添加分时数据
//   auto now = std::chrono::system_clock::now();
//   TimelinePoint point1(now, 10000, 10.55, 10.525);
//   stock->addTimelinePoint(point1);

//   // 添加一个分钟后的数据
//   auto oneMinuteLater = now + std::chrono::minutes(1);
//   TimelinePoint point2(oneMinuteLater, 12000, 10.6, 10.55);
//   stock->addTimelinePoint(point2);

//   // 添加日K数据
//   auto dailyKLine = stock->MakesureHistoricalData<KLineData>(Period::DAY);

//   KLinePoint klp;
//   klp.time = now;
//   klp.openPrice = 10.5;
//   klp.closePrice = 10.6;
//   klp.highPrice = 10.7;
//   klp.lowPrice = 10.5;
//   klp.volume = 1000000;
//   klp.turnover = 10600000.0;

//   dailyKLine->data.push_back(klp);

//   // 打印股票信息
//   stock->printInfo();

//   return 0;
// }