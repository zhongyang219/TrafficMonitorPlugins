#include "pch.h"
#include "DateTime.h"
#include "DataManager.h"
#include "OptionsDlg.h"

CDateTime CDateTime::m_instance;

CDateTime::CDateTime()
{
}

CDateTime& CDateTime::Instance()
{
    return m_instance;
}

IPluginItem* CDateTime::GetItem(int index)
{
    switch (index)
    {
    case 0:
        return &m_system_date;
    case 1:
        return &m_system_time;
    default:
        break;
    }
    return nullptr;
}

void CDateTime::DataRequired()
{
    g_data.m_format_helper.GetCurrentDateTime();
    g_data.m_cur_date = g_data.m_format_helper.GetCurrentDateTimeByFormat(g_data.m_setting_data.date_format.c_str());
    g_data.m_cur_time = g_data.m_format_helper.GetCurrentDateTimeByFormat(g_data.m_setting_data.time_format.c_str());
}

const wchar_t* CDateTime::GetInfo(PluginInfoIndex index)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        str.LoadString(IDS_PLUGIN_NAME);
        return str.GetString();
    case TMI_DESCRIPTION:
        str.LoadString(IDS_PLUGIN_DESCRIPTION);
        return str.GetString();
    case TMI_AUTHOR:
        return L"zhongyang219";
    case TMI_COPYRIGHT:
        return L"Copyright (C) by Zhong Yang 2021";
    case TMI_VERSION:
        return L"1.00";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
        break;
    default:
        break;
    }
    return L"";
}

ITMPlugin::OptionReturn CDateTime::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    COptionsDlg dlg(CWnd::FromHandle((HWND)hParent));
    dlg.m_data = CDataManager::Instance().m_setting_data;
    if (dlg.DoModal() == IDOK)
    {
        CDataManager::Instance().m_setting_data = dlg.m_data;
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}


void CDateTime::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
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

void* CDateTime::GetPluginIcon()
{
    return g_data.GetIcon(IDI_ICON1);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CDateTime::Instance();
}
