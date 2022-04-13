// SelectCityDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "SelectCityDlg.h"
#include "DataManager.h"

#include "Common.h"
// CSelectCityDlg 对话框

IMPLEMENT_DYNAMIC(CSelectCityDlg, CDialogEx)

CSelectCityDlg::CSelectCityDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_SELECT_CITY_DLG, pParent)
    , m_cityNameQuery(L"")
{

}

CSelectCityDlg::~CSelectCityDlg()
{
}

void CSelectCityDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CITY_NAME_EDIT, m_cityNameEdit);
    DDX_Control(pDX, IDC_ALTERNATIVES_LIST, m_alternativesList);
    DDX_Text(pDX, IDC_CITY_NAME_EDIT, m_cityNameQuery);
}


BEGIN_MESSAGE_MAP(CSelectCityDlg, CDialogEx)
    ON_BN_CLICKED(IDC_SEARCH_BTN, &CSelectCityDlg::OnBnClickedSearchBtn)
END_MESSAGE_MAP()


void CSelectCityDlg::ResetStates()
{
    m_alternativesList.DeleteAllItems();
    m_cityInfoList.clear();
    m_selectedCityInfo = SCityInfo();
}


// CSelectCityDlg 消息处理程序


BOOL CSelectCityDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    ResetStates();

    // 搜索框
    CString strBanner;
    strBanner.LoadString(IDS_EDIT_BANNER);
    m_cityNameEdit.SetCueBanner(strBanner);

    // 
    CRect rect;
    m_alternativesList.GetClientRect(rect);
    m_alternativesList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
    int width0 = rect.Width() / 5;
    m_alternativesList.InsertColumn(0, CDataManager::Instance().StringRes(IDS_CITY), LVCFMT_LEFT, width0);
    m_alternativesList.InsertColumn(1, CDataManager::Instance().StringRes(IDS_CITY_AO), LVCFMT_LEFT, width0 * 2);
    m_alternativesList.InsertColumn(2, CDataManager::Instance().StringRes(IDS_CITY_NO), LVCFMT_LEFT, width0 * 2);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void CSelectCityDlg::OnOK()
{
    auto pos = m_alternativesList.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        auto idx = m_alternativesList.GetNextSelectedItem(pos);
        if (idx >= 0 && idx < m_cityInfoList.size())
            m_selectedCityInfo = m_cityInfoList[idx];
    }
    else
    {
        if (m_cityInfoList.size() == 1)
            m_selectedCityInfo = m_cityInfoList[0];
    }

    CDialogEx::OnOK();
}


void CSelectCityDlg::OnCancel()
{
    ResetStates();

    CDialogEx::OnCancel();
}


void CSelectCityDlg::OnBnClickedSearchBtn()
{
    UpdateData(TRUE);
    if (m_cityNameQuery.IsEmpty()) return;

    ResetStates();

    if (query::QueryCityInfo(std::wstring(m_cityNameQuery), m_cityInfoList))
    {
        if (m_cityInfoList.size() == 0) return;

        int idx = 0;
        for (const auto &info : m_cityInfoList)
        {
            m_alternativesList.InsertItem(idx, info.CityName.c_str());
            m_alternativesList.SetItemText(idx, 1, info.CityAdministrativeOwnership.c_str());
            m_alternativesList.SetItemText(idx, 2, info.CityNO.c_str());

            ++idx;
        }
    }
    else
        MessageBox(CDataManager::Instance().StringRes(IDS_QUERY_ERR),
                   CDataManager::Instance().StringRes(IDS_WEATHER_PRO));
}
