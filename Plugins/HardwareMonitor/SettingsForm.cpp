#include "stdafx.h"
#include "SettingsForm.h"
#include "HardwareMonitor.h"
#include "HardwareMonitorHelper.h"
#include "Common.h"
using namespace System::Collections::Generic;

namespace HardwareMonitor
{
    SettingsForm::SettingsForm(void)
    {
        this->StartPosition = FormStartPosition::CenterParent;
        this->Icon = MonitorGlobal::Instance()->GetAppIcon();
        InitializeComponent();
        this->MinimumSize = this->Size;

        Common::LoadFormSize(this, L"settings");

        //添加图标
        Common::SetButtonIcon(addItemBtn, "Add");
        Common::SetButtonIcon(removeSelectBtn, "Delete");
        Common::SetButtonIcon(moveUpButton, "MoveUp");
        Common::SetButtonIcon(moveDownButton, "MoveDown");

        UpdateItemList();
        EnableControls();

        //初始化要监控的硬件选项
        Computer^ computer = MonitorGlobal::Instance()->computer;
        cpuCheck->Checked = computer->IsCpuEnabled;
        gpuCheck->Checked = computer->IsGpuEnabled;
        motherBoardCheck->Checked = computer->IsMotherboardEnabled;
        storageCheck->Checked = computer->IsStorageEnabled;
        batteryCheck->Checked = computer->IsBatteryEnabled;
        networkCheck->Checked = computer->IsNetworkEnabled;
        memoryCheck->Checked = computer->IsMemoryEnabled;
        controllerCheck->Checked = computer->IsControllerEnabled;
        psuCheck->Checked = computer->IsPsuEnabled;

        showTooltipCheck->Checked = CHardwareMonitor::GetInstance()->m_settings.show_mouse_tooltip;

        decimalPlaceCombo->DropDownStyle = ComboBoxStyle::DropDownList;
        decimalPlaceCombo->Items->Add("0");
        decimalPlaceCombo->Items->Add("1");
        decimalPlaceCombo->Items->Add("2");
        decimalPlaceCombo->Items->Add("3");

        monitorItemListBox->ItemHeight = CHardwareMonitor::GetInstance()->DPI(20);
        monitorItemListBox->DrawMode = DrawMode::OwnerDrawFixed; // 设置为自定义绘制模式
        monitorItemListBox->IntegralHeight = false; // 允许高度连续变化

        // 为ListBox的SelectedIndexChanged事件添加处理程序
        monitorItemListBox->SelectedIndexChanged += gcnew System::EventHandler(this, &SettingsForm::listBox_SelectedIndexChanged);
        decimalPlaceCombo->SelectedIndexChanged += gcnew EventHandler(this, &SettingsForm::OnDecimalPlaceComboBoxSelectedIndexChanged);
        monitorItemListBox->DrawItem += gcnew DrawItemEventHandler(this, &SettingsForm::ListBox_DrawItem);

        // 为FormClosing事件添加处理程序
        this->FormClosing += gcnew FormClosingEventHandler(this, &SettingsForm::OnFormClosing);
    
        SettingsForm_Resize(nullptr, nullptr);
    }

    void SettingsForm::UpdateItemsValue()
    {
        for each (const auto & obj in monitorItemListBox->Items)
        {
            ListItemInfo^ listItem = dynamic_cast<ListItemInfo^>(obj);
            if (listItem != nullptr && listItem->sensor != nullptr)
            {
                String^ identifyer = HardwareMonitorHelper::GetSensorIdentifyer(listItem->sensor);
                ItemInfo& itemInfo = CHardwareMonitor::GetInstance()->m_settings.FindItemInfo(Common::StringToStdWstring(identifyer));
                //获取displayValue
                listItem->displayValue = HardwareMonitorHelper::GetSensorValueText(listItem->sensor, gcnew String(itemInfo.unit.c_str()), itemInfo.decimal_places, itemInfo.show_unit);
            }
        }
        monitorItemListBox->Invalidate();
    }

    void SettingsForm::UpdateItemList()
    {
        //清除列表
        monitorItemListBox->Items->Clear();
        EnableControls();

        //填充数据
        for (const auto& item : CHardwareMonitor::GetInstance()->m_settings.items_info)
        {
            String^ item_name = gcnew String(CHardwareMonitor::GetInstance()->GetItemName(item.identifyer).c_str());
            if (item_name->Length > 0)
            {
                ListItemInfo^ listItem = gcnew ListItemInfo();
                listItem->displayName = item_name;
                ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(item.identifyer.c_str()));
                listItem->sensor = sensor;
                if (sensor != nullptr)
                {
                    //获取数值文本
                    listItem->displayValue = HardwareMonitorHelper::GetSensorValueText(sensor, gcnew String(item.unit.c_str()), item.decimal_places, item.show_unit);
                    //根据监控项类型获取图标
                    IHardware^ hardware = sensor->Hardware;
                    //如果是子硬件，则使用它的父硬件
                    if (hardware->Parent != nullptr)
                        hardware = hardware->Parent;
                    auto resName = HardwareMonitorHelper::GetHardwareIconResName(hardware->HardwareType);
                    if (resName->Length > 0)
                        listItem->icon = MonitorGlobal::Instance()->GetIcon(resName);
                    monitorItemListBox->Items->Add(listItem);
                }
            }
        }
    }

    bool SettingsForm::IsSelectionValid()
    {
        return (monitorItemListBox->SelectedIndex >= 0 && monitorItemListBox->SelectedIndex < monitorItemListBox->Items->Count);
    }

    ItemInfo& SettingsForm::GetSelectedItemInfo()
    {
        int index = monitorItemListBox->SelectedIndex;
        if (index >= 0 && index < monitorItemListBox->Items->Count)
        {
            ListItemInfo^ listItem = dynamic_cast<ListItemInfo^>(monitorItemListBox->Items[index]);
            if (listItem != nullptr && listItem->sensor != nullptr)
            {
                String^ identifyer = HardwareMonitorHelper::GetSensorIdentifyer(listItem->sensor);
                return CHardwareMonitor::GetInstance()->m_settings.FindItemInfo(Common::StringToStdWstring(identifyer));
            }
        }
        static ItemInfo emptyInfo;
        return emptyInfo;
    }

    void SettingsForm::EnableControls()
    {
        bool selection_enabled = IsSelectionValid();
        removeSelectBtn->Enabled = selection_enabled;
        decimalPlaceCombo->Enabled = selection_enabled;
        specifyValueWidthCheck->Enabled = selection_enabled;
        valueWidthEdit->Enabled = selection_enabled && specifyValueWidthCheck->Checked;
        showUnitCheck->Enabled = selection_enabled;
        unitCombo->Enabled = selection_enabled;
        int index = monitorItemListBox->SelectedIndex;
        moveUpButton->Enabled = selection_enabled && index > 0;
        moveDownButton->Enabled = selection_enabled && index < monitorItemListBox->Items->Count - 1;
    }

    void SettingsForm::listBox_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e)
    {
        // 根据是否有选中项启用或禁用按钮
        EnableControls();

        //设置选中项对应的设置控件
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            if (item_info.decimal_places >= 0 && item_info.decimal_places < decimalPlaceCombo->Items->Count)
            {
                decimalPlaceCombo->SelectedIndex = item_info.decimal_places;
            }
            else
            {
                decimalPlaceCombo->SelectedIndex = -1;
            }

            specifyValueWidthCheck->Checked = item_info.specify_value_width;
            valueWidthEdit->Value = item_info.value_width;

            showUnitCheck->Checked = item_info.show_unit;
            //填充“单位选择”下拉列表
            unitCombo->Items->Clear();
            ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(item_info.identifyer.c_str()));
            if (sensor != nullptr)
            {
                List<String^>^ unitList = HardwareMonitorHelper::GetSensorTypeUnit(sensor->SensorType);
                for each (String ^ unit in unitList)
                {
                    unitCombo->Items->Add(unit);
                }
            }
            if (unitCombo->Items->Count > 0)
            {
                int index = unitCombo->FindString(gcnew String(item_info.unit.c_str()));
                if (index >= 0)
                    unitCombo->SelectedIndex = index;
                else
                    unitCombo->SelectedIndex = 0;
            }
        }
    }

    void SettingsForm::OnDecimalPlaceComboBoxSelectedIndexChanged(Object^ sender, EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            item_info.decimal_places = decimalPlaceCombo->SelectedIndex;
        }
    }

    void SettingsForm::OnFormClosing(Object^ sender, FormClosingEventArgs^ e)
    {
        //更新要监控的硬件设置
        Computer^ computer = MonitorGlobal::Instance()->computer;
        try
        {
            computer->IsCpuEnabled = cpuCheck->Checked;
            computer->IsGpuEnabled = gpuCheck->Checked;
            computer->IsMotherboardEnabled = motherBoardCheck->Checked;
            computer->IsStorageEnabled = storageCheck->Checked;
            computer->IsBatteryEnabled = batteryCheck->Checked;
            computer->IsNetworkEnabled = networkCheck->Checked;
            computer->IsMemoryEnabled = memoryCheck->Checked;
            computer->IsControllerEnabled = controllerCheck->Checked;
            computer->IsPsuEnabled = psuCheck->Checked;
        }
        catch(System::Exception^)
        {}

        CHardwareMonitor::GetInstance()->m_settings.show_mouse_tooltip = showTooltipCheck->Checked;

        //保存设置
        CHardwareMonitor::GetInstance()->SaveConfig();
        Common::SaveFormSize(this, L"settings");

        //更改了监控的硬件设置后，传感器可能会发生变化，因此这里清空传感器缓存
        MonitorGlobal::Instance()->sensorMap->Clear();
    }

    void SettingsForm::ListBox_DrawItem(Object^ sender, DrawItemEventArgs^ e)
    {
        //确保index有效
        if (e->Index < 0 || e->Index >= monitorItemListBox->Items->Count)
            return;

        // 获取节点的矩形区域
        Rectangle bounds = e->Bounds;
        // 获取当前项的文本
        ListItemInfo^ listItem = dynamic_cast<ListItemInfo^>(monitorItemListBox->Items[e->Index]);
        if (listItem == nullptr)
            return;
        String^ text = listItem->displayName;

        // 检查是否为选中项
        bool isSelected = (e->State & DrawItemState::Selected) == DrawItemState::Selected;

        // 设置颜色
        System::Drawing::Color textColor;
        System::Drawing::Color valueColor;
        System::Drawing::Color backColor;
        if (isSelected)
        {
            textColor = SystemColors::HighlightText;
            valueColor = SystemColors::HighlightText;
            backColor = SystemColors::Highlight;
        }
        else
        {
            textColor = monitorItemListBox->ForeColor;
            valueColor = SystemColors::Highlight;
            backColor = SystemColors::Window;
        }
        // 绘制背景
        e->Graphics->FillRectangle(gcnew SolidBrush(backColor), e->Bounds);

        //绘制图标
        System::Drawing::Icon^ icon = listItem->icon;
        if (icon != nullptr)
        {
            Point start_pos = bounds.Location;
            int offset = (bounds.Height - icon->Size.Height) / 2;
            start_pos.Offset(offset, offset);
            e->Graphics->DrawIcon(icon, start_pos.X, start_pos.Y);
            bounds.Offset(bounds.Height, 0);
            bounds.Width -= bounds.Height;
        }

        //计算右侧数值部分的宽度
        SizeF rightTextSize = e->Graphics->MeasureString(listItem->displayValue, monitorItemListBox->Font);
        int rightWidth = std::min(bounds.Width, (int)rightTextSize.Width + CHardwareMonitor::GetInstance()->DPI(4));
        Rectangle rightRect = Rectangle(bounds.Right - rightWidth, bounds.Top, rightWidth, bounds.Height);
        Rectangle leftRect = Rectangle(bounds.Left, bounds.Top, bounds.Width - rightWidth, bounds.Height);

        // 绘制数值部分
        TextRenderer::DrawText(e->Graphics, listItem->displayValue, monitorItemListBox->Font, rightRect, valueColor, TextFormatFlags::Right | TextFormatFlags::VerticalCenter);

        // 绘制文本
        TextRenderer::DrawText(e->Graphics, text, monitorItemListBox->Font, leftRect, textColor, TextFormatFlags::VerticalCenter | TextFormatFlags::Left | TextFormatFlags::WordEllipsis);
    }

    System::Void SettingsForm::removeSelectBtn_Click(System::Object^ sender, System::EventArgs^ e)
    {
        int selectedIndex = monitorItemListBox->SelectedIndex;
        // 删除ListBox中的选中项
        if (selectedIndex >= 0)
        {
            // 弹出MessageBox询问用户是否要删除
            System::Windows::Forms::DialogResult result = System::Windows::Forms::MessageBox::Show(
                MonitorGlobal::Instance()->GetString(L"RemoveMonitorItemInquery"),
                MonitorGlobal::Instance()->GetString(L"PluginName"),
                System::Windows::Forms::MessageBoxButtons::OKCancel, 
                System::Windows::Forms::MessageBoxIcon::Question);
            // 如果用户点击“确定”，则删除选中项
            if (result == System::Windows::Forms::DialogResult::OK)
            {
                monitorItemListBox->Items->RemoveAt(selectedIndex);
                CHardwareMonitor::GetInstance()->RemoveDisplayItem(selectedIndex);
            }
        }
    }

    System::Void SettingsForm::addItemBtn_Click(System::Object^ sender, System::EventArgs^ e)
    {
        MonitorGlobal::Instance()->ShowHardwareInfoDialog();
        UpdateItemList();
    }

    System::Void SettingsForm::specifyValueWidthCheck_CheckedChanged(System::Object^ sender, System::EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            item_info.specify_value_width = specifyValueWidthCheck->Checked;
        }
        EnableControls();
    }

    System::Void SettingsForm::valueWidthEdit_ValueChanged(System::Object^ sender, System::EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            item_info.value_width = static_cast<int>(valueWidthEdit->Value);
        }
    }

    System::Void SettingsForm::unitCombo_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            int index = unitCombo->SelectedIndex;
            if (index >= 0 && index < unitCombo->Items->Count)
            {
                String^ unit = unitCombo->Items[index]->ToString();
                item_info.unit = Common::StringToStdWstring(unit);
            }
        }
    }

    System::Void SettingsForm::moveUpButton_Click(System::Object^ sender, System::EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            int index = monitorItemListBox->SelectedIndex;
            auto& items_info{ CHardwareMonitor::GetInstance()->m_settings.items_info };
            if (index > 0 && index < static_cast<int>(items_info.size()))
            {
                //交换列表中当前项和前一项的文本
                Common::SwapListBoxItems(monitorItemListBox, index, index - 1);
                //交换ItemInfo
                std::swap(items_info[index], items_info[index - 1]);
                //更改选中项
                monitorItemListBox->SelectedIndex--;
            }
        }
    }

    System::Void SettingsForm::moveDownButton_Click(System::Object^ sender, System::EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            int index = monitorItemListBox->SelectedIndex;
            auto& items_info{ CHardwareMonitor::GetInstance()->m_settings.items_info };
            if (index >= 0 && index < static_cast<int>(items_info.size()) - 1)
            {
                //交换列表中当前项和后一项的文本
                Common::SwapListBoxItems(monitorItemListBox, index, index + 1);
                //交换ItemInfo
                std::swap(items_info[index], items_info[index + 1]);
                //更改选中项
                monitorItemListBox->SelectedIndex++;
            }
        }
    }

    System::Void SettingsForm::SettingsForm_Resize(System::Object^ sender, System::EventArgs^ e)
    {
        //这里让monitorItemListBox的下边缘总是和groupBox2的下边缘对齐，以修正在高DPI下ListBox下边缘超出窗口边界的问题
        int listBoxHeight = groupBox2->Bottom - monitorItemListBox->Top;
        monitorItemListBox->Height = listBoxHeight;

        monitorItemListBox->Invalidate();
    }

    System::Void SettingsForm::showUnitCheck_CheckedChanged(System::Object^ sender, System::EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            item_info.show_unit = showUnitCheck->Checked;
        }
    }
}
