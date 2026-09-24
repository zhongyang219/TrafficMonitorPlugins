#pragma once

#include "ChartContext.h"
#include <StockDef.h>

// 状态栏/标题栏/持仓信息面板绘制
// 职责：绘制主标题栏、分时持仓信息栏、K线持仓信息栏、K线信息面板
class CStatusBarPanel
{
public:
	// 绘制主标题栏（股票名称+现价+涨跌幅+MACD信号）
	// leftStart：文字绘制起始左侧（避开顶部按钮区），textMaxWidth：文字可用最大宽度
	static void DrawMainHeader(CDC& memDC, const STOCK::StockInfo& realtimeData, int windowWidth, int headerHeight,
		int leftStart = 0, int textMaxWidth = 0);

	// 绘制管理股票栏（关联股票状态栏，位于标题栏下方）
	void DrawRelatedStatusBar(CDC& memDC, int w, int topBarY, int singleBarHeight, const std::wstring& stockId, int viewMode);

	// 绘制系统状态栏（底部单行）
	void DrawSystemStatusBar(CDC& memDC, int w, int bottomBarY, int singleBarHeight);

private:
};