#include "stdafx.h"
#include "HardwareMonitorItem.h"
#include "HardwareMonitor.h"
#include "HardwareMonitorHelper.h"
#include "Common.h"

using namespace LibreHardwareMonitor::Hardware;

namespace HardwareMonitor
{
    CHardwareMonitorItem::CHardwareMonitorItem(const std::wstring& _identifier, const std::wstring& _item_name, const std::wstring& _label_text)
        : identifier(_identifier), item_name(_item_name), label_text(_label_text)
    {
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
        return label_text.c_str();
    }

    const wchar_t* CHardwareMonitorItem::GetItemValueText() const
    {
        if (CHardwareMonitor::GetInstance()->m_tm_settings.is_taskbar)
            return item_value_taskbar.c_str();
        else
            return item_value_main_wnd.c_str();
    }

    const wchar_t* CHardwareMonitorItem::GetItemValueSampleText() const
    {
        static std::wstring sample_text;
        const ItemInfo& item_info{ CHardwareMonitor::GetInstance()->m_settings.FindItemInfo(identifier) };
        if (item_info.specify_value_width)
        {
            sample_text = std::wstring(item_info.value_width, L'0');
            if (item_info.decimal_places > 0)
            {
                sample_text += L'.';
                sample_text += std::wstring(item_info.decimal_places, L'0');
            }
            if (item_info.show_unit)
            {
                if (CHardwareMonitor::GetInstance()->m_tm_settings.sperate_with_space())
                    sample_text += L' ';
                SensorType type = static_cast<SensorType>(sensor_type);
                std::wstring unit;
                if (!item_info.unit.empty())
                    unit = item_info.unit;
                else
                    unit = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorTypeDefaultUnit(type));
                sample_text += unit;
            }
        }
        else
        {
            std::wstring item_value(GetItemValueText());
            if (item_value.empty())
                sample_text = L"0.00";
            else
                sample_text = item_value;
        }
        return sample_text.c_str();
    }

    int CHardwareMonitorItem::IsDrawResourceUsageGraph() const
    {
        SensorType type = static_cast<SensorType>(sensor_type);
        return type == SensorType::Temperature || type == SensorType::Load;
    }

    float CHardwareMonitorItem::GetResourceUsageGraphValue() const
    {
        SensorType type = static_cast<SensorType>(sensor_type);
        if (type == SensorType::Temperature || type == SensorType::Load)
        {
            return item_value_num / 100.0f;
        }
        return 0.0f;
    }

    void CHardwareMonitorItem::UpdateValue()
    {
        ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(identifier.c_str()));
        if (sensor != nullptr)
        {
            const ItemInfo& item_info{ CHardwareMonitor::GetInstance()->m_settings.FindItemInfo(identifier) };
            String^ unit;
            if (!item_info.unit.empty())
                unit = gcnew String(item_info.unit.c_str());
            else
                unit = HardwareMonitorHelper::GetSensorTypeDefaultUnit(sensor->SensorType);
            const auto& cfg{ CHardwareMonitor::GetInstance()->m_tm_settings };
            item_value_main_wnd = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorValueText(sensor, unit, item_info.decimal_places, item_info.show_unit, cfg.main_wnd_sperate_with_space));
            item_value_taskbar = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorValueText(sensor, unit, item_info.decimal_places, item_info.show_unit, cfg.taskbar_sperate_with_space));
            try
            {
                item_value_num = sensor->Value.Value;
            }
            catch (System::Exception^)
            {
                item_value_num = 0;
            }
            sensor_type = static_cast<int>(sensor->SensorType);
        }
    }

    const std::wstring& CHardwareMonitorItem::GetIdentifier() const
    {
        return identifier;
    }
}