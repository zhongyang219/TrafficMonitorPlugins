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
    int width0, width1;
    width0 = rect.Width() / 4;
    width1 = rect.Width() - width0 - CDataManager::Instance().DPI(20) - 1;
	m_alternativesList.InsertColumn(0, CDataManager::Instance().StringRes(IDS_CITY), LVCFMT_LEFT, width0);
	m_alternativesList.InsertColumn(1, CDataManager::Instance().StringRes(IDS_CITY_AO), LVCFMT_LEFT, width1);

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

	ResetStates();

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

	m_cityInfoList = query::QueryCityInfo(m_cityNameQuery);
	
	if (m_cityInfoList.size() == 0) return;

	int idx = 0;
	for (const auto &info : m_cityInfoList)
	{
		m_alternativesList.InsertItem(idx, info.CityName);
		m_alternativesList.SetItemText(idx, 1, info.CityAdministrativeOwnership);

		++idx;
	}
}
