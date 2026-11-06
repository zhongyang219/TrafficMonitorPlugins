#pragma once

#include <afx.h>
#include <string>
#include <map>
#include <vector>

// 前置声明（完整定义在 StockDef.h）
namespace STOCK { struct StockInfo; }

// ============================================================================
// 盘口数据模型（纯数据层，不含任何绘图代码，也不依赖 StockDef.h——避免循环包含）
// 职责：维护五档买卖盘/现价/高低/昨收等盘口状态，从数据库聚合各价格档位
//       的累计主动成交量，并在数据更新时一次性计算并缓存派生指标
//（累计净流入、区间净流入等），供渲染层只读访问。
// 数据更新入口：UpdateFromStockInfo（tick 推送时调用）+ RefreshTransCache（查库节流刷新）
// 渲染层只调用 Get* 只读接口，不做任何数据查询/计算。
// ============================================================================
class MarketOrderBook
{
public:
	// 档位（价格+数量），与 STOCK::OrderLevel 字段一致但定义独立
	struct Level
	{
		double price{ 0.0 };
		long long volume{ 0 };
	};

	// 各价格档位的累计主动成交量（来自数据库 tick_trade 聚合）
	// key 为价格量化到0.0001精度后的整数，避免浮点相等比较误差
	struct PriceCumVol
	{
		long long activeBuyVol{ 0 };   // 主动买(buyOrSell=0)累计手
		long long activeSellVol{ 0 };  // 主动卖(buyOrSell=1)累计手
	};

	// 成交明细行（渲染层直接显示，ETF价格已还原）
	struct TransLine
	{
		std::wstring timeKey;      // 时间 HH:MM
		double price{ 0.0 };       // 已还原的真实成交价
		long long vol{ 0 };        // 成交量（手）
		int buyOrSell{ 0 };        // 1=主动卖，0/2=主动买/中性
	};

public:
	void UpdateFromStockInfo(const STOCK::StockInfo& info);

	// 刷新数据库缓存：各价格档位累计成交量（2秒节流）+ 最近10条明细（3秒节流）
	// 并重算累计/区间净流入。渲染层每帧调用即可，内部有节流。
	void RefreshTransCache();

	// ===== 渲染层只读接口（全部为缓存值/预计算文本，无计算）=====

	// 五档只读（idx 0=卖一/买一，4=卖五/买五）
	const Level& GetAsk(int idx) const { return m_ask[idx]; }
	const Level& GetBid(int idx) const { return m_bid[idx]; }

	double GetCurrentPrice() const { return m_currentPrice; }
	double GetHighPrice() const { return m_high; }
	double GetLowPrice() const { return m_low; }
	double GetPrevClose() const { return m_prevClose; }
	double GetIOPV() const { return m_iopv; }
	bool IsETF() const { return m_isEtf; }
	const std::wstring& GetCode() const { return m_code; }

	// 最高/最低行显示文本（Update时预格式化）
	const CString& GetHighText() const { return m_highText; }
	const CString& GetLowText() const { return m_lowText; }

	// 累计净流入（整日，元）
	double GetCumNetInflow() const { return m_cumNetInflow; }
	// 区间净流入（当前明细10条，元）
	double GetIntervalNetInflow() const { return m_intervalNetInflow; }
	// 净比（1分钟加权平均，Update时从StockData::secVolumePool读取并缓存）
	double GetNetRatio() const { return m_netRatio; }
	bool HasNetRatio() const { return m_hasNetRatio; }

	// 各价格档位累计主动成交量（渲染层查表用）
	// 返回该价格的主动买,主动卖累计量；price<=0或无记录返回{0,0}
	std::pair<long long, long long> GetCumVol(double price) const;
	// 卖一(isAskSide=true)/买一后方累计主动成交量的瞬时变化量（手）
	long long GetOrderDeltaLots(double price, bool isAskSide) const;

	// 成交明细（最新在前，最多20条，已还原ETF价格）
	// count: 取前count条（盘口面板用10，MX明细界面用20）
	const std::vector<TransLine>& GetTransLines() const { return m_transLines; }

	// 是否有效（已有行情数据）
	bool IsValid() const { return m_valid; }

	// 剩余排队挂单量（手）：由 StockData::estimateRuleMap 中该价格的规则计算
	// = min(买入时挂单量, 快照最低, 当前实时挂单量) + 买入时累计 - 当前累计
	// 无规则/查询失败返回 INT64_MIN（渲染层不显示）
	long long GetRemainQueueAt(double price, bool isAskSide) const;

	// 五档中最大挂单量（Update时缓存，用于挂单量背景条占比）
	long long GetMaxOrderVolume() const { return m_maxOrderVol; }
	// 单行挂单量占五档最大量的比例（0~1）
	double GetOrderRowRatio(long long rowVol) const
	{
		return m_maxOrderVol > 0 ? static_cast<double>(rowVol) / m_maxOrderVol : 0.0;
	}
	// 价格是否等于当日最高/最低/现价（用于渲染配色）
	bool IsHighestPrice(double price) const { return m_high > 0 && price == m_high; }
	bool IsLowestPrice(double price) const { return m_low > 0 && price == m_low; }
	bool IsCurrentPrice(double price) const { return m_currentPrice > 0 && price == m_currentPrice; }

private:
	// 整日累计净流入（元）= Σ(主动买量-主动卖量)×价格×100
	void CalcCumNetInflow();
	// 区间净流入（元）= 当前10条明细的买卖差
	void CalcIntervalNetInflow();
	// 排队规则成交检查：剩余排队量<0 → 删除内存规则与DB记录（RefreshTransCache 尾部调用）
	void CheckNegativeRemainRules();

private:
	bool m_valid{ false };
	std::wstring m_code;   // 当前模型对应的股票代码（切换股票时清空缓存）

	// 原始盘口数据（五档、现价等）
	Level m_ask[5];
	Level m_bid[5];
	double m_currentPrice{ 0 };
	double m_high{ 0 };
	double m_low{ 0 };
	double m_prevClose{ 0 };
	double m_iopv{ 0 };
	bool m_isEtf{ false };
	long long m_maxOrderVol{ 0 };   // 五档最大挂单量（Update时缓存）

	// 缓存的派生结果（Update/Refresh时一次性计算）
	CString m_highText;      // "H:xx +yy" 预格式化
	CString m_lowText;       // "L:xx +yy" 预格式化
	double m_cumNetInflow{ 0 };      // 整日累计净流入（元）
	double m_intervalNetInflow{ 0 }; // 区间净流入（元）
	double m_netRatio{ 0 };          // 1分钟加权净比缓存
	bool m_hasNetRatio{ false };

	// 各价格档位累计成交量（当前采样 + 上次采样，用于瞬时变化量）
	std::map<long long, PriceCumVol> m_priceCumVolMap;
	std::map<long long, PriceCumVol> m_priceCumVolPrev;
	std::wstring m_priceCumVolCode;      // 已加载累计成交量的股票代码
	DWORD m_lastCumVolRefreshTick{ 0 };  // 上次刷新累计成交量的时机(ms)

	// 成交明细缓存（最新在前，最多10条）
	std::vector<TransLine> m_transLines;
	std::wstring m_transCode;            // 已加载明细的股票代码
	DWORD m_lastTransRefreshTick{ 0 };   // 上次刷新明细的时机(ms)

	// 剩余排队挂单监视：规则存于 StockData::estimateRuleMap（当日预估记录内存镜像，随DB同步），
	// 渲染层按当前档位价格+盘别查规则并计算剩余排队；不再保存在本模型中（重启后由启动加载恢复）
};
