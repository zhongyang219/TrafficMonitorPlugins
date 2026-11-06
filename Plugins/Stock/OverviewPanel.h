#pragma once

#include <StockDef.h>
#include <string>
#include <vector>

// 总览表行信息（用于双击处理：点击名称列切换股票、点击删除按钮删除股票）
struct OverviewRowInfo {
	std::wstring code;
	int rowY = 0;
	int rowH = 0;
	int nameColWidth = 0;
	int deleteBtnStartX = 0;
	int deleteBtnEndX = 0;
};

// 总览面板绘制
// 职责：1) DrawIndexSection - 绘制大盘指数区域（名称/价格/涨跌幅）
//       2) DrawOverviewTable - 绘制持仓总览表格（名称/价格/涨跌/成本/持股/市值/盈亏等）
// 数据来源：g_data 共享数据（线程安全加锁访问）
// 说明：DrawOverviewTable 通过 outRows 输出各行坐标信息，供调用方做点击命中检测
class COverviewPanel
{
public:
	// 绘制大盘指数区域
	// x, y, w: 区域位置和宽度；indices: 指数列表（代码+行情）
	void DrawIndexSection(CDC& memDC, int x, int y, int w, const std::vector<std::pair<std::wstring, STOCK::StockInfo>>& indices);

	// 绘制持仓总览表格
	// x, y, w, h: 表格区域；vScrollOffset: 垂直滚动偏移；totalHeight: 窗口总高度（用于底部汇总栏定位）
	// outRows: 输出各行坐标信息（供点击命中检测）
	void DrawOverviewTable(CDC& memDC, int x, int y, int w, int h, int vScrollOffset, int totalHeight,
		std::vector<OverviewRowInfo>& outRows);
};
