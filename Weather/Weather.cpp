#include "pch.h"
#include "Weather.h"
#include "Common.h"
#include <fstream>
#include "DataManager.h"
#include "utilities/yyjson/yyjson.h"
#include "OptionsDlg.h"
#include <sstream>
#include "CurLocationHelper.h"

CWeather CWeather::m_instance;

CWeather::CWeather()
{
}

CWeather& CWeather::Instance()
{
    return m_instance;
}

template<class T>
static void StringNormalize(T& str)
{
    if (str.empty()) return;

    int size = str.size();  //字符串的长度
    if (size < 0) return;
    int index1 = 0;     //字符串中第1个不是空格或控制字符的位置
    int index2 = size - 1;  //字符串中最后一个不是空格或控制字符的位置
    while (index1 < size && str[index1] >= 0 && str[index1] <= 32)
        index1++;
    while (index2 >= 0 && str[index2] >= 0 && str[index2] <= 32)
        index2--;
    if (index1 > index2)    //如果index1 > index2，说明字符串全是空格或控制字符
        str.clear();
    else if (index1 == 0 && index2 == size - 1) //如果index1和index2的值分别为0和size - 1，说明字符串前后没有空格或控制字符，直接返回
        return;
    else
        str = str.substr(index1, index2 - index1 + 1);
}

UINT CWeather::ThreadCallback(LPVOID dwUser)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(m_instance.m_is_thread_runing);

    time_t cur_time = time(nullptr);
    if (cur_time - m_instance.m_last_request_time > 3)  //确保请求天气信息的时间距离上次请求时超过3秒
    {
        m_instance.m_last_request_time = cur_time;

        //禁用选项设置中的“更新”按钮
        m_instance.DisableUpdateWeatherCommand();

        CityCodeItem cur_city_item;
        if (g_data.m_setting_data.auto_locate)      //自动获取当前城市
        {
            std::wstring cur_city = CCurLocationHelper::GetCurrentCity();
            CCurLocationHelper::Location location = CCurLocationHelper::ParseCityName(cur_city);
            cur_city_item = CCurLocationHelper::FindCityCodeItem(location);
            g_data.m_auto_located_city = cur_city_item;
            g_data.m_auto_located = true;
            if (m_instance.m_option_dlg != nullptr)
                m_instance.m_option_dlg->UpdateAutoLocteResult();
        }
        if (cur_city_item.name.empty())
        {
            cur_city_item = g_data.CurCity();
        }

        //获取天气信息
        std::wstring url{ L"http://t.weather.itboy.net/api/weather/city/" };
        url += g_data.CurCity().code;
        std::string weather_data;
        if (CCommon::GetURL(url, weather_data))
        {
            m_instance.ParseJsonData(weather_data);
        }

        //启用选项设置中的“更新”按钮
        m_instance.EnableUpdateWeatherCommand();
    }
    return 0;
}

void ParseWeatherInfo(CDataManager::WeatherInfo& weather_info, yyjson_val* forecast)
{
    weather_info.m_type = CCommon::StrToUnicode(yyjson_get_str(yyjson_obj_get(forecast, "type")), true);
    weather_info.m_high = CCommon::StrToUnicode(yyjson_get_str(yyjson_obj_get(forecast, "high")), true);
    weather_info.m_low = CCommon::StrToUnicode(yyjson_get_str(yyjson_obj_get(forecast, "low")), true);

    //去掉前面的“高温”和“低温”
    weather_info.m_high = weather_info.m_high.substr(2);
    weather_info.m_low = weather_info.m_low.substr(2);
    //去年低温后面的摄氏度符号
    weather_info.m_low.pop_back();
    StringNormalize(weather_info.m_high);
    StringNormalize(weather_info.m_low);
}

void CWeather::ParseJsonData(std::string json_data)
{
    //int a = 0;
    std::ofstream stream{ g_data.m_config_dir + L"Weather.dll.log" };
    stream << json_data;

    g_data.ResetText();

    yyjson_doc* doc = yyjson_read(json_data.c_str(), json_data.size(), 0);
    if (doc != nullptr)
    {
        //获取Json根节点
        yyjson_val* root = yyjson_doc_get_root(doc);

        //获取城市
        yyjson_val* city_info = yyjson_obj_get(root, "cityInfo");
        yyjson_val* city = yyjson_obj_get(city_info, "city");
        std::wstring str_city = CCommon::StrToUnicode(yyjson_get_str(city), true);

        //获取当前天气
        yyjson_val* data = yyjson_obj_get(root, "data");
        g_data.m_weather_info[WEATHER_CURRENT].m_high = CCommon::StrToUnicode(yyjson_get_str(yyjson_obj_get(data, "wendu")), true) + L"℃";
        g_data.m_weather_info[WEATHER_CURRENT].is_cur_weather = true;

        //获取3天的天气
        yyjson_val* forecast_arr = yyjson_obj_get(data, "forecast");
        yyjson_val* forecast_today = yyjson_arr_get_first(forecast_arr);
        yyjson_val* forecast_tommorrow = yyjson_arr_get(forecast_arr, 1);
        yyjson_val* forecast_day2 = yyjson_arr_get(forecast_arr, 2);
        ParseWeatherInfo(g_data.m_weather_info[WEATHER_TODAY], forecast_today);
        ParseWeatherInfo(g_data.m_weather_info[WEATHER_TOMMORROW], forecast_tommorrow);
        ParseWeatherInfo(g_data.m_weather_info[WEATHER_DAY2], forecast_day2);
        g_data.m_weather_info[WEATHER_CURRENT].m_type = g_data.m_weather_info[WEATHER_TODAY].m_type;

        //生成鼠标提示字符串
        const CDataManager::WeatherInfo& weather_current{ g_data.m_weather_info[WEATHER_CURRENT] };
        const CDataManager::WeatherInfo& weather_today{ g_data.m_weather_info[WEATHER_TODAY] };
        const CDataManager::WeatherInfo& weather_tomorrow{ g_data.m_weather_info[WEATHER_TOMMORROW] };
        const CDataManager::WeatherInfo& weather_day2{ g_data.m_weather_info[WEATHER_DAY2] };
        std::wstringstream wss;
        wss << str_city << L' ' << weather_current.ToString()
            << std::endl << g_data.StringRes(IDS_TODAY_WEATHER).GetString() << L": " << weather_today.ToString()
            << std::endl << g_data.StringRes(IDS_TOMMORROW_WEATHER).GetString() << L": " << weather_tomorrow.ToString()
            << std::endl << g_data.StringRes(IDS_THE_DAY_AFTER_TOMMORROW_WEATHER).GetString() << L": " << weather_day2.ToString()
            ;
        m_tooltop_info = wss.str();

        yyjson_doc_free(doc);
    }
}

void CWeather::LoadContextMenu()
{
    if (m_menu.m_hMenu == NULL)
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_menu.LoadMenu(IDR_MENU1);
    }
}

IPluginItem* CWeather::GetItem(int index)
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

const wchar_t* CWeather::GetTooltipInfo()
{
    if (g_data.m_setting_data.m_show_weather_in_tooltips)
        return m_tooltop_info.c_str();
    else
        return L"";
}

void CWeather::DataRequired()
{
    static int last_hour{ -1 };
    SYSTEMTIME system_time{};
    GetLocalTime(&system_time);
    //每隔一小时获取一次天气
    if (system_time.wHour != last_hour)
    {
        last_hour = system_time.wHour;
        SendWetherInfoQequest();
    }

}

ITMPlugin::OptionReturn CWeather::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    //g_data.DPIFromWindow(pParent);
    COptionsDlg dlg(pParent);
    dlg.m_data = g_data.m_setting_data;
    m_option_dlg = &dlg;
    auto rtn = dlg.DoModal();
    m_option_dlg = nullptr;
    if (rtn == IDOK)
    {
        bool city_changed{ g_data.m_setting_data.m_city_index != dlg.m_data.m_city_index ||
            g_data.m_setting_data.auto_locate != dlg.m_data.auto_locate };
        g_data.m_setting_data = dlg.m_data;
        if (city_changed)
        {
            CWeather::Instance().SendWetherInfoQequest();   //城市改变后，重新发送天气请求
        }

        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* CWeather::GetInfo(PluginInfoIndex index)
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
        return L"Copyright (C) by Zhong Yang 2021";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
        break;
    case TMI_VERSION:
        return L"1.01";
    default:
        break;
    }
    return L"";
}

void CWeather::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
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

void CWeather::SendWetherInfoQequest()
{
    if (!m_is_thread_runing)    //确保线程已退出
        AfxBeginThread(ThreadCallback, nullptr);
}

void CWeather::ShowContextMenu(CWnd* pWnd)
{
    LoadContextMenu();
    CMenu* context_menu = m_menu.GetSubMenu(0);
    if (context_menu != nullptr)
    {
        CPoint point1;
        GetCursorPos(&point1);
        DWORD id = context_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point1.x, point1.y, pWnd);
        //点击了“选项”
        if (id == ID_OPTIONS)
        {
            AFX_MANAGE_STATE(AfxGetStaticModuleState());
            COptionsDlg dlg;
            dlg.m_data = g_data.m_setting_data;
            m_option_dlg = &dlg;
            auto rtn = dlg.DoModal();
            m_option_dlg = nullptr;
            if (rtn == IDOK)
            {
                bool city_changed{ g_data.m_setting_data.m_city_index != dlg.m_data.m_city_index };
                g_data.m_setting_data = dlg.m_data;
                if (city_changed)
                {
                    CWeather::Instance().SendWetherInfoQequest();   //城市改变后，重新发送天气请求
                }
            }
        }
        //点击了“更新天气”
        else if (id == ID_UPDATE_WEATHER)
        {
            SendWetherInfoQequest();
        }
    }
}

void CWeather::DisableUpdateWeatherCommand()
{
    if (m_option_dlg != nullptr)
        m_option_dlg->EnableUpdateBtn(false);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE_WEATHER, MF_BYCOMMAND | MF_GRAYED);
}

void CWeather::EnableUpdateWeatherCommand()
{
    if (m_instance.m_option_dlg != nullptr)
        m_instance.m_option_dlg->EnableUpdateBtn(true);
    if (m_menu.m_hMenu != NULL)
        m_menu.EnableMenuItem(ID_UPDATE_WEATHER, MF_BYCOMMAND | MF_ENABLED);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CWeather::Instance();
}
