#pragma once
#include "PluginInterface.h"
#include "LoudnessMeterItem.h"
#include <string>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>

class CLoudnessMeter : public ITMPlugin
{
private:
    CLoudnessMeter();
    ~CLoudnessMeter();

public:
    static CLoudnessMeter& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

private:

private:
    static CLoudnessMeter m_instance;
    CLoudnessMeterItem m_item;
    std::wstring m_tooltip_info;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioMeterInformation* pMeterInfo = NULL;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
