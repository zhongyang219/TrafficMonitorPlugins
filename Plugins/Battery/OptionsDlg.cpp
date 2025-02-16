// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "Battery.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
#include "DataManager.h"

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
    DDX_Control(pDX, IDC_COMBO1, m_battery_type_combo);
}


BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_CBN_SELCHANGE(IDC_COMBO1, &COptionsDlg::OnCbnSelchangeCombo1)
    ON_BN_CLICKED(IDC_SHOW_TOOLTIPS_CHECK, &COptionsDlg::OnBnClickedShowTooltipsCheck)
    ON_BN_CLICKED(IDC_SHOW_PERCENT_CHECK, &COptionsDlg::OnBnClickedShowPercentCheck)
    ON_BN_CLICKED(IDC_SHOW_CHARGING_ANIMATION_CHECK, &COptionsDlg::OnBnClickedShowChargingAnimationCheck)
    ON_NOTIFY(NM_CLICK, IDC_HELP_SYSLINK, &COptionsDlg::OnNMClickHelpSyslink)
END_MESSAGE_MAP()


// COptionsDlg 消息处理程序


BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // TODO:  在此添加额外的初始化
    m_battery_type_combo.AddString(g_data.StringRes(IDS_BATTYER_TYPE_NUMBER));
    m_battery_type_combo.AddString(g_data.StringRes(IDS_BATTYER_TYPE_ICON));
    m_battery_type_combo.AddString(g_data.StringRes(IDS_BATTYER_TYPE_NUMBER_BESIDE_ICON));
    m_battery_type_combo.SetCurSel(static_cast<int>(m_data.battery_type));

    CheckDlgButton(IDC_SHOW_TOOLTIPS_CHECK, m_data.show_battery_in_tooltip);
    CheckDlgButton(IDC_SHOW_PERCENT_CHECK, m_data.show_percent);
    CheckDlgButton(IDC_SHOW_CHARGING_ANIMATION_CHECK, m_data.show_charging_animation);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void COptionsDlg::OnCbnSelchangeCombo1()
{
    m_data.battery_type = static_cast<BatteryType>(m_battery_type_combo.GetCurSel());
}


void COptionsDlg::OnBnClickedShowTooltipsCheck()
{
    m_data.show_battery_in_tooltip = (IsDlgButtonChecked(IDC_SHOW_TOOLTIPS_CHECK) != 0);
}


void COptionsDlg::OnBnClickedShowPercentCheck()
{
    m_data.show_percent = (IsDlgButtonChecked(IDC_SHOW_PERCENT_CHECK) != 0);
}


void COptionsDlg::OnBnClickedShowChargingAnimationCheck()
{
    m_data.show_charging_animation = (IsDlgButtonChecked(IDC_SHOW_CHARGING_ANIMATION_CHECK) != 0);
}


void COptionsDlg::OnNMClickHelpSyslink(NMHDR* pNMHDR, LRESULT* pResult)
{
    ShellExecute(NULL, _T("open"), _T("https://github.com/zhongyang219/TrafficMonitorPlugins/wiki/%E7%94%B5%E6%B1%A0%E6%8F%92%E4%BB%B6"), NULL, NULL, SW_SHOW);
    *pResult = 0;
}
