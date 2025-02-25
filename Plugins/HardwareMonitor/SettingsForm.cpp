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

        // 为ListBox的SelectedIndexChanged事件添加处理程序
        monitorItemListBox->SelectedIndexChanged += gcnew System::EventHandler(this, &SettingsForm::listBox_SelectedIndexChanged);
        decimalPlaceCombo->SelectedIndexChanged += gcnew EventHandler(this, &SettingsForm::OnDecimalPlaceComboBoxSelectedIndexChanged);
        monitorItemListBox->DrawItem += gcnew DrawItemEventHandler(this, &SettingsForm::ListBox_DrawItem);

        // 为FormClosing事件添加处理程序
        this->FormClosing += gcnew FormClosingEventHandler(this, &SettingsForm::OnFormClosing);
    }

    void SettingsForm::UpdateItemList()
    {
        //清除列表
        monitorItemListBox->Items->Clear();
        identifyerList->Clear();
        EnableControls();

        //填充数据
        for (const auto& item : CHardwareMonitor::GetInstance()->m_settings.items_info)
        {
            String^ item_name = gcnew String(CHardwareMonitor::GetInstance()->GetItemName(item.identifyer).c_str());
            if (item_name->Length > 0)
            {
                monitorItemListBox->Items->Add(item_name);
                identifyerList->Add(gcnew String(item.identifyer.c_str()));
                //根据监控项类型获取图标
                ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(item.identifyer.c_str()));
                if (sensor != nullptr)
                {
                    auto resName = HardwareMonitorHelper::GetHardwareIconResName(sensor->Hardware->HardwareType);
                    if (resName->Length > 0)
                        iconMap[item_name] = MonitorGlobal::Instance()->GetIcon(resName);
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
        if (index >= 0 && index < identifyerList->Count)
        {
            std::wstring identifyer = Common::StringToStdWstring(identifyerList[index]);
            return CHardwareMonitor::GetInstance()->m_settings.FindItemInfo(identifyer);
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
            int index = unitCombo->FindString(gcnew String(item_info.unit.c_str()));
            if (index >= 0)
                unitCombo->SelectedIndex = index;
            else
                unitCombo->SelectedIndex = 0;
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
        catch(System::Exception^ e)
        {}

        CHardwareMonitor::GetInstance()->m_settings.show_mouse_tooltip = showTooltipCheck->Checked;

        //保存设置
        CHardwareMonitor::GetInstance()->SaveConfig();

        Common::SaveFormSize(this, L"settings");
    }

    void SettingsForm::ListBox_DrawItem(Object^ sender, DrawItemEventArgs^ e)
    {
        // 获取节点的矩形区域
        Rectangle bounds = e->Bounds;
        // 获取当前项的文本
        String^ text = monitorItemListBox->Items[e->Index]->ToString();

        // 检查是否为选中项
        bool isSelected = (e->State & DrawItemState::Selected) == DrawItemState::Selected;

        // 设置颜色
        System::Drawing::Color textColor;
        System::Drawing::Color backColor;
        if (isSelected)
        {
            textColor = SystemColors::HighlightText;
            backColor = SystemColors::Highlight;
        }
        else
        {
            textColor = monitorItemListBox->ForeColor;
            backColor = SystemColors::Window;
        }
        // 绘制背景
        e->Graphics->FillRectangle(gcnew SolidBrush(backColor), e->Bounds);

        //绘制图标
        System::Drawing::Icon^ icon;
        if (iconMap->TryGetValue(text, icon))
        {
            if (icon != nullptr)
            {
                Point start_pos = bounds.Location;
                int offset = (bounds.Height - icon->Size.Height) / 2;
                start_pos.Offset(offset, offset);
                e->Graphics->DrawIcon(icon, start_pos.X, start_pos.Y);
                bounds.Offset(bounds.Height, 0);
                bounds.Width -= bounds.Height;
            }
        }

        // 绘制文本，垂直居中对齐
        TextRenderer::DrawText(e->Graphics, text, monitorItemListBox->Font, bounds, textColor, TextFormatFlags::VerticalCenter | TextFormatFlags::Left);
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

}
