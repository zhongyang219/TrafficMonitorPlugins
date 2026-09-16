#pragma once

#include <vector>
#include <StockDef.h>

class TechnicalIndicatorModel;

// 图表绘制共享上下文结构
// 从 CFloatingWnd 中提取，供各图表模块（CCallAuctionChart/CChartPrice/...）共享

// 分时图绘制上下文（包含几何坐标、数据指针、MA值等）
struct TimelineDrawContext {
	int chartWidth;
	int chartLeft{ 0 };   // 绘图区左侧偏移（用于Y轴标签留白）
	int windowWidth;
	int chartHeight;
	int priceChartTop;
	int priceChartHeight;
	int volumeChartTop;
	int volumeChartHeight;
	int macdChartTop;
	double niceStep{ 0 };  // Y轴刻度步长（OnPaint计算一次，绘制函数直接用）
	int macdChartHeight;
	int positionY;
	int visibleCount{ 0 };   // 可见数据点数（≤120）
	int xAxisPoints{ 0 };   // X轴总格数（=m_timelineVisibleCount，数据不足时右侧留白）
	int startIndex{ 0 };     // 可见数据起始索引
	int scrollRange{ 0 };    // 滚动范围
	STOCK::Price maxPrice{ 0 };   // 可见区间最大价（含内边距）
	STOCK::Price minPrice{ 0 };   // 可见区间最小价（含内边距）
	double unitY{ 0 };            // Y轴缩放（像素/价格）
	STOCK::StockInfo realtimeData;
	const std::vector<STOCK::TimelinePoint>* timelinePoint;    // 可见范围子集（subTimeline）
	const std::vector<STOCK::TimelinePoint>* fullTimeline{ nullptr };  // 完整分时数据（用于布林带等需要历史回溯的指标）
	const std::vector<STOCK::KLinePoint>* klineData;
	// 滚动均价（最新数据点的值）
	STOCK::Price ma1{ 0 };    // MA1（1分钟均价 = 当前价格）
	STOCK::Price ma5{ 0 };    // MA5（5分钟滚动均价）
	STOCK::Price ma10{ 0 };   // MA10（10分钟滚动均价）
	STOCK::Price ma20{ 0 };   // MA20（20分钟滚动均价）
	// 前一分钟数据点的MA值（用于箭头方向判断）
	STOCK::Price prevMa1{ 0 };
	STOCK::Price prevMa5{ 0 };
	STOCK::Price prevMa10{ 0 };
	STOCK::Price prevMa20{ 0 };
	// 技术指标模型（MACD/KDJ/WR/RSI/BOLL/量MA缓存序列，渲染层只读）
	const TechnicalIndicatorModel* indicatorModel{ nullptr };
	// MACD图内叠加KDJ的J线开关（KDJ按钮点击时由FloatingWnd设置）
	bool overlayKDJInMacd{ false };
};
