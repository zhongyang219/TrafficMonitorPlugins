#pragma once
#include "PluginInterface.h"
#include "PluginSystemDate.h"
#include "PluginSystemTime.h"

class CDateTime : public ITMPlugin
{
private:
    CDateTime();

public:
    static CDateTime& Instance();

    // 通过 ITMPlugin 继承
    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    void* GetPluginIcon() override;

private:
    CPluginSystemDate m_system_date;
    CPluginSystemTime m_system_time;

    static CDateTime m_instance;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
