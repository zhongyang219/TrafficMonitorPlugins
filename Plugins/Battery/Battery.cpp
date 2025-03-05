#include "pch.h"
#include "Battery.h"
#include "Common.h"
#include <fstream>
#include "DataManager.h"
#include "OptionsDlg.h"
#include <sstream>

CBattery CBattery::m_instance;

CBattery::CBattery()
{
}

CBattery& CBattery::Instance()
{
    return m_instance;
}

IPluginItem* CBattery::GetItem(int index)
{
    switch (index)
    {
    case 0:
        return &m_item;
    default:
        break;
    }
    return nullptr;
}

const wchar_t* CBattery::GetTooltipInfo()
{
    if (g_data.m_setting_data.show_battery_in_tooltip)
        return m_tooltop_info.c_str();
    else
        return L"";
}

void CBattery::DataRequired()
{
    //获取系统电量
    GetSystemPowerStatus(&g_data.m_sysPowerStatus);
    //g_data.m_sysPowerStatus.BatteryFlag = 1;
    //g_data.m_sysPowerStatus.BatteryLifePercent = 80;
    //生成鼠标提示信息
    std::wstringstream wss;
    wss << g_data.StringRes(IDS_BATTERY).GetString() << L": " << g_data.GetBatteryString();
    if (g_data.IsAcOnline())
        wss << L" " << g_data.StringRes(IDS_CHARGING).GetString();
    if (g_data.m_sysPowerStatus.BatteryLifeTime != -1)
        wss << std::endl << g_data.StringRes(IDS_BATTERY_LIFE_TIME).GetString() << L": " << CCommon::TimeFormat(g_data.m_sysPowerStatus.BatteryLifeTime);
    if (g_data.m_sysPowerStatus.BatteryFullLifeTime != -1)
        wss << std::endl << g_data.StringRes(IDS_BATTERY_FULL_LIFE_TIME).GetString() << L": " << CCommon::TimeFormat(g_data.m_sysPowerStatus.BatteryFullLifeTime);
    m_tooltop_info = wss.str();
}

ITMPlugin::OptionReturn CBattery::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    COptionsDlg dlg(pParent);
    dlg.m_data = g_data.m_setting_data;
    if (dlg.DoModal() == IDOK)
    {
        g_data.m_setting_data = dlg.m_data;
        g_data.SaveConfig();
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* CBattery::GetInfo(PluginInfoIndex index)
{
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        return g_data.StringRes(IDS_PLUGIN_NAME).GetString();
    case TMI_DESCRIPTION:
        return g_data.StringRes(IDS_PLUGIN_DESCRIPTION).GetString();
    case TMI_AUTHOR:
        return L"zhongyang219";
    case TMI_COPYRIGHT:
        return L"Copyright (C) by Zhong Yang 2025";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
        break;
    case TMI_VERSION:
        return L"1.03";
    default:
        break;
    }
    return L"";
}

void CBattery::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        //从配置文件读取配置
        g_data.LoadConfig(std::wstring(data));

        break;
    default:
        break;
    }
}

void* CBattery::GetPluginIcon()
{
    return g_data.GetIcon(IDI_BATTERY_DARK);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CBattery::Instance();
}
