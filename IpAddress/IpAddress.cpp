#include "pch.h"
#include "IpAddress.h"
#include "DataManager.h"
#include "OptionsDlg.h"

CIpAddress CIpAddress::m_instance;

CIpAddress::CIpAddress()
{
}

CIpAddress& CIpAddress::Instance()
{
    return m_instance;
}

IPluginItem* CIpAddress::GetItem(int index)
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

const wchar_t* CIpAddress::GetTooltipInfo()
{
    return m_tooltip_info.c_str();
}

void CIpAddress::DataRequired()
{
    //TODO: 在此添加获取监控数据的代码
}

ITMPlugin::OptionReturn CIpAddress::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    COptionsDlg dlg(pParent);
    dlg.m_data = g_data.m_setting_data;
    if (dlg.DoModal() == IDOK)
    {
        g_data.m_setting_data = dlg.m_data;
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* CIpAddress::GetInfo(PluginInfoIndex index)
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
    case TMI_VERSION:
        return L"1.00";
    default:
        break;
    }
    return L"";
}

void CIpAddress::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        //更新连接列表
        g_data.UpdateConnections();
        //从配置文件读取配置
        g_data.LoadConfig(std::wstring(data));
        break;
    default:
        break;
    }
}

void* CIpAddress::GetPluginIcon()
{
    return g_data.GetIcon(IDI_IP_ADDRESS);
}

int CIpAddress::GetCommandCount()
{
    return 1;
}

const wchar_t* CIpAddress::GetCommandName(int command_index)
{
    if (command_index == 0)
    {
        return g_data.StringRes(IDS_REFRESH_CONNECTION_LIST);
    }
    return L"";
}

void CIpAddress::OnPluginCommand(int command_index, void* hWnd, void* para)
{
    if (command_index == 0)
    {
        g_data.UpdateConnections();
    }
}

void* CIpAddress::GetCommandIcon(int command_index)
{
    return g_data.GetIcon(IDI_UPDATE);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CIpAddress::Instance();
}
