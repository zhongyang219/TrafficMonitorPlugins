#pragma once
#include "afxdialogex.h"
#include "StockDbManager.h"

// 预估成交量对话框：双击盘口行弹出
// 双击十档买卖行时自动带入该档的价格/方向，实时列显示盘口当前值；
// 双击首行（最高/最低行）时全部由用户手输。
// 点击确定只保存"买入时"列数据到 order_book_estimates 表，实时列不入库。
class CEstVolumeDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CEstVolumeDialog)

public:
	CEstVolumeDialog(CWnd* pParent = nullptr);
	virtual ~CEstVolumeDialog();

	enum { IDD = IDD_EST_VOLUME_DIALOG };

	// 调用方在 DoModal 前设置的入参
	void SetStockInfo(const std::wstring& stockCode, bool isAskRow)
	{
		m_stockCode = stockCode;
		m_tradeType = isAskRow ? 1 : 0;   // 双击卖盘=卖出(1)，买盘=买入(0)；与查询/保存口径一致
	}
	// 双击档位自动带入的信息（首行时不调用这些）
	void SetPrice(double price) { m_price = price; }
	void SetOrderVolume(long long vol) { m_orderVolume = vol; }
	void SetCumVolume(long long vol) { m_cumVolume = vol; }
	void SetEstVolume(long long vol) { m_estVolume = vol; }
	// 是否为十档行（决定价格/方向/实时列是否可编辑）
	void SetIsOrderRow(bool isOrderRow) { m_isOrderRow = isOrderRow; }
	// 实时列的当前盘口值（由调用方传入，双击后立即采集）
	void SetCurOrderVolume(long long vol) { m_curOrderVolume = vol; }
	void SetCurCumVolume(long long vol) { m_curCumVolume = vol; }
	void SetCurEstVolume(long long vol) { m_curEstVolume = vol; }
	// 数据库中已有的当天记录（买家价+方向匹配时回显），无记录时 totalAmount<=0 表示无
	void SetSavedEstimate(const CStockDbManager::OrderBookEstimate& est) { m_savedEst = est; m_hasSaved = est.totalAmount > 0; }
	// 双击时刻的时间（HH:MM），库中无记录时作为初始显示
	void SetNowTime(const CString& timeText) { m_timeText = timeText; }
	// 重置按钮用的当前盘口信息（十档行专用重置数据源；首行仅重置时间）
	void SetResetInfo(long long orderVolume, long long cumVolume) { m_resetOrderVolume = orderVolume; m_resetCumVolume = cumVolume; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedEstTypeBuy();
	afx_msg void OnBnClickedEstTypeSell();
	afx_msg void OnEnChangeEstCumBuyEdit();
	afx_msg void OnEnChangeEstOrderBuyEdit();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedResetBtn();

private:
	void UpdateEstVolume();   // 买入时预计成交累积量 = 挂单量 + 累计成交量（输入变化时联动）
	// 计算剩余排队挂单量（右列第三行）：
	// 基准 = min(买入时挂单量, 快照最低挂单量, 实时挂单量)，剩余排队 = 基准 + 买入时累计 - 当前累计
	// 同时把最终基准（最低挂单量）写入 m_minOrderVolume 供右列挂单量行展示
	long long CalcRemainQueue();
	long long m_minOrderVolume{ 0 };   // CalcRemainQueue 计算出的最低挂单量（手）

	std::wstring m_stockCode;
	int m_tradeType{ 0 };            // 0=买入, 1=卖出
	bool m_isOrderRow{ false };      // 双击的是十档买卖行（true）还是首行（false）
	double m_price{ 0.0 };
	long long m_orderVolume{ 0 };    // 双击行挂单量（买入时列初值）
	long long m_cumVolume{ 0 };      // 双击行累计成交量
	long long m_estVolume{ 0 };      // 双击行预计成交累积量
	long long m_curOrderVolume{ 0 }; // 当前实时挂单量
	long long m_curCumVolume{ 0 };   // 当前实时累计成交量
	long long m_curEstVolume{ 0 };   // 当前实时预计成交累积量
	CStockDbManager::OrderBookEstimate m_savedEst;  // 库中当天同价+方向的历史记录
	bool m_hasSaved{ false };        // 是否有可回显的历史记录
	CString m_timeText;              // 时间 HH:MM（库中有记录取库值，否则取双击时刻）
	long long m_resetOrderVolume{ 0 };  // 重置按钮的数据源：双击时的当前盘口挂单量(手)
	long long m_resetCumVolume{ 0 };    // 重置按钮的数据源：双击时的当前盘口累计成交量(手)

	CEdit m_editPrice;
	CButton m_btnBuy;
	CButton m_btnSell;
	CEdit m_editTime;                // 方向后侧的时间显示
	CEdit m_editTotal;
	CEdit m_editOrderBuy;
	CEdit m_editOrderCur;
	CEdit m_editCumBuy;
	CEdit m_editCumCur;
	CEdit m_editEstBuy;
	CEdit m_editEstCur;
	CButton m_btnOk;
	CButton m_btnCancel;
};
