#pragma once
#include <string>
class CommonData
{
};

enum WeahterSelected
{
    WEATHER_CURRENT,
    WEATHER_TODAY,
    WEATHER_TOMMORROW,
    WEATHER_DAY2
};

struct SettingData
{
    int m_city_index{};                     //选择的城市在列表中的序号
    WeahterSelected m_weather_selected{};   //要显示的天气
    bool m_show_weather_in_tooltips{};      //是否在鼠标提示中显示
    bool m_use_weather_icon{};
    //int m_display_width{};
    bool auto_locate{};         //自动定位
};

//一个天气信息
struct WeatherInfo
{
    std::wstring m_type = L"--";         //天气类型
    std::wstring m_high = L"-℃";        //最高温度
    std::wstring m_low = L"-℃";         //最低温度
    bool is_cur_weather{};                 //是否为当前温度，如果为true，则m_low无效
    std::wstring m_wind;                //风向和风力
    std::wstring ToString() const;
    std::wstring ToStringTemperature() const;
};
