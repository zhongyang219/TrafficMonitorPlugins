#include "pch.h"
#include "MarketOrderBook.h"
#include "StockDef.h"
#include "Common.h"
#include "DataManager.h"
#include <Stock.h>
#include <algorithm>
#include <limits>
#include <cmath>

// 将价格量化到0.0001精度作为map key，规避浮点相等比较误差
// 例如 ETF 真实价格0.59 与数据库 5.9/10=0.5900000000000001 会被归一到同一个key，避免查不到
static long long PriceToKey(double price)
{
	return static_cast<long long>(std::llround(price * 10000.0));
}

// 收到行情推送时更新盘口数据，并重算缓存指标
void MarketOrderBook::UpdateFromStockInfo(const STOCK::StockInfo& info)
{
	// 股票切换时清空明细与临时对比数据，避免变化量/明细跨股票残留
	if (info.code != m_code)
	{
		m_priceCumVolMap.clear();
		m_priceCumVolPrev.clear();
		m_transLines.clear();
		m_transCode.clear();
		m_priceCumVolCode.clear();
		m_lastCumVolRefreshTick = 0;
		m_lastTransRefreshTick = 0;
	}
	m_code = info.code;
	if (m_code.empty())
	{
		m_valid = false;
		return;
	}

	m_valid = true;
	m_isEtf = info.IsETF();
	m_currentPrice = info.currentPrice;
	m_high = info.highPrice;
	m_low = info.lowPrice;
	m_prevClose = info.prevClosePrice;
	m_iopv = info.iopv;
	m_maxOrderVol = 0;
	for (int i = 0; i < 5; ++i)
	{
		m_ask[i].price = info.askLevels[i].price;
		m_ask[i].volume = info.askLevels[i].volume;
		m_bid[i].price = info.bidLevels[i].price;
		m_bid[i].volume = info.bidLevels[i].volume;
		if (info.askLevels[i].volume > m_maxOrderVol)
			m_maxOrderVol = info.askLevels[i].volume;
		if (info.bidLevels[i].volume > m_maxOrderVol)
			m_maxOrderVol = info.bidLevels[i].volume;
	}

	// 预格式化最高/最低行文本（含与现价的差值，符号在更新时确定）
	double highDiff = info.highPrice - info.currentPrice;
	double lowDiff = info.lowPrice - info.currentPrice;
	CString highSign = highDiff >= 0 ? _T("+") : _T("");
	CString lowSign = lowDiff >= 0 ? _T("+") : _T("");
	m_highText.Format(_T("H:%s %s%s"), CCommon::FormatFloat(info.highPrice),
		highSign.GetString(), CCommon::FormatFloat(highDiff));
	m_lowText.Format(_T("L:%s %s%s"), CCommon::FormatFloat(info.lowPrice),
		lowSign.GetString(), CCommon::FormatFloat(lowDiff));

	// 1分钟加权平均净比（来自StockData::secVolumePool），在Update时一次性读取缓存
	m_hasNetRatio = false;
	m_netRatio = 0;
	auto stockData = g_data.GetStockData(info.code);
	if (stockData)
	{
		STOCK::Volume diff = 0;
		if (stockData->GetSecNetDiff(1, diff, m_netRatio))
			m_hasNetRatio = true;
	}
}

// 从数据库刷新成交相关缓存（渲染层不再查库，由本函数内部节流）：
// 1. 各价格档位累计成交量（同一股票2秒节流）
// 2. 最近10条成交明细（同一股票3秒节流）
// 末尾统一重算累计/区间净流入
void MarketOrderBook::RefreshTransCache()
{
	if (!m_valid || m_code.empty() || !g_data.GetDbManager().IsOpen())
		return;

	DWORD now = GetTickCount();
	bool isEtf = m_isEtf;

	// ---- 1. 各价格档位累计成交量（2秒节流）----
	if (m_code != m_priceCumVolCode || now - m_lastCumVolRefreshTick >= 2000)
	{
		if (m_code != m_priceCumVolCode)
		{
			m_priceCumVolMap.clear();
			m_priceCumVolPrev.clear();
		}
		m_priceCumVolCode = m_code;
		m_lastCumVolRefreshTick = now;

		std::string tradeDay = CCommon::get_stock_trade_date();
		auto stats = g_data.GetDbManager().LoadPriceVolumeStats(m_code, tradeDay);

		// 保存本次采样前的累计值，用于计算瞬时变化量（仅首次采样时 prev 为空，变化量显示0）
		m_priceCumVolPrev = m_priceCumVolMap;
		m_priceCumVolMap.clear();

		for (const auto& s : stats)
		{
			// s.buyOrSell: 1=主动卖, 0=主动买
			double realPrice = isEtf ? (s.price / 10.0) : s.price;
			long long key = PriceToKey(realPrice);
			if (s.buyOrSell == 0)
				m_priceCumVolMap[key].activeBuyVol = s.vol;    // 主动买
			else if (s.buyOrSell == 1)
				m_priceCumVolMap[key].activeSellVol = s.vol;   // 主动卖
		}
	}

	// ---- 2. 最近10条成交明细（3秒节流）----
	if (m_code != m_transCode || now - m_lastTransRefreshTick >= 3000)
	{
		m_transCode = m_code;
		m_lastTransRefreshTick = now;
		m_transLines.clear();
		auto raw = g_data.GetDbManager().LoadLatestTransactions(m_code, 20);
		for (auto& t : raw)
		{
			// ETF 成交价格在DB中被Python放大了10倍，此处除以10还原
			if (isEtf)
				t.price = t.price / 10.0;
			TransLine line;
			line.timeKey = CCommon::StrToUnicode(t.timeKey.c_str());
			line.price = t.price;
			line.vol = t.vol;
			line.buyOrSell = t.buyOrSell;
			m_transLines.push_back(std::move(line));
		}
	}

	// 数据变化后统一重算净流入（节流周期内重复调用无开销，仅map遍历）
	CalcCumNetInflow();
	CalcIntervalNetInflow();

	CheckNegativeRemainRules();
}

// 排队规则成交检查：剩余排队量变负 → 该单已完全成交，删除内存规则与DB记录
// （挂在 RefreshTransCache 尾部，累计成交量map刚刷新后的调用点，同一数据无需重查）
void MarketOrderBook::CheckNegativeRemainRules()
{
	if (m_code.empty())
		return;
	auto stockData = g_data.GetStockData(m_code);
	if (!stockData || stockData->estimateRuleMap.empty())
		return;

	std::vector<long long> toDelete;
	for (const auto& kv : stockData->estimateRuleMap)
	{
		const STOCK::VolumeOrderEstimate& rule = kv.second;
		bool isAskSide = (rule.tradeType == 1);
		double price = kv.first / 10000.0;   // 量化key还原

		// 与 GetRemainQueueAt 相同口径：min(挂单基准, 快照最低, 实时挂单) + 买入累计 - 当前累计
		long long remain = GetRemainQueueAt(price, isAskSide);
		if (remain != (std::numeric_limits<long long>::min)() && remain < 0)
			toDelete.push_back(kv.first);
	}

	if (toDelete.empty())
		return;

	// 删除内存规则与DB记录。调用点（RefreshTransCache→OnPaint）已持有 m_stockDataMutex，
	// std::mutex 不可重入，此处不能再 lock——直接操作即可。
	std::string tradeDate = CCommon::get_stock_trade_date();
	for (long long priceKey : toDelete)
	{
		auto it = stockData->estimateRuleMap.find(priceKey);
		if (it == stockData->estimateRuleMap.end())
			continue;
		double price = priceKey / 10000.0;
		g_data.GetDbManager().DeleteOrderBookEstimates(m_code, tradeDate, price, it->second.tradeType);
		stockData->estimateRuleMap.erase(priceKey);
	}
}

// 整日累计净流入（元）= Σ(主动买量-主动卖量)×价格×100
void MarketOrderBook::CalcCumNetInflow()
{
	m_cumNetInflow = 0.0;
	for (const auto& kv : m_priceCumVolMap)
	{
		double price = kv.first / 10000.0;   // 由量化key还原真实价格
		m_cumNetInflow += (kv.second.activeBuyVol - kv.second.activeSellVol) * price * 100.0;
	}
}

// 区间净流入（元）= 当前10条明细的买卖差
void MarketOrderBook::CalcIntervalNetInflow()
{
	m_intervalNetInflow = 0.0;
	for (const auto& t : m_transLines)
	{
		double amt = t.price * static_cast<double>(t.vol) * 100.0;   // 元
		if (t.buyOrSell == 1)      // 主动卖：流出
			m_intervalNetInflow -= amt;
		else                       // 主动买/中性：流入
			m_intervalNetInflow += amt;
	}
}

// 渲染层查表：该价格的主动买,主动卖累计量；price<=0或无记录返回{0,0}
std::pair<long long, long long> MarketOrderBook::GetCumVol(double price) const
{
	std::pair<long long, long long> ret{ 0,0 };
	if (price <= 0)
		return ret;
	auto it = m_priceCumVolMap.find(PriceToKey(price));
	if (it == m_priceCumVolMap.end())
		return ret;
	// 卖盘(ask)显示主动买成交量，买盘(bid)显示主动卖成交量
	ret.first = it->second.activeBuyVol;
	ret.second = it->second.activeSellVol;
	return ret;
}

// 卖一/买一后方累计主动成交量的瞬时变化量（手）
// isAskSide=true(卖一)返回累计主动买变化量，isAskSide=false(买一)返回累计主动卖变化量
long long MarketOrderBook::GetOrderDeltaLots(double price, bool isAskSide) const
{
	if (price <= 0)
		return 0;
	long long key = PriceToKey(price);
	// 需要当前采样和上次采样都存在才能计算变化量
	auto curIt = m_priceCumVolMap.find(key);
	auto prevIt = m_priceCumVolPrev.find(key);
	if (curIt == m_priceCumVolMap.end() || prevIt == m_priceCumVolPrev.end())
		return 0;
	return isAskSide
		? (curIt->second.activeBuyVol - prevIt->second.activeBuyVol)
		: (curIt->second.activeSellVol - prevIt->second.activeSellVol);
}

// 剩余排队挂单量（手）：从 StockData::estimateRuleMap（当日预估规则内存镜像）读取该价位的规则，
// = min(买入时挂单量, 建仓后快照最低挂单量, 当前实时挂单量) + 买入时累计 - 当前累计
// 规则.tradeType: 0=买入(watch买盘档), 1=卖出(watch卖盘档)；isAskSide须与规则盘别一致才返回
long long MarketOrderBook::GetRemainQueueAt(double price, bool isAskSide) const
{
	const long long NO_RULE = (std::numeric_limits<long long>::min)();
	if (m_code.empty() || price <= 0)
		return NO_RULE;
	auto stockData = g_data.GetStockData(m_code);
	if (!stockData)
		return NO_RULE;

	// 价格→规则（key与PriceToKey同口径：万分位整数）
	auto it = stockData->estimateRuleMap.find(PriceToKey(price));
	if (it == stockData->estimateRuleMap.end())
		return NO_RULE;
	const STOCK::VolumeOrderEstimate& rule = it->second;
	if (rule.tradeType == 1 != isAskSide)   // 盘别须一致(1=卖出→卖盘)
		return NO_RULE;

	// 当前实时挂单量（手）：该档位在当前盘口中的量，不在盘口（撤光/位移）时视为0
	const Level* levels = isAskSide ? m_ask : m_bid;
	long long curOrderLots = 0;
	for (int i = 0; i < 5; i++)
	{
		if (std::abs(levels[i].price - price) < 0.0005 && levels[i].price > 0)
		{
			curOrderLots = levels[i].volume / 100;   // 股→手
			break;
		}
	}

	// 当前累计成交量（该价格对侧口径：买入统计主动卖累计，卖出统计主动买累计）
	long long key = PriceToKey(price);
	long long curCum = 0;
	auto cumIt = m_priceCumVolMap.find(key);
	if (cumIt != m_priceCumVolMap.end())
		curCum = isAskSide ? cumIt->second.activeBuyVol : cumIt->second.activeSellVol;

	long long baseMin = rule.orderVolume;
	if (rule.snapshotMinOrder >= 0 && rule.snapshotMinOrder < baseMin)
		baseMin = rule.snapshotMinOrder;
	if (curOrderLots < baseMin)
		baseMin = curOrderLots;

	return baseMin + rule.cumVolume - curCum;
}

