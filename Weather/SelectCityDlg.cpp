// SelectCityDlg.cpp: 实现文件
//

#include "pch.h"
#include "Weather.h"
#include "SelectCityDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"
#include "CityCode.h"


// CSelectCityDlg 对话框

IMPLEMENT_DYNAMIC(CSelectCityDlg, CDialog)

CSelectCityDlg::CSelectCityDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_SELECT_CITY_DIALOG, pParent)
{

}

CSelectCityDlg::~CSelectCityDlg()
{
}

int CSelectCityDlg::GetCityIndex() const
{
    return m_index;
}

void CSelectCityDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CITY_LIST, m_list_ctrl);
}


BEGIN_MESSAGE_MAP(CSelectCityDlg, CDialog)
    ON_NOTIFY(NM_CLICK, IDC_CITY_LIST, &CSelectCityDlg::OnNMClickCityList)
    ON_NOTIFY(NM_RCLICK, IDC_CITY_LIST, &CSelectCityDlg::OnNMRClickCityList)
END_MESSAGE_MAP()


// CSelectCityDlg 消息处理程序


BOOL CSelectCityDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetIcon(g_data.GetIcon(IDI_WEATHER), FALSE);
    CRect rect;
    m_list_ctrl.GetClientRect(rect);
    m_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
    int width0, width1;
    width0 = rect.Width() / 4;
    width1 = rect.Width() - width0 - g_data.DPI(20) - 1;
    m_list_ctrl.InsertColumn(0, g_data.StringRes(IDS_CITY), LVCFMT_LEFT, width0);
    m_list_ctrl.InsertColumn(1, g_data.StringRes(IDS_CITY_CODE), LVCFMT_LEFT, width1);

    int i{};
    for (const auto& item : CityCode)
    {
        m_list_ctrl.InsertItem(i, item.name.c_str());
        m_list_ctrl.SetItemText(i, 1, item.code.c_str());
        i++;
    }

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void CSelectCityDlg::OnNMClickCityList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码
    m_index = pNMItemActivate->iItem;
    *pResult = 0;
}


void CSelectCityDlg::OnNMRClickCityList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码
    m_index = pNMItemActivate->iItem;
    *pResult = 0;
}
