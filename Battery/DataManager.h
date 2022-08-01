#pragma once
#include <string>
#include <map>
#include "resource.h"

#define g_data CDataManager::Instance()

#define BATTERY_COLOR_HIGH RGB(128, 194, 105)   //电池电量颜色——高
#define BATTERY_COLOR_LOW RGB(241, 197, 0)      //电池电量颜色——低
#define BATTERY_COLOR_CRITICAL RGB(206, 23, 0)  //电池电量颜色——电量不足

//电量显示样式
enum class BatteryType
{
    NUMBER,             //仅数字
    ICON,               //仅图标
    NUMBER_BESIDE_ICON  //数字显示在图标旁边
};

struct SettingData
{
    BatteryType battery_type{};
    bool show_battery_in_tooltip{};
    bool show_percent{};
    bool show_charging_animation{};     //显示充电动画
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
    bool IsAcOnline() const;        //电源是否已接通
    bool IsCharging() const;        //是否正在充电

    std::wstring GetBatteryString() const;
    COLORREF GetBatteryColor() const;

    SettingData m_setting_data;
    SYSTEM_POWER_STATUS m_sysPowerStatus{};   // 系统电量信息
    ULONG_PTR m_gdiplusToken;

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    int m_dpi{ 96 };
};
