#include "stdafx.h"
#include "HardwareInfoForm.h"
#include "HardwareMonitor.h"
#include <cliext/map>
#include "HardwareMonitorHelper.h"

namespace HardwareMonitor
{

    HardwareInfoForm::HardwareInfoForm(void)
    {
        this->StartPosition = FormStartPosition::CenterParent;
        this->Icon = MonitorGlobal::Instance()->GetAppIcon();

        InitializeComponent();
        InitUserComponent();

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
                        if (sensor_str != sensor_node->Text)
                            sensor_node->Text = sensor_str;
                    }
                }
            }
        }
        treeView1->EndUpdate();
    }

    void HardwareInfoForm::InitUserComponent()
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

        treeView1->DrawMode = TreeViewDrawMode::OwnerDrawText; // 设置为自定义绘制模式
        // 添加 DrawNode 事件处理程序
        treeView1->DrawNode += gcnew DrawTreeNodeEventHandler(this, &HardwareInfoForm::TreeView_DrawNode);

        autoRefreshCheck->Checked = CHardwareMonitor::GetInstance()->m_settings.hardware_info_auto_refresh;
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
        // 获取点击位置的节点
        TreeNode^ clickedNode = treeView1->GetNodeAt(e->X, e->Y);
        if (clickedNode != nullptr)
        {
            // 选中点击的节点
            treeView1->SelectedNode = clickedNode;
        }
    }
    void HardwareInfoForm::TreeView_DrawNode(Object^ sender, DrawTreeNodeEventArgs^ e)
    {
        // 获取节点的矩形区域
        Rectangle bounds = e->Bounds;
        // 扩展 bounds 的宽度到整个控件的宽度
        bounds.Width = treeView1->ClientSize.Width - bounds.Left - 1; // 减去1以避免绘制超出控件边界
        System::Drawing::Color textColor;
        // 如果节点被选中，绘制选中背景
        if (e->Node == treeView1->SelectedNode)
        {
            // 绘制选中背景
            e->Graphics->FillRectangle(gcnew SolidBrush(SystemColors::Highlight), bounds);
            textColor = SystemColors::HighlightText;
        }
        else
        {
            e->Graphics->FillRectangle(gcnew SolidBrush(SystemColors::Window), bounds);
            textColor = treeView1->ForeColor;
        }

        // 获取节点的文本
        String^ text = e->Node->Text;

        // 拆分文本
        array<String^>^ parts = System::Text::RegularExpressions::Regex::Split(text, "\\s{4}");
        if (parts->Length < 2)
            parts = gcnew array<String^>{text, ""};

        // 计算两个矩形区域
        SizeF rightTextSize = e->Graphics->MeasureString(parts[1], treeView1->Font);
        int rightWidth = (int)rightTextSize.Width;
        Rectangle rightRect = Rectangle(bounds.Right - rightWidth, bounds.Top, rightWidth, bounds.Height);
        Rectangle leftRect = Rectangle(bounds.Left, bounds.Top, bounds.Width - rightWidth, bounds.Height);

        // 绘制第二部分文本（右对齐）
        String^ rightText = parts[1];
        TextRenderer::DrawText(e->Graphics, rightText, treeView1->Font, rightRect, textColor, TextFormatFlags::Right);

        // 绘制第一部分文本（左对齐）
        String^ leftText = parts[0];
        TextRenderer::DrawText(e->Graphics, leftText, treeView1->Font, leftRect, textColor, TextFormatFlags::Left | TextFormatFlags::WordEllipsis);
    }

    System::Void HardwareInfoForm::autoRefreshCheck_CheckedChanged(System::Object^ sender, System::EventArgs^ e)
    {
        CHardwareMonitor::GetInstance()->m_settings.hardware_info_auto_refresh = autoRefreshCheck->Checked;
    }
}
