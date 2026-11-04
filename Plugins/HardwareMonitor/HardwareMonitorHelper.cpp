#include "stdafx.h"
#include "HardwareMonitorHelper.h"
#include "HardwareMonitor.h"
#include <gcroot.h>
#include <unordered_map>
#include <wchar.h>
#include <vector>
#include <algorithm>
#include "Common.h"

using namespace LibreHardwareMonitor::Hardware;
using namespace System::Collections::Generic;
using namespace System;

namespace HardwareMonitor
{
    String^ HardwareMonitorHelper::GetSensorTypeName(SensorType type)
    {
        switch (type)
        {
        case SensorType::Voltage: return MonitorGlobal::Instance()->GetString(L"Voltage");
        case SensorType::Current: return MonitorGlobal::Instance()->GetString(L"Current");
        case SensorType::Power: return MonitorGlobal::Instance()->GetString(L"Power");
        case SensorType::Clock: return MonitorGlobal::Instance()->GetString(L"Clock");
        case SensorType::Temperature: return MonitorGlobal::Instance()->GetString(L"Temperature");
        case SensorType::Load: return MonitorGlobal::Instance()->GetString(L"Load");
        case SensorType::Frequency: return MonitorGlobal::Instance()->GetString(L"Frequency");
        case SensorType::Fan: return MonitorGlobal::Instance()->GetString(L"Fan");
        case SensorType::Flow: return MonitorGlobal::Instance()->GetString(L"Flow");
        case SensorType::Control: return MonitorGlobal::Instance()->GetString(L"Control");
        case SensorType::Level: return MonitorGlobal::Instance()->GetString(L"Level");
        case SensorType::Factor: return MonitorGlobal::Instance()->GetString(L"Factor");
        case SensorType::Data: return MonitorGlobal::Instance()->GetString(L"Data");
        case SensorType::SmallData: return MonitorGlobal::Instance()->GetString(L"Data");
        case SensorType::Throughput: return MonitorGlobal::Instance()->GetString(L"Throughput");
        case SensorType::TimeSpan: return MonitorGlobal::Instance()->GetString(L"TimeSpan");
        case SensorType::Energy: return MonitorGlobal::Instance()->GetString(L"Energy");
        case SensorType::Noise: return MonitorGlobal::Instance()->GetString(L"Noise");
        case SensorType::Conductivity: return MonitorGlobal::Instance()->GetString(L"Conductivity");
        case SensorType::Humidity: return MonitorGlobal::Instance()->GetString(L"Humidity");
        }
        return L"";
    }

    String^ HardwareMonitorHelper::GetHardwareIconResName(LibreHardwareMonitor::Hardware::HardwareType type)
    {
        switch (type)
        {
        case HardwareType::Motherboard: return L"MotherBoard";
        case HardwareType::Battery: return L"batteries";
        case HardwareType::Cpu: return L"CPU";
        case HardwareType::EmbeddedController: return L"FanColtroller";
        case HardwareType::Network: return L"Network";
        case HardwareType::Memory: return L"RAM";
        case HardwareType::Storage: return L"Storage";
        case HardwareType::GpuAmd: return L"AMD";
        case HardwareType::GpuIntel: return L"intel";
        case HardwareType::GpuNvidia: return L"Nvidia";
        }
        return L"";
    }

    List<String^>^ HardwareMonitorHelper::GetSensorTypeUnit(SensorType type)
    {
        List<String^>^ unitList = gcnew List<String^>();
        switch (type)
        {
        case SensorType::Voltage: unitList->Add("V"); unitList->Add("mV"); break;
        case SensorType::Current: unitList->Add("A"); unitList->Add("mA"); break;
        case SensorType::Power: unitList->Add("W"); unitList->Add("mW"); break;
        case SensorType::Clock: unitList->Add("MHz"); unitList->Add("GHz"); break;
        case SensorType::Temperature: unitList->Add("°C"); unitList->Add("°F"); break;
        case SensorType::Load: unitList->Add("%"); break;
        case SensorType::Frequency: unitList->Add("Hz"); break;
        case SensorType::Fan: unitList->Add("RPM"); break;
        case SensorType::Flow: unitList->Add("L/h"); break;
        case SensorType::Control: unitList->Add("%"); break;
        case SensorType::Level: unitList->Add("%"); break;
        case SensorType::Factor: unitList->Add(""); break;
        case SensorType::Data: unitList->Add("GB"); unitList->Add("MB"); break;
        case SensorType::SmallData: unitList->Add("MB"); unitList->Add("GB"); break;
        case SensorType::Throughput: unitList->Add("KB/s"); unitList->Add("MB/s"); break;
        case SensorType::TimeSpan: unitList->Add("s"); break;
        case SensorType::Energy: unitList->Add("mWh"); unitList->Add("Wh"); break;
        case SensorType::Noise: unitList->Add("dBA"); break;
        case SensorType::Conductivity: unitList->Add(L"µS/cm"); break;
        case SensorType::Humidity: unitList->Add("%"); break;
        default: unitList->Add(""); break;
        }
        return unitList;
    }

    String^ HardwareMonitorHelper::GetSensorTypeDefaultUnit(LibreHardwareMonitor::Hardware::SensorType type)
    {
        List<String^>^ unitList = GetSensorTypeUnit(type);
        if (unitList->Count > 0)
            return unitList[0];
        return gcnew String("");
    }

    //查找一个硬件下的传感器
    //func为一个lambda表达式，原型为 bool(ISensor^)
    template<class Func>
    static ISensor^ FindSensorInHardware(IHardware^ hardware, Func func)
    {
        //遍历传感器
        for each (ISensor ^ sensor in hardware->Sensors)
        {
            if (func(sensor))
                return sensor;
        }
        //遍历子硬件
        for each (IHardware ^ sub_hardware in hardware->SubHardware)
        {
            ISensor^ sensor = FindSensorInHardware(sub_hardware, func);
            if (sensor != nullptr)
                return sensor;
        }
        return nullptr;

    }

    ISensor^ HardwareMonitorHelper::FindSensorByIdentifyer(String^ identifyer)
    {
        ISensor^ sensor;
        if (!MonitorGlobal::Instance()->sensorMap->TryGetValue(identifyer, sensor))
        {
            auto computer = MonitorGlobal::Instance()->computer;
            gcroot<String^> _identifyer = identifyer;       //使用gcroot包装托管类指针，以便被lambda捕获
            for each (IHardware^ hardware in computer->Hardware)
            {
                sensor = FindSensorInHardware(hardware, [&](ISensor^ _sensor) {
                    return GetSensorIdentifyer(_sensor) == _identifyer;
                });
                if (sensor != nullptr)
                    break;
            }
            MonitorGlobal::Instance()->sensorMap->Add(identifyer, sensor);
        }
        return sensor;
    }

    float HardwareMonitorHelper::GetSensorValue(LibreHardwareMonitor::Hardware::ISensor^ sensor, System::String^ unit)
    {
        if (sensor == nullptr || !sensor->Value.HasValue)
            return 0.0f;
        float value = sensor->Value.Value;
        //电压
        if (sensor->SensorType == SensorType::Voltage)
        {
            if (unit->Equals("mV"))
                value *= 1000.0f;
        }
        //电流
        else if (sensor->SensorType == SensorType::Current)
        {
            if (unit->Equals("mA"))
                value *= 1000.0f;
        }
        //功率
        else if (sensor->SensorType == SensorType::Power)
        {
            if (unit->Equals("mW"))
                value *= 1000.0f;
        }
        //时钟频率
        else if (sensor->SensorType == SensorType::Clock)
        {
            if (unit->Equals("GHz"))
                value /= 1024.0f;
        }
        //温度
        else if (sensor->SensorType == SensorType::Temperature)
        {
            if (unit->Equals("°F"))
                value = (value * 9.0f / 5.0f) + 32.0f;
        }

        //数据（默认GB）
        else if (sensor->SensorType == SensorType::Data)
        {
            if (unit->Equals("MB"))
                value *= 1024.0f;
        }
        //数据（默认MB）
        else if (sensor->SensorType == SensorType::SmallData)
        {
            if (unit->Equals("GB"))
                value /= 1024.0f;
        }
        //速率
        else if (sensor->SensorType == SensorType::Throughput)
        {
            if (unit->Equals("MB/s")) //MB/s
                value /= (1024.0f * 1024.0f);
            else                    //KB/s
                value /= 1024.0f;
        }
        //电量
        else if (sensor->SensorType == SensorType::Energy)
        {
            if (unit->Equals("Wh"))
                value /= 1024.0f;
        }
        if (sensor->SensorType == SensorType::Power && sensor->Name == L"Discharge Rate")   //放电功率显示为负数
            value = -value;
        if (sensor->SensorType == SensorType::Current && sensor->Name == L"Discharge Current")   //放电电流显示为负数
            value = -value;
        return value;
    }

    String^ HardwareMonitorHelper::GetSensorValueText(ISensor^ sensor, String^ unit, int decimal_place, bool show_unit, bool seperate_with_space)
    {
        String^ sensor_str;
        if (sensor->Value.HasValue)
        {
            float value = GetSensorValue(sensor, unit);
            String^ formatString = String::Format("F{0}", decimal_place);
            sensor_str += value.ToString(formatString);
        }
        else
        {
            sensor_str += "--";
        }
        if (show_unit)
        {
            if (seperate_with_space)
                sensor_str += " ";
            sensor_str += unit;
        }
        return sensor_str;
    }

    String^ HardwareMonitorHelper::GetSensorDisplayName(ISensor^ sensor)
    {
        String^ name;
        IHardware^ hardware = sensor->Hardware;
        name = hardware->Name;
        name += "/";
        name += GetSensorTypeName(sensor->SensorType);
        name += "/";
        name += Common::GetTranslatedString(sensor->Name);
        return name;
    }

    String^ HardwareMonitorHelper::GetSensorIdentifyer(ISensor^ sensor)
    {
        //不使用sensor->Identifier，因为sensor->Identifier代表的传感器并不总是固定的
        //return sensor->Identifier->ToString();

        String^ path;
        IHardware^ hardware = sensor->Hardware;
        while (hardware != nullptr)
        {
            //Hardware部分仍然使用它自己的Identifier
            path = hardware->Identifier + "/" + path;
            hardware = hardware->Parent;
        }
        //Sensor部分使用“类型+名称”
        path += sensor->SensorType.ToString();
        path += "/";
        path += sensor->Name;
        return path;
    }

    //////////////////////////////////////////////////////////////////////////
    // 显卡显存项目
    //////////////////////////////////////////////////////////////////////////

    static String^ GpuMemoryItemPrefix()
    {
        static gcroot<String^> prefix(gcnew String(L"__gpu_memory__/"));
        return prefix;
    }

    static const std::wstring& GpuMemoryItemPrefixW()
    {
        static const std::wstring prefix(L"__gpu_memory__/");
        return prefix;
    }

    bool HardwareMonitorHelper::IsGpuHardware(IHardware^ hardware)
    {
        if (hardware == nullptr)
            return false;
        return hardware->HardwareType == HardwareType::GpuNvidia
            || hardware->HardwareType == HardwareType::GpuAmd
            || hardware->HardwareType == HardwareType::GpuIntel;
    }

    String^ HardwareMonitorHelper::GetGpuMemoryItemIdentifier(IHardware^ hardware)
    {
        if (hardware == nullptr)
            return nullptr;
        return GpuMemoryItemPrefix() + hardware->Identifier->ToString();
    }

    bool HardwareMonitorHelper::IsGpuMemoryItemIdentifier(String^ identifier)
    {
        if (identifier == nullptr)
            return false;
        return identifier->StartsWith(GpuMemoryItemPrefix());
    }

    bool HardwareMonitorHelper::IsGpuMemoryItemIdentifier(const std::wstring& identifier)
    {
        const std::wstring& prefix = GpuMemoryItemPrefixW();
        return identifier.size() >= prefix.size() && identifier.compare(0, prefix.size(), prefix) == 0;
    }

    String^ HardwareMonitorHelper::GetGpuMemoryHardwareIdentifier(String^ identifier)
    {
        if (!IsGpuMemoryItemIdentifier(identifier))
            return nullptr;
        String^ prefix = GpuMemoryItemPrefix();
        return identifier->Substring(prefix->Length);
    }

    std::wstring HardwareMonitorHelper::GetGpuMemoryHardwareIdentifier(const std::wstring& identifier)
    {
        if (!IsGpuMemoryItemIdentifier(identifier))
            return std::wstring();
        return identifier.substr(GpuMemoryItemPrefixW().size());
    }

    static IHardware^ FindHardwareInHardware(IHardware^ hardware, String^ identifier)
    {
        if (hardware == nullptr || identifier == nullptr)
            return nullptr;
        if (hardware->Identifier->ToString()->Equals(identifier))
            return hardware;
        for each (IHardware ^ sub_hardware in hardware->SubHardware)
        {
            IHardware^ result = FindHardwareInHardware(sub_hardware, identifier);
            if (result != nullptr)
                return result;
        }
        return nullptr;
    }

    IHardware^ HardwareMonitorHelper::FindHardwareByIdentifier(String^ identifier)
    {
        if (identifier == nullptr)
            return nullptr;
        for each (IHardware ^ hardware in MonitorGlobal::Instance()->computer->Hardware)
        {
            IHardware^ result = FindHardwareInHardware(hardware, identifier);
            if (result != nullptr)
                return result;
        }
        return nullptr;
    }

    static ISensor^ FindMemorySensorByName(IHardware^ hardware, String^ name)
    {
        if (hardware == nullptr || name == nullptr)
            return nullptr;
        for each (ISensor ^ sensor in hardware->Sensors)
        {
            if ((sensor->SensorType == SensorType::Data || sensor->SensorType == SensorType::SmallData)
                && sensor->Name->Equals(name))
            {
                return sensor;
            }
        }
        for each (IHardware ^ sub_hardware in hardware->SubHardware)
        {
            ISensor^ sensor = FindMemorySensorByName(sub_hardware, name);
            if (sensor != nullptr)
                return sensor;
        }
        return nullptr;
    }

    //查找一个显卡的“已占用显存”和“总显存”传感器
    static bool FindGpuMemorySensors(IHardware^ hardware, ISensor^% used, ISensor^% total)
    {
        used = nullptr;
        total = nullptr;
        if (hardware == nullptr)
            return false;

        //NVIDIA/AMD独立显卡
        used = FindMemorySensorByName(hardware, "GPU Memory Used");
        total = FindMemorySensorByName(hardware, "GPU Memory Total");

        //D3D专用显存
        if (used == nullptr || total == nullptr)
        {
            ISensor^ d3d_used = FindMemorySensorByName(hardware, "D3D Dedicated Memory Used");
            ISensor^ d3d_total = FindMemorySensorByName(hardware, "D3D Dedicated Memory Total");
            if (d3d_used != nullptr && d3d_total != nullptr)
            {
                used = d3d_used;
                total = d3d_total;
            }
        }

        //共享显存（Intel核显等）
        if (used == nullptr || total == nullptr)
        {
            ISensor^ shared_used = FindMemorySensorByName(hardware, "D3D Shared Memory Used");
            ISensor^ shared_total = FindMemorySensorByName(hardware, "D3D Shared Memory Total");
            if (shared_used != nullptr && shared_total != nullptr)
            {
                used = shared_used;
                total = shared_total;
            }
        }

        //最后再尝试按名称匹配
        if (used == nullptr || total == nullptr)
        {
            //匹配串提升为函数内静态，避免循环内为每个传感器分配托管字符串
            static gcroot<String^> s_memory_used(gcnew String(L"Memory Used"));
            static gcroot<String^> s_used(gcnew String(L"Used"));
            static gcroot<String^> s_total(gcnew String(L"Total"));
            for each (ISensor ^ sensor in hardware->Sensors)
            {
                if ((sensor->SensorType != SensorType::Data && sensor->SensorType != SensorType::SmallData)
                    || !sensor->Name->Contains(s_memory_used))
                {
                    continue;
                }
                String^ sensor_total_name = sensor->Name->Replace(s_used, s_total);
                ISensor^ sensor_total = FindMemorySensorByName(hardware, sensor_total_name);
                if (sensor_total != nullptr)
                {
                    used = sensor;
                    total = sensor_total;
                    break;
                }
            }
        }

        return used != nullptr && total != nullptr;
    }

    bool HardwareMonitorHelper::HasGpuMemorySensors(IHardware^ hardware)
    {
        ISensor^ used;
        ISensor^ total;
        return FindGpuMemorySensors(hardware, used, total);
    }

    //////////////////////////////////////////////////////////////////////////
    // 显存传感器引用缓存
    //
    // 正常情况下硬件树和传感器集合稳定，“identifier -> 硬件 + 已配对传感器”
    // 的解析结果只需执行一次，之后热路径（UpdateValue等）直接取缓存的传感器引用读取Value，
    // 不再重复遍历硬件树和按名称匹配传感器。
    // 查找失败（硬件不存在或没有可用的显存传感器）时不写缓存，
    // 占位项保持空引用，下次调用自动重新解析；
    // 硬件枚举结果可能变化时（如监控硬件设置更改后、显示器布局变化后GPU硬件被重新枚举）
    // 调用ClearGpuMemoryCache清空，下次调用重新解析。
    //////////////////////////////////////////////////////////////////////////
    struct GpuMemorySensorCache
    {
        //用gcroot持有托管引用，GC回收前会先注销这些句柄，防止引用被回收
        gcroot<IHardware^> hardware{ nullptr };
        gcroot<ISensor^> used{ nullptr };
        gcroot<ISensor^> total{ nullptr };
    };

    static std::unordered_map<std::wstring, GpuMemorySensorCache>& GpuMemorySensorCacheMap()
    {
        static std::unordered_map<std::wstring, GpuMemorySensorCache> cache;
        return cache;
    }

    void HardwareMonitorHelper::ClearGpuMemoryCache()
    {
        GpuMemorySensorCacheMap().clear();
    }

    void HardwareMonitorHelper::ClearSensorCache()
    {
        MonitorGlobal::Instance()->sensorMap->Clear();
    }

    void HardwareMonitorHelper::ClearGpuLoadSensorCache()
    {
        auto map = MonitorGlobal::Instance()->sensorMap;
        if (map == nullptr)
            return;

        //GetSensorIdentifyer() 生成的key格式：
        // 硬件路径（根硬件通常为 /gpu-...） + “/SensorType/Name”。
        // 这里只移除 GPU 的 Load 传感器（GPU 占用百分比），
        // 不清除 CPU、内存、主板等普通传感器缓存。
        String^ gpuPrefix = gcnew String(L"/gpu-");
        String^ loadSegment = gcnew String(L"/Load/");
        List<String^>^ keys = gcnew List<String^>(map->Keys);
        for each (String^ key in keys)
        {
            if (key != nullptr && key->StartsWith(gpuPrefix) && key->Contains(loadSegment))
                map->Remove(key);
        }
    }

    void HardwareMonitorHelper::ClearGpuRelatedCaches()
    {
        ClearGpuMemoryCache();
        ClearGpuLoadSensorCache();
    }

    void HardwareMonitorHelper::ClearAllCaches()
    {
        ClearGpuMemoryCache();
        ClearSensorCache();
    }

    bool HardwareMonitorHelper::GetGpuMemoryValues(const std::wstring& identifier, float% used_gb, float% total_gb)
    {
        used_gb = 0;
        total_gb = 0;
        if (!IsGpuMemoryItemIdentifier(identifier))
            return false;

        GpuMemorySensorCache& entry = GpuMemorySensorCacheMap()[identifier];
        //取出缓存的句柄（gcroot隐式转换为托管引用）
        ISensor^ used = entry.used;
        ISensor^ total = entry.total;

        //缓存未命中时，执行一次硬件树遍历 + 传感器名称匹配，成功后写入缓存
        if (used == nullptr || total == nullptr)
        {
            std::wstring hardware_identifier = GetGpuMemoryHardwareIdentifier(identifier);
            IHardware^ hardware = hardware_identifier.empty()
                ? nullptr
                : FindHardwareByIdentifier(gcnew String(hardware_identifier.c_str()));
            ISensor^ sensor_used;
            ISensor^ sensor_total;
            if (hardware != nullptr && FindGpuMemorySensors(hardware, sensor_used, sensor_total))
            {
                entry.hardware = hardware;
                entry.used = sensor_used;
                entry.total = sensor_total;
                used = sensor_used;
                total = sensor_total;
            }
        }
        if (used == nullptr || total == nullptr)
            return false;
        if (!used->Value.HasValue || !total->Value.HasValue)
            return false;

        //Data类型的数值单位为GB，SmallData类型的数值单位为MB
        float used_value = used->Value.Value;
        float total_value = total->Value.Value;
        if (used->SensorType == SensorType::SmallData)
            used_value /= 1024.0f;
        if (total->SensorType == SensorType::SmallData)
            total_value /= 1024.0f;

        used_gb = used_value;
        total_gb = total_value;
        return true;
    }

    bool HardwareMonitorHelper::GetGpuMemoryValues(String^ identifier, float% used_gb, float% total_gb)
    {
        return GetGpuMemoryValues(Common::StringToStdWstring(identifier), used_gb, total_gb);
    }

    //判断单位是否等于某个固定token（忽略大小写，Ordinal比较），
    //与常量比较时不分配托管字符串
    static bool GpuMemoryUnitEquals(const std::wstring& unit, const wchar_t* token)
    {
        const size_t token_len = wcslen(token);
        return unit.size() == token_len && _wcsnicmp(unit.c_str(), token, token_len) == 0;
    }

    //获取显存项目的单位文本（含与数字之间的空格约定）：
    //G为紧凑单位，与数字之间不加空格，如“45.1G”；
    //%遵循“数值和单位使用空格分隔”设置，与CPU/内存等百分比项目一致，如“94 %”；
    //选GB说明任务栏窗口宽度宽裕，单位前加空格排版更好，如“45.1 GB”
    static std::wstring GpuMemoryUnitText(const std::wstring& unit, bool show_unit, bool separate_with_space)
    {
        if (!show_unit)
            return std::wstring();
        if (GpuMemoryUnitEquals(unit, L"G"))
            return std::wstring(L"G");
        if (GpuMemoryUnitEquals(unit, L"%"))
            return separate_with_space ? std::wstring(L" %") : std::wstring(L"%");
        return std::wstring(L" ") + unit;
    }

    //按指定小数位数格式化数值（F格式，与原先ToString("Fn")行为一致，含当前区域的小数点分隔符）。
    //格式串F0~F15预生成并缓存，避免每次调用都分配托管字符串
    static std::wstring GpuMemoryFormatFloat(float value, int decimal_place)
    {
        //F格式的合法精度范围是0~15，配置异常时收敛到该范围
        if (decimal_place < 0)
            decimal_place = 0;
        else if (decimal_place > 15)
            decimal_place = 15;
        static gcroot<String^> formats[] =
        {
            gcnew String("F0"),  gcnew String("F1"),  gcnew String("F2"),  gcnew String("F3"),
            gcnew String("F4"),  gcnew String("F5"),  gcnew String("F6"),  gcnew String("F7"),
            gcnew String("F8"),  gcnew String("F9"),  gcnew String("F10"), gcnew String("F11"),
            gcnew String("F12"), gcnew String("F13"), gcnew String("F14"), gcnew String("F15")
        };
        return Common::StringToStdWstring(value.ToString(formats[decimal_place]));
    }

    String^ HardwareMonitorHelper::GetGpuMemoryValueText(String^ identifier, String^ unit, int decimal_place, bool show_unit, bool seperate_with_space)
    {
        std::wstring unit_w = Common::StringToStdWstring(unit);
        if (unit_w.empty())
            unit_w = L"G";
        float used_gb;
        float total_gb;
        if (!GetGpuMemoryValues(Common::StringToStdWstring(identifier), used_gb, total_gb))
            return gcnew String(L"--");
        return gcnew String(FormatGpuMemoryFullValueText(used_gb, total_gb, unit_w, decimal_place, show_unit, seperate_with_space).c_str());
    }

    String^ HardwareMonitorHelper::GetGpuMemoryUnitText(String^ unit, bool show_unit, bool seperate_with_space)
    {
        return gcnew String(GpuMemoryUnitText(Common::StringToStdWstring(unit), show_unit, seperate_with_space).c_str());
    }

    String^ HardwareMonitorHelper::GetGpuMemoryShortValueText(String^ identifier, String^ unit, int decimal_place, bool show_unit, bool seperate_with_space)
    {
        std::wstring unit_w = Common::StringToStdWstring(unit);
        if (unit_w.empty())
            unit_w = L"G";
        float used_gb;
        float total_gb;
        if (!GetGpuMemoryValues(Common::StringToStdWstring(identifier), used_gb, total_gb))
            return gcnew String(L"--");
        return gcnew String(FormatGpuMemoryShortValueText(used_gb, total_gb, unit_w, decimal_place, show_unit, seperate_with_space).c_str());
    }

    std::wstring HardwareMonitorHelper::FormatGpuMemoryFullValueText(float used_gb, float total_gb, const std::wstring& unit, int decimal_place, bool show_unit, bool separate_with_space)
    {
        //百分比单位：与CPU/内存等百分比项目保持一致，输出整数百分比，如“94 %”
        if (GpuMemoryUnitEquals(unit, L"%"))
        {
            int percent = (total_gb > 0) ? static_cast<int>(used_gb / total_gb * 100.0f + 0.5f) : 0;
            return std::to_wstring(percent) + GpuMemoryUnitText(unit, show_unit, separate_with_space);
        }

        //单位MB时以MB输出，G/GB以GB输出
        const bool use_mb = GpuMemoryUnitEquals(unit, L"MB");
        const float used_value = use_mb ? used_gb * 1024.0f : used_gb;
        const float total_value = use_mb ? total_gb * 1024.0f : total_gb;

        std::wstring text = GpuMemoryFormatFloat(used_value, decimal_place);
        text += L'/';
        //总显存显示为整数（市售显卡的显存容量均为整GB）
        text += GpuMemoryFormatFloat(total_value, 0);
        text += GpuMemoryUnitText(unit, show_unit, separate_with_space);
        return text;
    }

    std::wstring HardwareMonitorHelper::FormatGpuMemoryShortValueText(float used_gb, float total_gb, const std::wstring& unit, int decimal_place, bool show_unit, bool separate_with_space)
    {
        //百分比单位，与CPU/内存等百分比项目保持一致：整数百分比（不加小数点），
        //是否带空格遵循“数值和单位使用空格分隔”设置，如“98 %”
        if (GpuMemoryUnitEquals(unit, L"%"))
        {
            int percent = (total_gb > 0) ? static_cast<int>(used_gb / total_gb * 100.0f + 0.5f) : 0;
            return std::to_wstring(percent) + GpuMemoryUnitText(unit, show_unit, separate_with_space);
        }

        //已占用显存格式，单位文本的空格约定见GpuMemoryUnitText（如“45.1G”“45.1 GB”）
        const bool use_mb = GpuMemoryUnitEquals(unit, L"MB");
        const float used_value = use_mb ? used_gb * 1024.0f : used_gb;
        return GpuMemoryFormatFloat(used_value, decimal_place) + GpuMemoryUnitText(unit, show_unit, separate_with_space);
    }

    String^ HardwareMonitorHelper::GetGpuMemoryDisplayName(String^ identifier)
    {
        String^ hardware_identifier = GetGpuMemoryHardwareIdentifier(identifier);
        IHardware^ hardware = FindHardwareByIdentifier(hardware_identifier);
        String^ hardware_name = hardware != nullptr ? hardware->Name : gcnew String(L"GPU");
        String^ item_name = MonitorGlobal::Instance()->GetString(L"GpuMemoryUsageItem");
        if (String::IsNullOrEmpty(item_name))
            item_name = gcnew String(L"VRAM Used/Total");
        return hardware_name + "/" + item_name;
    }

    String^ HardwareMonitorHelper::GetGpuMemoryLabel()
    {
        String^ label = MonitorGlobal::Instance()->GetString(L"GpuMemoryLabel");
        if (String::IsNullOrEmpty(label))
            label = gcnew String(L"VRAM");
        return label;
    }

    void HardwareMonitorHelper::GetDefaultMonitorItem(List<ISensor^>^ default_sensors)
    {
        auto computer = MonitorGlobal::Instance()->computer;
        for (int i = 0; i < computer->Hardware->Count; i++)
        {
            IHardware^ hardware = computer->Hardware[i];
            //CPU
            if (hardware->HardwareType == HardwareType::Cpu)
            {
                ISensor^ cpu_temp = nullptr;
                ISensor^ cpu_temp_default = nullptr;
                //先查找"Core Average"
                for (int j = 0; j < hardware->Sensors->Length; j++)
                {
                    ISensor^ sensor = hardware->Sensors[j];
                    if (sensor->SensorType == SensorType::Temperature)
                    {
                        if (cpu_temp_default == nullptr)
                            cpu_temp_default = sensor;
                        if (sensor->Name->Equals("Core Average"))
                        {
                            cpu_temp = sensor;
                            break;
                        }
                    }
                }
                //没找到则使用第一个
                if (cpu_temp == nullptr && cpu_temp_default != nullptr)
                    cpu_temp = cpu_temp_default;
                if (cpu_temp != nullptr)
                    default_sensors->Add(cpu_temp);
            }

            //GPU
            if (hardware->HardwareType == HardwareType::GpuNvidia || hardware->HardwareType == HardwareType::GpuAmd
                || hardware->HardwareType == HardwareType::GpuIntel)
            {
                ISensor^ gpu_temp = nullptr;
                ISensor^ gpu_temp_default = nullptr;
                for (int j = 0; j < hardware->Sensors->Length; j++)
                {
                    ISensor^ sensor = hardware->Sensors[j];
                    //GPU温度
                    if (sensor->SensorType == SensorType::Temperature)
                    {
                        if (gpu_temp_default == nullptr)
                            gpu_temp_default = sensor;
                        if (sensor->Name->Equals("GPU Core"))
                        {
                            gpu_temp = sensor;
                            break;
                        }
                    }
                    //GPU负载
                    else if (sensor->SensorType == SensorType::Load)
                    {
                        if (sensor->Name->Equals("GPU Core") || sensor->Name->Equals("D3D 3D"))
                            default_sensors->Add(sensor);
                    }
                }
                //没找到则使用第一个
                if (gpu_temp == nullptr && gpu_temp_default != nullptr)
                    gpu_temp = gpu_temp_default;
                if (gpu_temp != nullptr)
                    default_sensors->Add(gpu_temp);
            }
        }
    }
}
