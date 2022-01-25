#pragma once
#include "PluginInterface.h"
#include "PluginTemplateItem.h"
#include <string>

class CPluginTemplate : public ITMPlugin
{
private:
    CPluginTemplate();

public:
    static CPluginTemplate& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

private:

private:
    static CPluginTemplate m_instance;
    CPluginTemplateItem m_item;
    std::wstring m_tooltip_info;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
