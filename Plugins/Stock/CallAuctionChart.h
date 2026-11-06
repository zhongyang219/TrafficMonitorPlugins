#pragma once

#include "ChartContext.h"
#include <StockDef.h>

// 集合竞价走势图绘制
// 职责：在 ctx 指定区域内绘制集合竞价的价格走势曲线、成交量副图、时间标签
// 数据来源：callAuctionData（由调用方加锁获取后传入）
class CCallAuctionChart
{
public:
	void Draw(CDC& memDC, const TimelineDrawContext& ctx, const STOCK::CallAuctionData& callAuctionData);
};
