#pragma once
#include "DataManager.h"

// COptionsDlg 对话框

class COptionsDlg : public CDialog
{
    DECLARE_DYNAMIC(COptionsDlg)

public:
    COptionsDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~COptionsDlg();
    void EnableUpdateBtn(bool enable);

    CString m_gp_code;

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_OPTIONS_DIALOG };
#endif

private:
    CEdit m_code_edit;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    int m_radio_gp_types;
    virtual BOOL OnInitDialog();
    afx_msg void OnChangeCodeEdit();
    afx_msg void OnBnClickedOk();
    afx_msg void OnBnClickedCancel();
    afx_msg void OnRadioClickedGpTypes();
};
