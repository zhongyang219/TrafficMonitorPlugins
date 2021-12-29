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

void COptionsDlg::EnableUpdateBtn(bool enable)
{
    ::EnableWindow(GetDlgItem(IDC_UPDATE_WEATHER_BUTTON)->GetSafeHwnd(), enable);
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
    ON_BN_CLICKED(IDC_USE_WEATHER_ICON_CHECK, &COptionsDlg::OnBnClickedUseWeatherIconCheck)
    ON_EN_CHANGE(IDC_DISPLAY_WIDTH_EDIT, &COptionsDlg::OnEnChangeDisplayWidthEdit)
    ON_BN_CLICKED(IDC_UPDATE_WEATHER_BUTTON, &COptionsDlg::OnBnClickedUpdateWeatherButton)
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
    CheckDlgButton(IDC_USE_WEATHER_ICON_CHECK, m_data.m_use_weather_icon);
    SetDlgItemInt(IDC_DISPLAY_WIDTH_EDIT, m_data.m_display_width);

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


void COptionsDlg::OnBnClickedUseWeatherIconCheck()
{
    m_data.m_use_weather_icon = (IsDlgButtonChecked(IDC_USE_WEATHER_ICON_CHECK) != 0);
}


void COptionsDlg::OnEnChangeDisplayWidthEdit()
{
    // TODO:  如果该控件是 RICHEDIT 控件，它将不
    // 发送此通知，除非重写 CDialog::OnInitDialog()
    // 函数并调用 CRichEditCtrl().SetEventMask()，
    // 同时将 ENM_CHANGE 标志“或”运算到掩码中。

    // TODO:  在此添加控件通知处理程序代码
    m_data.m_display_width = GetDlgItemInt(IDC_DISPLAY_WIDTH_EDIT);
}


void COptionsDlg::OnBnClickedUpdateWeatherButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CWeather::Instance().SendWetherInfoQequest();
}
