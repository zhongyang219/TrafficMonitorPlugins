// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "Weather.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"
#include "SelectCityDlg.h"

// COptionsDlg 对话框

IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_OPTIONS_DIALOG, pParent)
{
}

COptionsDlg::~COptionsDlg()
{
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_WEATHER_TYPE_COMBO, m_weather_type_combo);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_BN_CLICKED(IDC_SELECT_CITY_BUTTON, &COptionsDlg::OnBnClickedSelectCityButton)
    ON_CBN_SELCHANGE(IDC_WEATHER_TYPE_COMBO, &COptionsDlg::OnCbnSelchangeWeatherTypeCombo)
    ON_BN_CLICKED(IDC_SHOW_TOOLTIPS_CHECK, &COptionsDlg::OnBnClickedShowTooltipsCheck)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetIcon(g_data.GetIcon(IDI_WEATHER), FALSE);
    SetDlgItemText(IDC_CITY_EDIT, g_data.CurCity().name.c_str());

    m_weather_type_combo.AddString(g_data.StringRes(IDS_TODAY_WEATHER));
    m_weather_type_combo.AddString(g_data.StringRes(IDS_TOMMORROW_WEATHER));
    m_weather_type_combo.SetCurSel(m_data.m_weather_selected);

    CheckDlgButton(IDC_SHOW_TOOLTIPS_CHECK, m_data.m_show_weather_in_tooltips);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnBnClickedSelectCityButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CSelectCityDlg dlg(this);
    if (dlg.DoModal() == IDOK)
    {
        int city_index = dlg.GetCityIndex();
        if (city_index >= 0 && city_index < static_cast<int>(CityCode.size()))
        {
            SetDlgItemText(IDC_CITY_EDIT, CityCode[city_index].name.c_str());
            m_data.m_city_index = city_index;
        }
    }
}


void COptionsDlg::OnCbnSelchangeWeatherTypeCombo()
{
    m_data.m_weather_selected = static_cast<WeahterSelected>(m_weather_type_combo.GetCurSel());
}


void COptionsDlg::OnBnClickedShowTooltipsCheck()
{
    m_data.m_show_weather_in_tooltips = (IsDlgButtonChecked(IDC_SHOW_TOOLTIPS_CHECK) != 0);
}
