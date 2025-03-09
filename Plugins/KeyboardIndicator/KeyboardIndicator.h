#pragma once
#include "PluginInterface.h"
#include "KeyboardIndicatorItem.h"
#include <string>

class CKeyboardIndicator : public ITMPlugin
{
private:
    CKeyboardIndicator();

public:
    static CKeyboardIndicator& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void* GetPluginIcon() override;

    static bool IsCapsLockOn();
    static bool IsNumLockOn();
    static bool IsScrollLockOn();
private:

private:
    static CKeyboardIndicator m_instance;
    CKeyboardIndicatorItem m_item;
    std::wstring m_tooltip_info;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
