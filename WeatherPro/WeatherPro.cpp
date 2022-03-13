// WeatherPro.cpp: 定义 DLL 的初始化例程。
//

#include "pch.h"
#include "framework.h"
#include "WeatherPro.h"

#include "OptionsDlg.h"

CWeatherPro CWeatherPro::m_instance;

CWeatherPro::CWeatherPro()
{}

CWeatherPro& CWeatherPro::Instance()
{
    return m_instance;
}

IPluginItem* CWeatherPro::GetItem(int index)
{
    // todo

    return nullptr;
}

void CWeatherPro::DataRequired()
{
    // todo
}

const wchar_t* CWeatherPro::GetInfo(PluginInfoIndex index)
{
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        return L"WeatherPro";
    case TMI_DESCRIPTION:
        return L"用于显示天气的TrafficMonitor插件";
    case TMI_AUTHOR:
        return L"Haojia521";
    case TMI_COPYRIGHT:
        return L"Copyright (C) by Haojia 2022";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
        break;
    case TMI_VERSION:
        return L"0.1";
    default:
        break;
    }
    return L"";
}

const wchar_t* CWeatherPro::GetTooltipInfo()
{
    // todo
    return L"";
}

ITMPlugin::OptionReturn CWeatherPro::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    CWnd *parent = CWnd::FromHandle((HWND)hParent);
    COptionsDlg dlg(parent);
    dlg.DoModal();

    return ITMPlugin::OR_OPTION_UNCHANGED;
}

void CWeatherPro::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    // todo
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CWeatherPro::Instance();
}
