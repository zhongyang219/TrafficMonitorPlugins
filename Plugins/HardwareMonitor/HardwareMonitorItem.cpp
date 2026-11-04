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

    const wchar_t* CHardwareMonitorItem::GetTooltipValueText() const
    {
        //显存项目固定显示完整格式（已占用/总量）；
        //其他项目沿用GetItemValueText，按当前窗口返回对应数值
        if (HardwareMonitorHelper::IsGpuMemoryItemIdentifier(identifier))
            return item_value_main_wnd.c_str();
        return GetItemValueText();
    }

    const wchar_t* CHardwareMonitorItem::GetItemValueSampleText() const
    {
        static std::wstring sample_text;

        //显存项目由两个数值组成，单独生成示例文本
        if (HardwareMonitorHelper::IsGpuMemoryItemIdentifier(identifier))
        {
            const ItemInfo& item_info{ CHardwareMonitor::GetInstance()->m_settings.FindItemInfo(identifier) };
            String^ unit = item_info.unit.empty() ? gcnew String(L"G") : gcnew String(item_info.unit.c_str());
            bool use_percent = unit->Equals(gcnew String(L"%"), StringComparison::OrdinalIgnoreCase);
            //显存单位文本（G不带空格、GB前带空格、%遵循“数值和单位使用空格分隔”设置）
            std::wstring unit_text = Common::StringToStdWstring(HardwareMonitorHelper::GetGpuMemoryUnitText(unit, item_info.show_unit, CHardwareMonitor::GetInstance()->m_tm_settings.sperate_with_space()));
            sample_text.clear();

            //任务栏窗口固定显示短格式，因此示例文本也用短格式文本，保证计算出的宽度不会被完整格式撑开
            if (CHardwareMonitor::GetInstance()->m_tm_settings.is_taskbar)
            {
                sample_text = GetItemValueText();
                if (sample_text.empty())
                {
                    //数值尚未就绪时，按单位估算示例文本
                    if (use_percent)
                        sample_text = L"100" + unit_text;
                    else
                    {
                        sample_text = L"0";
                        if (item_info.decimal_places > 0)
                        {
                            sample_text += L'.';
                            sample_text += std::wstring(item_info.decimal_places, L'0');
                        }
                        sample_text += unit_text;
                    }
                }
                return sample_text.c_str();
            }

            //单位为%时，完整格式也是百分比文本，示例文本按100%估算宽度
            if (use_percent)
            {
                sample_text = L"100" + unit_text;
                return sample_text.c_str();
            }

            if (!item_info.specify_value_width)
            {
                //不指定数值宽度时，使用当前的实际数值作为示例文本，使显示区域刚好足够宽
                sample_text = GetItemValueText();
                if (sample_text.empty())
                    sample_text = L"0/0" + unit_text;
                return sample_text.c_str();
            }
            //指定数值宽度时，根据实际的总显存估算示例数值。
            //已占用显存不会超过总显存，这样可以保证显示区域不会被过宽的示例文本撑开
            float used_gb{};
            float total_gb{};
            if (HardwareMonitorHelper::GetGpuMemoryValues(identifier, used_gb, total_gb))
            {
                bool use_mb = unit->Equals(gcnew String(L"MB"), StringComparison::OrdinalIgnoreCase);
                float total = use_mb ? total_gb * 1024.0f : total_gb;
                String^ used_format = String::Format("F{0}", item_info.decimal_places);
                sample_text = Common::StringToStdWstring(total.ToString(used_format));
                sample_text += L'/';
                //总显存显示为整数
                sample_text += Common::StringToStdWstring(total.ToString("F0"));
            }
            else
            {
                std::wstring number_sample(item_info.value_width, L'0');
                if (item_info.decimal_places > 0)
                {
                    number_sample += L'.';
                    number_sample += std::wstring(item_info.decimal_places, L'0');
                }
                sample_text = number_sample;
                sample_text += L'/';
                sample_text += std::wstring(item_info.value_width, L'0');
            }
            sample_text += unit_text;
            return sample_text.c_str();
        }

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
        //显存项目绘制占用背景条，数值为 已占用显存/总显存，范围 0.0~1.0
        if (HardwareMonitorHelper::IsGpuMemoryItemIdentifier(identifier))
            return 1;
        SensorType type = static_cast<SensorType>(sensor_type);
        return type == SensorType::Temperature || type == SensorType::Load;
    }

    float CHardwareMonitorItem::GetResourceUsageGraphValue() const
    {
        if (HardwareMonitorHelper::IsGpuMemoryItemIdentifier(identifier))
        {
            float used_gb;
            float total_gb;
            if (HardwareMonitorHelper::GetGpuMemoryValues(identifier, used_gb, total_gb) && total_gb > 0)
                return used_gb / total_gb;
            return 0.0f;
        }
        SensorType type = static_cast<SensorType>(sensor_type);
        if (type == SensorType::Temperature || type == SensorType::Load)
        {
            return item_value_num / 100.0f;
        }
        return 0.0f;
    }

    void CHardwareMonitorItem::UpdateValue()
    {
        //显存项目不是一个真实的传感器，它的数值由“已占用显存”和“总显存”两个传感器组合而成
        if (HardwareMonitorHelper::IsGpuMemoryItemIdentifier(identifier))
        {
            const ItemInfo& item_info{ CHardwareMonitor::GetInstance()->m_settings.FindItemInfo(identifier) };
            const auto& cfg{ CHardwareMonitor::GetInstance()->m_tm_settings };
            //单位取自ItemInfo（std::wstring），无需转换为托管字符串
            static const std::wstring default_unit = L"G";
            const std::wstring& unit = item_info.unit.empty() ? default_unit : item_info.unit;
            //底层数据只获取一次（传感器引用走缓存），同一份数值分别生成
            //主窗口（完整格式）和任务栏（短格式）两种文本，
            //避免重复的硬件查找、传感器匹配和数值获取
            float used_gb{};
            float total_gb{};
            if (HardwareMonitorHelper::GetGpuMemoryValues(identifier, used_gb, total_gb))
            {
                item_value_main_wnd = HardwareMonitorHelper::FormatGpuMemoryFullValueText(used_gb, total_gb, unit, item_info.decimal_places, item_info.show_unit, cfg.main_wnd_sperate_with_space);
                //任务栏窗口宽度有限（左右停靠时尤其如此），完整的“已用/总量”文本可能被裁切，
                //因此任务栏固定显示短格式：单位G/GB显示已占用数值（如“45.1G”），单位%显示百分比（如“94 %”），
                //完整值仍在鼠标提示中显示
                item_value_taskbar = HardwareMonitorHelper::FormatGpuMemoryShortValueText(used_gb, total_gb, unit, item_info.decimal_places, item_info.show_unit, cfg.sperate_with_space());
            }
            else
            {
                item_value_main_wnd = L"--";
                item_value_taskbar = L"--";
            }
            sensor_type = static_cast<int>(SensorType::Data);
            return;
        }

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

            //判断是否要显示通知
            item_value_with_unit = HardwareMonitorHelper::GetSensorValue(sensor, unit);
            if (item_info.show_notify)
            {
                if (last_item_value_with_unit < item_info.notify_value && item_value_with_unit >= item_info.notify_value)
                {
                    String^ format_str = MonitorGlobal::Instance()->GetString(L"ValueExceedNotifyMsg");
                    String^ value_with_unit = HardwareMonitorHelper::GetSensorValueText(sensor, unit);
                    String^ str_msg = String::Format(format_str, gcnew String(item_name.c_str()), value_with_unit);
                    CHardwareMonitor::GetInstance()->GetMainApp()->ShowNotifyMessage(Common::StringToStdWstring(str_msg).c_str());
                    last_item_value_with_unit = item_value_with_unit;
                }
            }
            last_item_value_with_unit = item_value_with_unit;
        }
    }

    const std::wstring& CHardwareMonitorItem::GetIdentifier() const
    {
        return identifier;
    }
}