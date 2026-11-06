#pragma once

#include <StockDef.h>
#include <Common.h>
#include <vector>

// 筹码峰面板绘制
// 职责：在指定矩形区域内绘制筹码分布图（价格-筹码量条形图）、获利比例条、
//       平均成本/90%成本区间等统计文字
// 数据来源：chipData（筹码分布）、timelinePoint（分时成交量，用于筹码衰减修正）
// 说明：当 isKLineMode=false 且 stockInfo.circulatingAShares>0 时，会基于分时成交量
//       对筹码分布进行衰减修正（模拟换手导致筹码迁移）
class CChipPeakPanel
{
public:
	// 绘制筹码峰面板
	// left, right: 面板左右边界（含主标题栏下方的盘口标题栏区域）
	// height: 面板总高度（含盘口标题栏）
	// viewMode: 当前视图模式（K线模式下跳过筹码衰减修正）
	void Draw(CDC& memDC, int left, int right, int height, const STOCK::StockInfo& stockInfo,
		const STOCK::ChipDistribution& chipData, const std::vector<STOCK::TimelinePoint>& timelinePoint,
		UIViewMode viewMode);
};
