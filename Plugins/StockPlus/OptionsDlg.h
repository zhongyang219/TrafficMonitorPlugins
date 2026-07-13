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

    CString m_stock_code;

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_OPTIONS_DIALOG };
#endif

private:
    CEdit m_code_edit;
    int m_radio_stock_types;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnChangeCodeEdit();
    afx_msg void OnBnClickedOk();
    afx_msg void OnBnClickedCancel();
    afx_msg void OnRadioClickedStockTypes();
};
