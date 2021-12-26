#pragma once
#include <afxeditbrowsectrl.h>
#include <vector>

// CSelectCityDlg 对话框

class CSelectCityDlg : public CDialog
{
    DECLARE_DYNAMIC(CSelectCityDlg)

public:
    CSelectCityDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CSelectCityDlg();
    int GetCityIndex() const;

    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_SELECT_CITY_DIALOG };
#endif
private:
    CListCtrl m_list_ctrl;
    CEdit m_search_edit;
    int m_index{ -1 };
    bool m_searched{ false };           //是否处于搜索状态
    std::vector<int> m_search_result;   //搜索结果

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
    void ShowList();
    void QuickSearch(const std::wstring& key_word);

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnNMClickCityList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMRClickCityList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnEnChangeSearchEdit();
};
