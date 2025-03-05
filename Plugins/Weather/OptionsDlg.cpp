// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "Weather.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"
#include "SelectCityDlg.h"
#include "CurLocationHelper.h"

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

void COptionsDlg::UpdateAutoLocteResult()
{
    CString str_info;
    //显示定位结果
    if (g_data.m_setting_data.auto_locate && g_data.m_auto_located)
    {
        if (!g_data.m_auto_locate_succeed)
            str_info = g_data.StringRes(IDS_LOCATION_FAILED);
        else
            str_info = g_data.StringRes(IDS_CUR_POSITION) + _T(": ") + g_data.CurCity().name.c_str();
        str_info += _T(' ');
    }
    //显示更新时间
    str_info += g_data.StringRes(IDS_UPDATE_TIME);
    str_info += _T(": ");
    str_info += g_data.GetUpdateTimeAsString().GetString();

    SetDlgItemText(IDC_AUTO_LOCATE_RESULT_STATIC, str_info);
}

void COptionsDlg::EnableControl()
{
    //GetDlgItem(IDC_CITY_EDIT)->EnableWindow(!m_data.auto_locate);
    //GetDlgItem(IDC_SELECT_CITY_BUTTON)->EnableWindow(!m_data.auto_locate);
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
    ON_BN_CLICKED(IDC_UPDATE_WEATHER_BUTTON, &COptionsDlg::OnBnClickedUpdateWeatherButton)
    ON_BN_CLICKED(IDC_TEST_BUTTON, &COptionsDlg::OnBnClickedTestButton)
    ON_BN_CLICKED(IDC_AUTO_LOCATE_CHECK, &COptionsDlg::OnBnClickedAutoLocateCheck)
    ON_NOTIFY(NM_CLICK, IDC_HELP_SYSLINK, &COptionsDlg::OnNMClickHelpSyslink)
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetIcon(g_data.GetIcon(IDI_WEATHER), FALSE);

    //获取初始时窗口的大小
    CRect rect;
    GetWindowRect(rect);
    m_min_size.cx = rect.Width();
    m_min_size.cy = rect.Height();

    SetDlgItemText(IDC_CITY_EDIT, g_data.CurCity().name.c_str());

    m_weather_type_combo.AddString(g_data.StringRes(IDS_CURRENT_WEATHER));
    m_weather_type_combo.AddString(g_data.StringRes(IDS_TODAY_WEATHER));
    m_weather_type_combo.AddString(g_data.StringRes(IDS_TOMMORROW_WEATHER));
    m_weather_type_combo.SetCurSel(m_data.m_weather_selected);

    CheckDlgButton(IDC_AUTO_LOCATE_CHECK, m_data.auto_locate);
    CheckDlgButton(IDC_SHOW_TOOLTIPS_CHECK, m_data.m_show_weather_in_tooltips);
    CheckDlgButton(IDC_USE_WEATHER_ICON_CHECK, m_data.m_use_weather_icon);

#ifndef _DEBUG
    CWnd* pBtn = GetDlgItem(IDC_TEST_BUTTON);
    if (pBtn != nullptr)
        pBtn->ShowWindow(SW_HIDE);
#endif // !_DEBUG

    EnableControl();

    UpdateAutoLocteResult();

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
            //手动选择了城市后取消“自动定位”的勾选
            m_data.auto_locate = false;
            CheckDlgButton(IDC_AUTO_LOCATE_CHECK, FALSE);
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


void COptionsDlg::OnBnClickedUpdateWeatherButton()
{
    // TODO: 在此添加控件通知处理程序代码
    CWeather::Instance().SendWetherInfoQequest();
}


void COptionsDlg::OnBnClickedTestButton()
{
    // TODO: 在此添加控件通知处理程序代码
    std::wstring cur_city = CCurLocationHelper::GetCurrentCity();
    CCurLocationHelper::Location location = CCurLocationHelper::ParseCityName(cur_city);
    int city_code_index = CCurLocationHelper::FindCityCodeItem(location);
    int a = 0;
}


void COptionsDlg::OnBnClickedAutoLocateCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    m_data.auto_locate = (IsDlgButtonChecked(IDC_AUTO_LOCATE_CHECK) != 0);
    EnableControl();
}


void COptionsDlg::OnNMClickHelpSyslink(NMHDR* pNMHDR, LRESULT* pResult)
{
    ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/TrafficMonitorPlugins/wiki/%E5%A4%A9%E6%B0%94%E6%8F%92%E4%BB%B6"), NULL, NULL, SW_SHOW);
    *pResult = 0;
}


void COptionsDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    //限制窗口最小大小
    lpMMI->ptMinTrackSize.x = m_min_size.cx;		//设置最小宽度
    lpMMI->ptMinTrackSize.y = m_min_size.cy;		//设置最小高度

    CDialog::OnGetMinMaxInfo(lpMMI);
}
