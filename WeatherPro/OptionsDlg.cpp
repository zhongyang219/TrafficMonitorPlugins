// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "OptionsDlg.h"

#include "SelectCityDlg.h"


// COptionsDlg 对话框

IMPLEMENT_DYNAMIC(COptionsDlg, CDialogEx)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_OPTIONS_DLG, pParent)
	, m_currentCityName(_T(""))
{

}

COptionsDlg::~COptionsDlg()
{
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_CURRENT_CITY, m_currentCityName);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_SELECT_CITY, &COptionsDlg::OnBnClickedBtnSelectCity)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


void COptionsDlg::OnBnClickedBtnSelectCity()
{
	CSelectCityDlg dlg(this);
	if (dlg.DoModal() == IDOK)
	{
		m_currentCityName = dlg.m_selectedCityInfo.CityName;

		UpdateData(FALSE);
	}
}
