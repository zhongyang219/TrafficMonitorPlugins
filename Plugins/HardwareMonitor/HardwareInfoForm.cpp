#include "stdafx.h"
#include "HardwareInfoForm.h"
#include "HardwareMonitor.h"
#include "HardwareMonitorHelper.h"
#include "Common.h"

using namespace System::Collections::Generic;

namespace HardwareMonitor
{
    static TreeNode^ AddHardwareNode(TreeNodeCollection^ nodes, IHardware^ hardware)
    {
        auto hardware_node = nodes->Add(Common::GetTranslatedString(hardware->Name));
        //保存图标资源名称
        hardware_node->ImageKey = HardwareMonitorHelper::GetHardwareIconResName(hardware->HardwareType);
        //添加Sensor节点
        Dictionary<SensorType, TreeNode^>^ sensor_type_nodes = gcnew Dictionary<SensorType, TreeNode^>();       //保存所有Sensor类型的节点
        for (int j = 0; j < hardware->Sensors->Length; j++)
        {
            auto sensor = hardware->Sensors[j];
            //根据Sensor的类型创建父节点
            TreeNode^ type_node;
            if (!sensor_type_nodes->TryGetValue(sensor->SensorType, type_node))
            {
                //创建类型节点，并保存到map中
                type_node = hardware_node->Nodes->Add(HardwareMonitorHelper::GetSensorTypeName(sensor->SensorType));
                sensor_type_nodes->Add(sensor->SensorType, type_node);
            }

            String^ sensor_str = Common::GetTranslatedString(sensor->Name);
            auto sensor_node = type_node->Nodes->Add(sensor_str);
            //保存传感器节点的信息到Tag
            HardwareInfoForm::SensorNodeInfo^ nodeInfo = gcnew HardwareInfoForm::SensorNodeInfo();
            nodeInfo->displayName = sensor_str;
            String^ defaultUnit = HardwareMonitorHelper::GetSensorTypeDefaultUnit(sensor->SensorType);
            nodeInfo->displayValue = HardwareMonitorHelper::GetSensorValueText(sensor, defaultUnit);
            nodeInfo->sensor = sensor;
            sensor_node->Tag = nodeInfo;
        }
        return hardware_node;
    }

    HardwareInfoForm::HardwareInfoForm(void)
    {
        this->StartPosition = FormStartPosition::CenterParent;
        this->Icon = MonitorGlobal::Instance()->GetAppIcon();

        InitializeComponent();
        InitUserComponent();

        Common::LoadFormSize(this, L"hardware_info");

        //填充数据
        auto computer = MonitorGlobal::Instance()->computer;
        for (int i = 0; i < computer->Hardware->Count; i++)
        {
            //添加Hardware节点
            auto hardware = computer->Hardware[i];
            auto hardware_node = AddHardwareNode(treeView1->Nodes, hardware);
            //遍历SubHardware
            for (int j = 0; j < hardware->SubHardware->Length; j++)
            {
                auto sub_hardware = hardware->SubHardware[j];
                AddHardwareNode(hardware_node->Nodes, sub_hardware);
            }
        }

        if (MonitorGlobal::Instance()->treeCollapseNodes->Count == 0)
            //展开所有节点
            treeView1->ExpandAll();
        else
            //恢复节点的展开状态
            Common::RestoreTreeNodeExpandStatus(treeView1, MonitorGlobal::Instance()->treeCollapseNodes);
    }

    //更新一个树节点的值
    static void UpdateNodeValue(TreeNode^ node)
    {
        //有子节点则递归调用
        if (node->Nodes->Count != 0)
        {
            for each (TreeNode ^ sub_node in node->Nodes)
            {
                UpdateNodeValue(sub_node);
            }
        }
        //叶子节点，更新值
        else
        {
            HardwareInfoForm::SensorNodeInfo^ sensorInfo = dynamic_cast<HardwareInfoForm::SensorNodeInfo^>(node->Tag);
            if (sensorInfo != nullptr && sensorInfo->sensor != nullptr)
            {
                sensorInfo->displayName = Common::GetTranslatedString(sensorInfo->sensor->Name);
                String^ defaultUnit = HardwareMonitorHelper::GetSensorTypeDefaultUnit(sensorInfo->sensor->SensorType);
                sensorInfo->displayValue = HardwareMonitorHelper::GetSensorValueText(sensorInfo->sensor, defaultUnit);
            }
        }
    }

    void HardwareMonitor::HardwareInfoForm::UpdateData()
    {
        //更新树节点的数据
        //遍历Hardware节点
        for each (TreeNode ^ hardware_node in treeView1->Nodes)
        {
            UpdateNodeValue(hardware_node);
        }
        treeView1->Invalidate();
    }

    void HardwareInfoForm::InitUserComponent()
    {
        //使用ImageList设置树控件的行高（实际不添加图标，图标由TreeView_DrawNode函数自绘）
        ImageList^ imageList = gcnew ImageList();
        int iconWidth = CHardwareMonitor::GetInstance()->DPI(16);
        int iconHeight = CHardwareMonitor::GetInstance()->DPI(19);
        imageList->ImageSize = System::Drawing::Size(iconWidth, iconHeight);
        treeView1->ImageList = imageList;

        // 初始化 ContextMenuStrip
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

        // 添加 Resize 事件处理程序
        treeView1->Resize += gcnew EventHandler(this, &HardwareInfoForm::TreeView_Resize);

        autoRefreshCheck->Checked = CHardwareMonitor::GetInstance()->m_settings.hardware_info_auto_refresh;

        // 为FormClosing事件添加处理程序
        this->FormClosing += gcnew FormClosingEventHandler(this, &HardwareInfoForm::OnFormClosing);
    }

    void HardwareInfoForm::AddItem_Click(System::Object^ sender, System::EventArgs^ e)
    {
        // 获取选中的节点
        TreeNode^ selectedNode = treeView1->SelectedNode;
        if (selectedNode != nullptr)
        {
            SensorNodeInfo^ sensorInfo = dynamic_cast<SensorNodeInfo^>(selectedNode->Tag);
            if (sensorInfo != nullptr && sensorInfo->sensor != nullptr)
            {
                if (CHardwareMonitor::GetInstance()->AddDisplayItem(sensorInfo->sensor))
                    CHardwareMonitor::GetInstance()->SaveConfig();
                else
                    MessageBox::Show(MonitorGlobal::Instance()->GetString(L"AddItemFailedMsg"),
                        MonitorGlobal::Instance()->GetString(L"PluginName"),
                        MessageBoxButtons::OK, MessageBoxIcon::Information);
            }
        }
    }

    void HardwareInfoForm::ContextMenuStrip_Opening(Object^ sender, CancelEventArgs^ e)
    {
        TreeNode^ selectedNode = treeView1->SelectedNode;
        // 检查是否为叶子节点（仅第叶子允许添加监控项目）
        addItem->Enabled = (selectedNode != nullptr && selectedNode->Nodes->Count == 0);
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
        SensorNodeInfo^ sensorInfo = dynamic_cast<SensorNodeInfo^>(e->Node->Tag);

        // 获取节点的矩形区域
        Rectangle bounds = e->Bounds;
        // 由于通过ImageList设置了图标，但是并不通过ImageList显示图标，因此矩形区域向左扩展图标大小的距离
        if (treeView1->ImageList != nullptr)
        {
            int imageWidth = treeView1->ImageList->ImageSize.Width;
            bounds.Offset(-imageWidth - CHardwareMonitor::GetInstance()->DPI(2), 0);
        }

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
            //判断当前项是否已添加到监控
            String^ identifyer{};
            if (sensorInfo != nullptr)
                identifyer = HardwareMonitorHelper::GetSensorIdentifyer(sensorInfo->sensor);
            //已添加到监控项目的文本颜色显示为高亮颜色
            if (identifyer != nullptr && CHardwareMonitor::GetInstance()->IsDisplayItemExist(Common::StringToStdWstring(identifyer)))
                textColor = SystemColors::Highlight;
            else
                textColor = treeView1->ForeColor;
        }

        // 为根节点绘制图标
        if (e->Node->Parent == nullptr && e->Node->ImageKey->Length > 0)
        {
            System::Drawing::Icon^ icon = MonitorGlobal::Instance()->GetIcon(e->Node->ImageKey);
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

        // 获取节点的文本
        String^ text;
        if (sensorInfo != nullptr)
            text = sensorInfo->displayName;
        else
            text = e->Node->Text;

        String^ rightText;
        if (sensorInfo != nullptr)
            rightText = sensorInfo->displayValue;

        // 计算两个矩形区域
        SizeF rightTextSize = e->Graphics->MeasureString(rightText, treeView1->Font);
        int rightWidth = std::min(bounds.Width, (int)rightTextSize.Width + CHardwareMonitor::GetInstance()->DPI(4));
        Rectangle rightRect = Rectangle(bounds.Right - rightWidth, bounds.Top, rightWidth, bounds.Height);
        Rectangle leftRect = Rectangle(bounds.Left, bounds.Top, bounds.Width - rightWidth, bounds.Height);

        // 绘制第二部分文本（右对齐）
        if (rightText != nullptr && rightText->Length > 0)
            TextRenderer::DrawText(e->Graphics, rightText, treeView1->Font, rightRect, textColor, TextFormatFlags::Right | TextFormatFlags::VerticalCenter);

        // 绘制第一部分文本（左对齐）
        TextRenderer::DrawText(e->Graphics, text, treeView1->Font, leftRect, textColor, TextFormatFlags::Left | TextFormatFlags::VerticalCenter | TextFormatFlags::WordEllipsis);
    }

    void HardwareInfoForm::TreeView_Resize(Object^ sender, EventArgs^ e)
    {
        // 强制重绘
        treeView1->Invalidate();
    }

    void HardwareInfoForm::OnFormClosing(Object^ sender, FormClosingEventArgs^ e)
    {
        Common::SaveFormSize(this, L"hardware_info");
        Common::SaveTreeNodeExpandStatus(treeView1, MonitorGlobal::Instance()->treeCollapseNodes);
        CHardwareMonitor::GetInstance()->SaveConfig();
    }

    System::Void HardwareInfoForm::autoRefreshCheck_CheckedChanged(System::Object^ sender, System::EventArgs^ e)
    {
        CHardwareMonitor::GetInstance()->m_settings.hardware_info_auto_refresh = autoRefreshCheck->Checked;
    }
}
