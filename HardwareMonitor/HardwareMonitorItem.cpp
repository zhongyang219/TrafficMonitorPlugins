#include "stdafx.h"
#include "HardwareMonitorItem.h"
#include "HardwareMonitor.h"
#include "HardwareMonitorHelper.h"

using namespace LibreHardwareMonitor::Hardware;

namespace HardwareMonitor
{
    CHardwareMonitorItem::CHardwareMonitorItem(const std::wstring& _identifier, const std::wstring& _item_name)
        : identifier(_identifier), item_name(_item_name)
    {
    }

    void CHardwareMonitorItem::FromSensor(LibreHardwareMonitor::Hardware::ISensor^ sensor)
    {
        identifier = MonitorGlobal::ClrStringToStdWstring(sensor->Identifier->ToString());

    }

    const wchar_t* CHardwareMonitorItem::GetItemName() const
    {
        return item_name.c_str();
    }

    const wchar_t* CHardwareMonitorItem::GetItemId() const
    {
        static std::wstring item_id;
        item_id = L"7KrUeFVl_" + identifier;
        return item_id.c_str();
    }

    const wchar_t* CHardwareMonitorItem::GetItemLableText() const
    {
        return item_name.c_str();
    }

    const wchar_t* CHardwareMonitorItem::GetItemValueText() const
    {
        return item_value.c_str();
    }

    const wchar_t* CHardwareMonitorItem::GetItemValueSampleText() const
    {
        static std::wstring sample_text;
        if (item_value.empty())
            sample_text = L"99.99 °„C";
        else
            sample_text = item_value + L' ';
        return sample_text.c_str();
    }

    void CHardwareMonitorItem::UpdateValue()
    {
        ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(identifier.c_str()));
        if (sensor != nullptr)
        {
            item_value = MonitorGlobal::ClrStringToStdWstring(HardwareMonitorHelper::GetSensorValueText(sensor));
        }
    }

    const std::wstring& CHardwareMonitorItem::GetIdentifier() const
    {
        return identifier;
    }
}