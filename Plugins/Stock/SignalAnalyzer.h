#pragma once
#include "StockDef.h"

class CSignalAnalyzer
{
public:
	// T+0日内买卖信号
	struct T0Signal {
		bool valid;         // 是否有有效信号
		bool isBuy;         // true=买点, false=卖点
		int strength;       // 信号强度 1-3
		CString reason;     // 信号原因描述
		double price;       // 信号价格
		CString time;       // 信号时间
		STOCK::TrendState30m trendState; // 30分钟趋势状态
		bool isForbid;      // 风控禁止交易标志

		T0Signal() : valid(false), isBuy(false), strength(0), price(0),
			trendState(STOCK::TrendState30m::STATE_SHAKE), isForbid(false) {
		}
	};

	// 批量信号点
	struct SmartSignalPoint {
		int barIndex;               // bars5中的索引
		bool isBuy;                 // true=买, false=卖
		bool isForbid;              // 风控禁止
		STOCK::TrendState30m trendState; // 30分钟趋势
		CString reason;             // 信号原因
	};

	// 所有函数都是 static，纯计算无状态

	// ========== 基础工具函数 ==========
	static double CalcMA(const std::vector<double>& values, int N);
	static double CalcEMA(const std::vector<double>& values, int N);
	static double CalcSMA(const std::vector<double>& values, int N, double M = 1.0);
	static double CalcStdDev(const std::vector<double>& values);

	// ========== 5个核心指标计算函数 ==========
	static STOCK::BollResult CalcBoll(const std::vector<STOCK::Bar>& bars, int N = 20);
	static void CalcMACDSeries(const std::vector<STOCK::Bar>& bars, std::vector<double>& difSeq, std::vector<double>& deaSeq, std::vector<double>& barSeq);
	static STOCK::MACDResult CalcMACD(const std::vector<STOCK::Bar>& bars);
	static STOCK::KDJResult CalcKDJ(const std::vector<STOCK::Bar>& bars, int N = 9);
	static double CalcRSI(const std::vector<STOCK::Bar>& bars, int N);
	static double CalcWR(const std::vector<STOCK::Bar>& bars, int N);

	// ========== 30分钟趋势判定 ==========
	static STOCK::TrendState30m Get30mTrendState(const std::vector<STOCK::Bar>& bars30);

	// ========== 5分钟共振买卖判定 ==========
	static STOCK::Signal5m Get5mSignal(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState);

	// ========== 单边钝化风控过滤器 ==========
	static bool IsForbidTrade(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState);

	// ========== 完整买卖点判定函数 ==========
	static T0Signal DetectSmartSignal(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30, double lastSignalPrice = 0);

	// ========== 批量信号检测 ==========
	static std::vector<SmartSignalPoint> BatchDetectSignals(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30);
};
