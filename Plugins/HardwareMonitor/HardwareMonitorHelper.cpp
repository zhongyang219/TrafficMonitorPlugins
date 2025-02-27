#include "stdafx.h"
#include "HardwareMonitorHelper.h"
#include "HardwareMonitor.h"
#include <gcroot.h>
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

    String^ HardwareMonitorHelper::GetSensorValueText(ISensor^ sensor, String^ unit, int decimal_place, bool show_unit)
    {
        String^ sensor_str;
        if (sensor->Value.HasValue)
        {
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
            String^ formatString = String::Format("F{0}", decimal_place);
            sensor_str += value.ToString(formatString);
        }
        else
        {
            sensor_str += "--";
        }
        if (show_unit)
        {
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
