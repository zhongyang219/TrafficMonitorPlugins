#include "pch.h"
#include <wchar.h>
#include "PluginSystemDate.h"
#include "DataManager.h"

CPluginSystemDate::CPluginSystemDate()
{
}

const wchar_t* CPluginSystemDate::GetItemName() const
{
    return CDataManager::Instance().StringRes(IDS_DATE);
}

const wchar_t* CPluginSystemDate::GetItemId() const
{
    return L"o282ffc4";
}

const wchar_t* CPluginSystemDate::GetItemLableText() const
{
    return CDataManager::Instance().StringRes(IDS_DATE);
}

const wchar_t* CPluginSystemDate::GetItemValueText() const
{
    return CDataManager::Instance().m_cur_date.c_str();
}

const wchar_t* CPluginSystemDate::GetItemValueSampleText() const
{
    SYSTEMTIME sample_time{};
    GetLocalTime(&sample_time);
    sample_time.wHour = 10;
    sample_time.wMinute = 59;
    sample_time.wMinute = 59;
    return g_data.m_format_helper.GetDateTimeByFormat(g_data.m_setting_data.date_format.c_str(), sample_time);
}
