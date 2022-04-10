#pragma once

#include <include/PluginInterface.h>
#include <string>
#include <ctime>

#include "WeatherProItem.h"

class CWeatherPro : public ITMPlugin
{
private:
    CWeatherPro();

public:
    static CWeatherPro& Instance();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

    void UpdateWeatherInfo(bool force = false);
    void UpdateTooltip(const std::wstring &info);

    std::time_t GetLastUpdateTimestamp() const;

private:
    static CWeatherPro m_instance;

    CWeatherProItem m_item;
    std::wstring m_tooltips_info;

    std::time_t m_last_update_timestamp;
    std::time_t m_next_update_time_span;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
