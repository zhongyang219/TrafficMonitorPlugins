#include "stdafx.h"
#include "HardwareInfoForm.h"
#include "HardwareMonitor.h"
#include <cliext/map>
#include "HardwareMonitorHelper.h"

namespace HardwareMonitor
{

    HardwareInfoForm::HardwareInfoForm(void)
    {
        InitializeComponent();
        InitContextMenu();

        //填充数据
        auto computer = MonitorGlobal::Instance()->computer;
        for (int i = 0; i < computer->Hardware->Count; i++)
        {
            //添加Hardware节点
            auto hardware = computer->Hardware[i];
            auto hardware_node = treeView1->Nodes->Add(hardware->Name);
            //添加Sensor节点
            typedef cliext::map<SensorType, TreeNode^> TypeNodeMap;
            TypeNodeMap sensor_type_nodes;       //保存所有Sensor类型的节点
            for (int j = 0; j < hardware->Sensors->Length; j++)
            {
                auto sensor = hardware->Sensors[j];
                //根据Sensor的类型创建父节点
                TreeNode^ type_node;
                TypeNodeMap::iterator iter = sensor_type_nodes.find(sensor->SensorType);
                if (iter == sensor_type_nodes.end())
                {
                    //创建类型节点，并保存到map中
                    type_node = hardware_node->Nodes->Add(gcnew String(HardwareMonitorHelper::GetSensorTypeName(sensor->SensorType)));
                    sensor_type_nodes.insert(TypeNodeMap::make_value(sensor->SensorType, type_node));
                }
                else
                {
                    //已存在的节点
                    type_node = iter->second;
                }

                String^ sensor_str = HardwareMonitorHelper::GetSensorNameValueText(sensor);
                auto sensor_node = type_node->Nodes->Add(sensor_str);
                sensor_node->Tag = sensor->Identifier->ToString();
            }
        }

        //展开所有节点
        treeView1->ExpandAll();
    }

    void HardwareMonitor::HardwareInfoForm::UpdateData()
    {
        treeView1->BeginUpdate();
        //更新树节点的数据
        //遍历Hardware节点
        for each (TreeNode ^ hardware_node in treeView1->Nodes)
        {
            //遍历SensorType节点
            for each (TreeNode ^ sensor_type_node in hardware_node->Nodes)
            {
                //遍历Sensor节点
                for each (TreeNode ^ sensor_node in sensor_type_node->Nodes)
                {
                    String^ identifyer = sensor_node->Tag->ToString();
                    ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(identifyer);
                    if (sensor != nullptr)
                    {
                        String^ sensor_str = HardwareMonitorHelper::GetSensorNameValueText(sensor);
                        sensor_node->Text = sensor_str;
                    }
                }
            }
        }
        treeView1->EndUpdate();
    }

    void HardwareInfoForm::InitContextMenu()
    {
        contextMenuStrip = gcnew System::Windows::Forms::ContextMenuStrip();

        // 添加菜单项
        addItem = gcnew ToolStripMenuItem();
        addItem->Text = MonitorGlobal::Instance()->GetString(L"AddToMonitorItem");
        addItem->Click += gcnew EventHandler(this, &HardwareInfoForm::AddItem_Click);
        contextMenuStrip->Opening += gcnew CancelEventHandler(this, &HardwareInfoForm::ContextMenuStrip_Opening);

        // 将菜单项添加到 ContextMenuStrip
        contextMenuStrip->Items->Add(addItem);

        // 将 ContextMenuStrip 绑定到 TreeView
        treeView1->ContextMenuStrip = contextMenuStrip;

        // 添加 MouseClick 事件处理程序
        treeView1->MouseClick += gcnew MouseEventHandler(this, &HardwareInfoForm::TreeView_MouseClick);
    }

    void HardwareInfoForm::AddItem_Click(System::Object^ sender, System::EventArgs^ e)
    {
        // 获取选中的节点
        TreeNode^ selectedNode = treeView1->SelectedNode;
        if (selectedNode != nullptr && selectedNode->Tag != nullptr)
        {
            String^ identifyer = selectedNode->Tag->ToString();
            ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(identifyer);
            if (CHardwareMonitor::GetInstance()->AddDisplayItem(sensor))
                CHardwareMonitor::GetInstance()->SaveConfig();
            else
                MessageBox::Show(MonitorGlobal::Instance()->GetString(L"AddItemFailedMsg"));
        }
    }

    void HardwareInfoForm::ContextMenuStrip_Opening(Object^ sender, CancelEventArgs^ e)
    {
        TreeNode^ selectedNode = treeView1->SelectedNode;
        // 检查是否为第 3 级节点（仅第3级节点允许添加监控项目）
        if (selectedNode != nullptr && selectedNode->Parent != nullptr && selectedNode->Parent->Parent != nullptr)
            addItem->Enabled = true;
        else
            addItem->Enabled = false;
    }

    void HardwareInfoForm::TreeView_MouseClick(Object^ sender, MouseEventArgs^ e)
    {
        if (e->Button == System::Windows::Forms::MouseButtons::Right)
        {
            // 获取点击位置的节点
            TreeNode^ clickedNode = treeView1->GetNodeAt(e->X, e->Y);
            if (clickedNode != nullptr)
            {
                // 选中点击的节点
                treeView1->SelectedNode = clickedNode;
            }
        }
    }
}
