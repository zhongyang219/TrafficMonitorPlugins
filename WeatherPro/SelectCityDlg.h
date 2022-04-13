#pragma once
#include "afxdialogex.h"
#include "DataQuerier.h"

// CSelectCityDlg 对话框

class CSelectCityDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CSelectCityDlg)

public:
    CSelectCityDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CSelectCityDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_SELECT_CITY_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()

    CityInfoList m_cityInfoList;
    CEdit m_cityNameEdit;
    CListCtrl m_alternativesList;
public:
    afx_msg void OnBnClickedSearchBtn();
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();

    void ResetStates();
    SCityInfo m_selectedCityInfo;
    CString m_cityNameQuery;
};
