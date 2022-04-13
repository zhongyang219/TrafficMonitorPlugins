// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "OptionsDlg.h"

#include "SelectCityDlg.h"
#include "DataManager.h"


// COptionsDlg 对话框

IMPLEMENT_DYNAMIC(COptionsDlg, CDialogEx)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_OPTIONS_DLG, pParent)
    , m_currentCityName(_T(""))
    , m_showWeatherIcon(FALSE)
    , m_showWeatherInTooltip(FALSE)
    , m_showWeatherAlerts(FALSE)
    , m_showBriefWeatherAlertInfo(FALSE)
    , m_showBriefRTWeather(FALSE)
{

}

COptionsDlg::~COptionsDlg()
{
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_CURRENT_CITY, m_currentCityName);
    DDX_Control(pDX, IDC_COMBO_INFO_TYPE, m_ctrlInfoType);
    DDX_Check(pDX, IDC_CHECK_SHOW_WEATHER_ICON, m_showWeatherIcon);
    DDX_Check(pDX, IDC_CHECK_SHOW_WEATHE_IN_TOOLTIP, m_showWeatherInTooltip);
    DDX_Check(pDX, IDC_CHECK_SHOW_WEATHER_ALERTS, m_showWeatherAlerts);
    DDX_Check(pDX, IDC_CHECK_SHOW_BRIEF_WEATHER_ALERT_INFO, m_showBriefWeatherAlertInfo);
    DDX_Check(pDX, IDC_CHECK_SHOW_BRIEF_RT_WEATHER, m_showBriefRTWeather);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_SELECT_CITY, &COptionsDlg::OnBnClickedBtnSelectCity)
    ON_BN_CLICKED(IDC_BTN_UPDATE_MANUALLY, &COptionsDlg::OnBnClickedBtnUpdateManually)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 在此添加额外的初始化
    m_currentCityName = CDataManager::Instance().GetCurrentCityInfo().CityName.c_str();

    const auto &config = CDataManager::Instance().GetConfig();

    m_ctrlInfoType.AddString(CDataManager::Instance().StringRes(IDS_WIT_REAL_TIME));
    m_ctrlInfoType.AddString(CDataManager::Instance().StringRes(IDS_WIT_24H));
    m_ctrlInfoType.AddString(CDataManager::Instance().StringRes(IDS_WIT_48H));
    m_ctrlInfoType.SetCurSel(static_cast<int>(config.m_wit));

    m_showWeatherIcon = config.m_show_weather_icon ? TRUE : FALSE;
    m_showWeatherInTooltip = config.m_show_weather_in_tooltips ? TRUE : FALSE;
    m_showWeatherAlerts = config.m_show_weather_alerts ? TRUE : FALSE;
    m_showBriefWeatherAlertInfo = config.m_show_brief_weather_alert_info ? TRUE : FALSE;
    m_showBriefRTWeather = config.m_show_brief_rt_weather_info ? TRUE : FALSE;

    UpdateData(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnBnClickedBtnSelectCity()
{
    CSelectCityDlg dlg(this);
    if (dlg.DoModal() == IDOK)
    {
        if (!dlg.m_selectedCityInfo.CityName.empty() &&
            dlg.m_selectedCityInfo.CityNO != CDataManager::Instance().GetCurrentCityInfo().CityNO)
        {
            if (CDataManager::Instance().GetCurrentCityInfo().CityNO != dlg.m_selectedCityInfo.CityNO)
            {
                // 更换了城市，更新天气信息
                CDataManager::InstanceRef().SetCurrentCityInfo(dlg.m_selectedCityInfo);

                m_currentCityName = dlg.m_selectedCityInfo.CityName.c_str();
                UpdateData(FALSE);

                CWeatherPro::Instance().UpdateWeatherInfo(true);
            }
        }
    }
}


void COptionsDlg::OnOK()
{
    // 更新设置
    UpdateData(TRUE);

    auto &config = CDataManager::InstanceRef().GetConfig();
    config.m_wit = static_cast<EWeatherInfoType>(m_ctrlInfoType.GetCurSel());
    config.m_show_weather_icon = m_showWeatherIcon == TRUE;
    config.m_show_weather_in_tooltips = m_showWeatherInTooltip == TRUE;
    config.m_show_brief_rt_weather_info = m_showBriefRTWeather == TRUE;
    config.m_show_weather_alerts = m_showWeatherAlerts == TRUE;
    config.m_show_brief_weather_alert_info = m_showBriefWeatherAlertInfo == TRUE;

    // 更新tooltip
    CDataManager::InstanceRef().RefreshWeatherInfoCache();
    CWeatherPro::Instance().UpdateTooltip(CDataManager::Instance().GetTooptipInfo());

    // 保存配置文件
    CDataManager::Instance().SaveConfigs();

    CDialogEx::OnOK();
}


void COptionsDlg::OnBnClickedBtnUpdateManually()
{
    // the time span between two updates should be at least 30 seconds
    const std::time_t time_span{ 30 };

    auto t = std::time(nullptr);
    auto ts = t - CWeatherPro::Instance().GetLastUpdateTimestamp();
    if (ts < time_span)
    {
        CString info;
        info.Format(CDataManager::Instance().StringRes(IDS_UPDATTE_MANUALLY_LATER),
                    (time_span - ts));
        MessageBox(info, CDataManager::Instance().StringRes(IDS_WEATHER_PRO));
        return;
    }

    CWeatherPro::Instance().UpdateWeatherInfo(true);
}
