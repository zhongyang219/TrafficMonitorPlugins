#pragma once
#include "DataManager.h"
#include "afxdialogex.h"

// COptionsDlg 对话框

class COptionsDlg : public CDialogEx
{
    DECLARE_DYNAMIC(COptionsDlg)

public:
    COptionsDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~COptionsDlg();

    SettingData m_data;

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_OPTIONS_DIALOG };
#endif

private:
    std::wstring m_file_path_ori;   //初始时的文件路径

protected:
    void EnableDlgControl(UINT id, bool enable);
    void EnableControls();
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedBrowseButton();
    virtual void OnOK();
    afx_msg void OnBnClickedMouseWheelEnableCheck();
    afx_msg void OnBnClickedWheelReadPageCheck();
};
