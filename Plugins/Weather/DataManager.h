#pragma once
#include <string>
#include <map>
#include "resource.h"
#include "CityCode.h"

#define g_data CDataManager::Instance()

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

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

public:
    static CDataManager& Instance();

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;
    const CString& StringRes(UINT id);      //根据资源id获取一个字符串资源
    void DPIFromWindow(CWnd* pWnd);
    int DPI(int pixel);
    float DPIF(float pixel);
    int RDPI(int pixel);
    HICON GetIcon(UINT id);
    CityCodeItem CurCity() const;
    void ResetText();
    HICON GetWeatherIcon(const std::wstring weather_type);

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
    WeatherInfo& GetWeather();

    std::map<WeahterSelected, WeatherInfo> m_weather_info;
    CTime m_update_time;        //更新时间
    float m_pm2_5{};            //PM2.5
    std::wstring m_quality;     //空气质量

    CString GetUpdateTimeAsString();  //获取更新日间的字符串格式
    CString GetPM25AsString() const;

    SettingData m_setting_data;
    std::wstring m_config_dir;
    bool m_auto_located{};          //是否执行了自动定位
    bool m_auto_locate_succeed{};   //自动定位是否成功

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    std::map<std::wstring, UINT> m_weather_icon_id;     //保存所有天气图标的ID
    int m_dpi{ 96 };
};
