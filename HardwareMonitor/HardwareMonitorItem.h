#pragma once
#include "PluginInterface.h"
#include <string>

namespace HardwareMonitor
{
    class CHardwareMonitorItem : public IPluginItem
    {
    public:
        CHardwareMonitorItem(const std::wstring& _identifier, const std::wstring& _item_name);

        void FromSensor(LibreHardwareMonitor::Hardware::ISensor^ sensor);

        // Í¨¹ý IPluginItem ¼Ì³Ð
        const wchar_t* GetItemName() const override;
        const wchar_t* GetItemId() const override;
        const wchar_t* GetItemLableText() const override;
        const wchar_t* GetItemValueText() const override;
        const wchar_t* GetItemValueSampleText() const override;

        void UpdateValue();
        const std::wstring& GetIdentifier() const;

    private:
        std::wstring identifier;
        std::wstring item_name;
        std::wstring item_value;
    };
}
