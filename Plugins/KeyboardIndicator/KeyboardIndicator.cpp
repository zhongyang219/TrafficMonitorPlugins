#include "pch.h"
#include "KeyboardIndicator.h"
#include "DataManager.h"
#include "OptionsDlg.h"

CKeyboardIndicator CKeyboardIndicator::m_instance;

CKeyboardIndicator::CKeyboardIndicator()
{
}

CKeyboardIndicator& CKeyboardIndicator::Instance()
{
    return m_instance;
}

IPluginItem* CKeyboardIndicator::GetItem(int index)
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

const wchar_t* CKeyboardIndicator::GetTooltipInfo()
{
    return m_tooltip_info.c_str();
}

void CKeyboardIndicator::DataRequired()
{
}

ITMPlugin::OptionReturn CKeyboardIndicator::ShowOptionsDialog(void* hParent)
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

const wchar_t* CKeyboardIndicator::GetInfo(PluginInfoIndex index)
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
        return L"Copyright (C) 2025 By ZhongYang";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
    case TMI_VERSION:
        return L"1.00";
    default:
        break;
    }
    return L"";
}

void CKeyboardIndicator::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
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

bool CKeyboardIndicator::IsCapsLockOn()
{
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

bool CKeyboardIndicator::IsNumLockOn()
{
    return (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
}

bool CKeyboardIndicator::IsScrollLockOn()
{
    return (GetKeyState(VK_SCROLL) & 0x0001) != 0;
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CKeyboardIndicator::Instance();
}
