#include "stdafx.h"
#include "HardwareInfoForm.h"
#include "HardwareMonitor.h"
#include <cliext/map>
#include "HardwareMonitorHelper.h"

namespace HardwareMonitor
{
    static TreeNode^ AddHardwareNode(TreeNodeCollection^ nodes, IHardware^ hardware)
    {
        auto hardware_node = nodes->Add(hardware->Name);
        switch (hardware->HardwareType)
        {
        case HardwareType::Motherboard: hardware_node->ImageIndex = 0; break;
        case HardwareType::Battery: hardware_node->ImageIndex = 1; break;
        case HardwareType::Cpu: hardware_node->ImageIndex = 2; break;
        case HardwareType::EmbeddedController: hardware_node->ImageIndex = 3; break;
        case HardwareType::Network: hardware_node->ImageIndex = 4; break;
        case HardwareType::Memory: hardware_node->ImageIndex = 5; break;
        case HardwareType::Storage: hardware_node->ImageIndex = 6; break;
        case HardwareType::GpuAmd: hardware_node->ImageIndex = 7; break;
        case HardwareType::GpuIntel: hardware_node->ImageIndex = 8; break;
        case HardwareType::GpuNvidia: hardware_node->ImageIndex = 9; break;
        }
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
            sensor_node->Tag = HardwareMonitorHelper::GetSensorIdentifyer(sensor);
        }
        return hardware_node;
    }

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
            auto hardware_node = AddHardwareNode(treeView1->Nodes, hardware);
            //遍历SubHardware
            for (int j = 0; j < hardware->SubHardware->Length; j++)
            {
                auto sub_hardware = hardware->SubHardware[j];
                AddHardwareNode(hardware_node->Nodes, sub_hardware);
            }
        }

        //展开所有节点
        treeView1->ExpandAll();
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
            if (node->Tag != nullptr)
            {
                String^ identifyer = node->Tag->ToString();
                ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(identifyer);
                if (sensor != nullptr)
                {
                    String^ sensor_str = HardwareMonitorHelper::GetSensorNameValueText(sensor);
                    if (sensor_str != node->Text)
                        node->Text = sensor_str;
                }
            }
        }
    }

    void HardwareMonitor::HardwareInfoForm::UpdateData()
    {
        treeView1->BeginUpdate();
        //更新树节点的数据
        //遍历Hardware节点
        for each (TreeNode ^ hardware_node in treeView1->Nodes)
        {
            UpdateNodeValue(hardware_node);
        }
        treeView1->EndUpdate();
    }

    void HardwareInfoForm::InitUserComponent()
    {
        // 初始化 ImageList
        imageList1 = gcnew ImageList();
        int icon_size = CHardwareMonitor::GetInstance()->DPI(16);
        imageList1->ImageSize = System::Drawing::Size(icon_size, icon_size); // 设置图标大小

        // 添加图标到 ImageList
        Resources::ResourceManager^ resourceManager = MonitorGlobal::Instance()->GetResourceManager();
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("MotherBoard")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("batteries")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("CPU")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("FanColtroller")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("Network")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("RAM")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("Storage")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("AMD")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("intel")));
        imageList1->Images->Add(static_cast<Image^>(resourceManager->GetObject("Nvidia")));

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
            //判断当前项是否已添加到监控
            String^ identifyer{};
            if (e->Node->Tag != nullptr)
                identifyer = e->Node->Tag->ToString();
            //已添加到监控项目的文本颜色显示为高亮颜色
            if (identifyer != nullptr && CHardwareMonitor::GetInstance()->IsDisplayItemExist(MonitorGlobal::ClrStringToStdWstring(identifyer)))
                textColor = SystemColors::Highlight;
            else
                textColor = treeView1->ForeColor;
        }

        // 为根节点绘制图标
        if (e->Node->ImageIndex != -1 && e->Node->Parent == nullptr)
        {
            Point start_pos = bounds.Location;
            int offset = (bounds.Height - imageList1->ImageSize.Width) / 2;
            start_pos.Offset(offset, offset);
            imageList1->Draw(e->Graphics, start_pos, e->Node->ImageIndex);
            bounds.Offset(bounds.Height, 0);
            bounds.Width -= bounds.Height;
        }

        // 获取节点的文本
        String^ text = e->Node->Text;

        // 拆分文本
        array<String^>^ parts = System::Text::RegularExpressions::Regex::Split(text, "\\s{4}");
        if (parts->Length < 2)
            parts = gcnew array<String^>{text, ""};

        // 计算两个矩形区域
        SizeF rightTextSize = e->Graphics->MeasureString(parts[1], treeView1->Font);
        int rightWidth = std::min(bounds.Width, (int)rightTextSize.Width + 4);
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
