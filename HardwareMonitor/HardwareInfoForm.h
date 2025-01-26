#pragma once

namespace HardwareMonitor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// HardwareInfoForm 摘要
	/// </summary>
	public ref class HardwareInfoForm : public System::Windows::Forms::Form
	{
	public:
        HardwareInfoForm(void);

        void UpdateData();

	protected:
		/// <summary>
		/// 清理所有正在使用的资源。
		/// </summary>
		~HardwareInfoForm()
		{
			if (components)
			{
				delete components;
			}
		}

        void InitContextMenu();

    private:
        void AddItem_Click(System::Object^ sender, System::EventArgs^ e);
        void ContextMenuStrip_Opening(Object^ sender, CancelEventArgs^ e);
        void TreeView_MouseClick(Object^ sender, MouseEventArgs^ e);

    private: System::Windows::Forms::TreeView^ treeView1;

    private: System::ComponentModel::IContainer^ components;
    protected:
        System::Windows::Forms::ContextMenuStrip^ contextMenuStrip;
        ToolStripMenuItem^ addItem;

	private:
		/// <summary>
		/// 必需的设计器变量。
		/// </summary>


#pragma region Windows Form Designer generated code
		/// <summary>
		/// 设计器支持所需的方法 - 不要修改
		/// 使用代码编辑器修改此方法的内容。
		/// </summary>
		void InitializeComponent(void)
		{
            this->treeView1 = (gcnew System::Windows::Forms::TreeView());
            this->SuspendLayout();
            // 
            // treeView1
            // 
            this->treeView1->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom)
                | System::Windows::Forms::AnchorStyles::Left)
                | System::Windows::Forms::AnchorStyles::Right));
            this->treeView1->Location = System::Drawing::Point(14, 16);
            this->treeView1->Margin = System::Windows::Forms::Padding(3, 4, 3, 4);
            this->treeView1->Name = L"treeView1";
            this->treeView1->Size = System::Drawing::Size(419, 463);
            this->treeView1->TabIndex = 0;
            // 
            // HardwareInfoForm
            // 
            this->AutoScaleDimensions = System::Drawing::SizeF(9, 20);
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
            this->ClientSize = System::Drawing::Size(447, 496);
            this->Controls->Add(this->treeView1);
            this->Font = (gcnew System::Drawing::Font(L"微软雅黑", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
                static_cast<System::Byte>(134)));
            this->Margin = System::Windows::Forms::Padding(3, 4, 3, 4);
            this->Name = L"HardwareInfoForm";
            this->Text = L"HardwareInfoForm";
            this->ResumeLayout(false);

        }
#pragma endregion
	};
}
