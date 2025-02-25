#pragma once
#include "PluginInterface.h"
#include <string>
#include "CommonData.h"

namespace HardwareMonitor
{
    class CHardwareMonitorItem : public IPluginItem
    {
    public:
        CHardwareMonitorItem(const std::wstring& _identifier, const std::wstring& _item_name, const std::wstring& _label_text);

        // 通过 IPluginItem 继承
        const wchar_t* GetItemName() const override;
        const wchar_t* GetItemId() const override;
        const wchar_t* GetItemLableText() const override;
        const wchar_t* GetItemValueText() const override;
        const wchar_t* GetItemValueSampleText() const override;
        virtual int IsDrawResourceUsageGraph() const override;
        virtual float GetResourceUsageGraphValue() const override;

        void UpdateValue();
        const std::wstring& GetIdentifier() const;

    private:
        std::wstring identifier;
        std::wstring item_name;
        std::wstring item_value;
        std::wstring label_text;
        int sensor_type{};
        float item_value_num{};
    };
}
