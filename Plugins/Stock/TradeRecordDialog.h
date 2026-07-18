#pragma once
#include "afxdialogex.h"

class CTradeRecordDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CTradeRecordDialog)

public:
	CTradeRecordDialog(CWnd* pParent = nullptr);
	virtual ~CTradeRecordDialog();

	enum { IDD = IDD_TRADE_RECORD_DIALOG };

	void SetTradeInfo(const CString& time, double price, const CString& stockCode, const CString& stockName = _T(""));

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedTradeTypeBuy();
	afx_msg void OnBnClickedTradeTypeSell();
	afx_msg void OnEnChangeTradeAmountEdit();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();

private:
	void UpdateCalculations();

	CString m_time;
	double m_price;
	CString m_stockCode;
	CString m_stockName;
	int m_tradeType;
	CString m_amount;
	double m_fee;
	double m_total;

	CButton m_btnBuy;
	CButton m_btnSell;
	CEdit m_editTime;
	CEdit m_editPrice;
	CEdit m_editAmount;
	CStatic m_staticAmount;
	CStatic m_staticFee;
	CStatic m_staticTotal;
};
