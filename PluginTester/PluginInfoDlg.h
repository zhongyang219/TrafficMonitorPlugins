#pragma once
#include "CommonData.h"

// CPluginInfoDlg 对话框

class CPluginInfoDlg : public CDialog
{
    DECLARE_DYNAMIC(CPluginInfoDlg)

public:
    CPluginInfoDlg(PluginInfo plugin_info, CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CPluginInfoDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DETAIL_DIALOG };
#endif

private:
    CListCtrl m_info_list;
    PluginInfo m_plugin_info;

    //列表中的列
    enum RowIndex
    {
        RI_FILE_NAME,
        RI_FILE_PATH,
        RI_NAME,
        RI_DESCRIPTION,
        RI_ITEM_NUM,
        RI_ITEM_NAMES,
        RI_ITEM_ID,
        RI_AUTHOR,
        RI_COPYRIGHT,
        RI_URL,
        RI_VERSION,
        RI_API_VERSION,
        RI_MAX
    };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
    void ShowInfo();
    static CString GetRowName(int row_index);   //获取行的名称

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();

};
