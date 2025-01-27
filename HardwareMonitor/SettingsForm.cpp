#include "stdafx.h"
#include "SettingsForm.h"
#include "HardwareMonitor.h"
#include "HardwareMonitorHelper.h"

namespace HardwareMonitor
{
    SettingsForm::SettingsForm(void)
    {
        this->StartPosition = FormStartPosition::CenterParent;
        InitializeComponent();
        UpdateItemList();

        //“移除”按钮初始为禁用状态
        removeSelectBtn->Enabled = false;

        // 为ListBox的SelectedIndexChanged事件添加处理程序
        monitorItemListBox->SelectedIndexChanged += gcnew System::EventHandler(this, &SettingsForm::listBox_SelectedIndexChanged);
    }

    void SettingsForm::UpdateItemList()
    {
        //清除列表
        monitorItemListBox->Items->Clear();

        //填充数据
        for (const auto& identifyer : CHardwareMonitor::GetInstance()->m_settings.item_identifyers)
        {
            String^ item_name = gcnew String(CHardwareMonitor::GetInstance()->GetItemName(identifyer).c_str());
            if (item_name->Length > 0)
                monitorItemListBox->Items->Add(item_name);
        }
    }

    void SettingsForm::listBox_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e)
    {
        // 根据是否有选中项启用或禁用按钮
        removeSelectBtn->Enabled = monitorItemListBox->SelectedIndex >= 0;
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

}
