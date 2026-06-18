#pragma once
#include "DataManager.h"

// COptionsDlg 对话框

class COptionsDlg : public CDialog
{
	DECLARE_DYNAMIC(COptionsDlg)

public:
	COptionsDlg(const std::wstring& code = std::wstring(), CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~COptionsDlg();
	void EnableUpdateBtn(bool enable);

	static void RemoveTypeFromCode(CString& code);      //从股票代码中移除类型
	static CString GetCodeType(const CString& code);    //获取股票代码的类型
	static CString GetMarketTypeByCode(const CString& code); //根据股票代码自动判断市场类型
	static CString TypeToDisplayName(const CString& type);   //将类型转换为显示名称
	static CString DisplayNameToType(const CString& displayName); //将显示名称转换为类型

	CString m_stock_code;
	CRect m_refWndRect;  // 参考窗口位置，用于定位对话框

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_OPTIONS_DIALOG };
#endif

private:
	CEdit m_code_edit;
	CComboBox m_market_type_combo;
	double m_alert_low_price{};
	double m_alert_high_price{};
	double m_cost_price{};
	double m_holding_count{};
	CString m_buy_date;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeCodeEdit();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedToday();
};
