#pragma once
#include "PluginInterface.h"
#include "BatteryItem.h"
#include <string>

class CBattery : public ITMPlugin
{
private:
    CBattery();

public:
    static CBattery& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void* GetPluginIcon() override;

private:

private:
    static CBattery m_instance;
    CBatteryItem m_item;
    std::wstring m_tooltop_info;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
