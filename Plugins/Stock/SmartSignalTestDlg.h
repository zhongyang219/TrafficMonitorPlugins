#pragma once

#include <string>
#include <afxwin.h>
#include "SignalAnalyzer.h"

// 信号测试弹窗：双击走势图时弹出信号分析结果
class CSmartSignalTestDlg
{
public:
	// 弹出信号分析测试对话框
	// stockId: 股票代码
	// timelineIndex: 双击位置的时间线索引
	// isKLineMode: 是否K线模式
	// isMin5KLineMode: 是否5分钟K线模式
	// pendingTradePrice: 双击价格
	// pendingTradeTime: 双击时间
	// pParentWnd: 父窗口（用于MessageBox和定位）
	static void Show(const std::wstring& stockId, int timelineIndex,
		bool isKLineMode, bool isMin5KLineMode,
		double pendingTradePrice, const CString& pendingTradeTime,
		CWnd* pParentWnd);

private:
	// 辅助文本转换函数
	static CString BoolToText(bool value);
	static CString TrendStateToText(STOCK::TrendState30m state);
	static CString Signal5mToText(STOCK::Signal5m signal);
};
