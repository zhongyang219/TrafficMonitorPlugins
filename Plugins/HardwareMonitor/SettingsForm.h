#pragma once

namespace HardwareMonitor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace System::Collections::Generic;

    struct ItemInfo;

	/// <summary>
	/// SettingsForm 摘要
	/// </summary>
	public ref class SettingsForm : public System::Windows::Forms::Form
	{
	public:
        SettingsForm(void);
        void UpdateItemsValue();

	protected:
        void UpdateItemList();

		/// <summary>
		/// 清理所有正在使用的资源。
		/// </summary>
		~SettingsForm()
		{
			if (components)
			{
				delete components;
			}
		}

    private:
        //用于保存列表中的每一项
        ref class ListItemInfo : public Object
        {
        public:
            String^ displayName;        //显示的名称
            String^ displayValue;       //显示的数值
            LibreHardwareMonitor::Hardware::ISensor^ sensor;    //传感器对象
            System::Drawing::Icon^ icon;    //列表项的图标
        };

    private: System::Windows::Forms::ListBox^ monitorItemListBox;
    private: System::Windows::Forms::Button^ removeSelectBtn;
    private: System::Windows::Forms::Label^ label1;
    private: System::Windows::Forms::Button^ cancelBtn;
    private: System::Windows::Forms::GroupBox^ groupBox1;
    private: System::Windows::Forms::CheckBox^ gpuCheck;
    private: System::Windows::Forms::CheckBox^ cpuCheck;
    private: System::Windows::Forms::CheckBox^ networkCheck;
    private: System::Windows::Forms::CheckBox^ batteryCheck;
    private: System::Windows::Forms::CheckBox^ storageCheck;
    private: System::Windows::Forms::CheckBox^ motherBoardCheck;
    private: System::Windows::Forms::Label^ label2;
    private: System::Windows::Forms::CheckBox^ showTooltipCheck;
    private: System::Windows::Forms::Label^ label3;
    private: System::Windows::Forms::ComboBox^ decimalPlaceCombo;
    private: System::Windows::Forms::CheckBox^ memoryCheck;
    private: System::Windows::Forms::CheckBox^ controllerCheck;
    private: System::Windows::Forms::CheckBox^ psuCheck;
    private: System::Windows::Forms::CheckBox^ specifyValueWidthCheck;
    private: System::Windows::Forms::NumericUpDown^ valueWidthEdit;
    private: System::Windows::Forms::Label^ label4;
    private: System::Windows::Forms::GroupBox^ groupBox2;
    private: System::Windows::Forms::Label^ label5;
    private: System::Windows::Forms::ComboBox^ unitCombo;
    private: System::Windows::Forms::Button^ moveUpButton;
    private: System::Windows::Forms::Button^ moveDownButton;
    private: System::Windows::Forms::CheckBox^ showUnitCheck;


    private: System::Windows::Forms::Button^ addItemBtn;

    protected:
        bool IsSelectionValid();
        ItemInfo& GetSelectedItemInfo();
        void EnableControls();

        void listBox_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e);
        void OnDecimalPlaceComboBoxSelectedIndexChanged(Object^ sender, EventArgs^ e);
        void OnFormClosing(Object^ sender, FormClosingEventArgs^ e);
        void ListBox_DrawItem(Object^ sender, DrawItemEventArgs^ e);

	private:
		/// <summary>
		/// 必需的设计器变量。
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// 设计器支持所需的方法 - 不要修改
		/// 使用代码编辑器修改此方法的内容。
		/// </summary>
		void InitializeComponent(void)
		{
            System::ComponentModel::ComponentResourceManager^ resources = (gcnew System::ComponentModel::ComponentResourceManager(SettingsForm::typeid));
            this->monitorItemListBox = (gcnew System::Windows::Forms::ListBox());
            this->removeSelectBtn = (gcnew System::Windows::Forms::Button());
            this->label1 = (gcnew System::Windows::Forms::Label());
            this->cancelBtn = (gcnew System::Windows::Forms::Button());
            this->addItemBtn = (gcnew System::Windows::Forms::Button());
            this->groupBox1 = (gcnew System::Windows::Forms::GroupBox());
            this->psuCheck = (gcnew System::Windows::Forms::CheckBox());
            this->controllerCheck = (gcnew System::Windows::Forms::CheckBox());
            this->memoryCheck = (gcnew System::Windows::Forms::CheckBox());
            this->networkCheck = (gcnew System::Windows::Forms::CheckBox());
            this->batteryCheck = (gcnew System::Windows::Forms::CheckBox());
            this->storageCheck = (gcnew System::Windows::Forms::CheckBox());
            this->motherBoardCheck = (gcnew System::Windows::Forms::CheckBox());
            this->gpuCheck = (gcnew System::Windows::Forms::CheckBox());
            this->cpuCheck = (gcnew System::Windows::Forms::CheckBox());
            this->label2 = (gcnew System::Windows::Forms::Label());
            this->showTooltipCheck = (gcnew System::Windows::Forms::CheckBox());
            this->label3 = (gcnew System::Windows::Forms::Label());
            this->decimalPlaceCombo = (gcnew System::Windows::Forms::ComboBox());
            this->specifyValueWidthCheck = (gcnew System::Windows::Forms::CheckBox());
            this->valueWidthEdit = (gcnew System::Windows::Forms::NumericUpDown());
            this->label4 = (gcnew System::Windows::Forms::Label());
            this->groupBox2 = (gcnew System::Windows::Forms::GroupBox());
            this->showUnitCheck = (gcnew System::Windows::Forms::CheckBox());
            this->label5 = (gcnew System::Windows::Forms::Label());
            this->unitCombo = (gcnew System::Windows::Forms::ComboBox());
            this->moveUpButton = (gcnew System::Windows::Forms::Button());
            this->moveDownButton = (gcnew System::Windows::Forms::Button());
            this->groupBox1->SuspendLayout();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->valueWidthEdit))->BeginInit();
            this->groupBox2->SuspendLayout();
            this->SuspendLayout();
            // 
            // monitorItemListBox
            // 
            resources->ApplyResources(this->monitorItemListBox, L"monitorItemListBox");
            this->monitorItemListBox->FormattingEnabled = true;
            this->monitorItemListBox->Name = L"monitorItemListBox";
            // 
            // removeSelectBtn
            // 
            resources->ApplyResources(this->removeSelectBtn, L"removeSelectBtn");
            this->removeSelectBtn->Name = L"removeSelectBtn";
            this->removeSelectBtn->UseVisualStyleBackColor = true;
            this->removeSelectBtn->Click += gcnew System::EventHandler(this, &SettingsForm::removeSelectBtn_Click);
            // 
            // label1
            // 
            resources->ApplyResources(this->label1, L"label1");
            this->label1->Name = L"label1";
            // 
            // cancelBtn
            // 
            resources->ApplyResources(this->cancelBtn, L"cancelBtn");
            this->cancelBtn->DialogResult = System::Windows::Forms::DialogResult::Cancel;
            this->cancelBtn->Name = L"cancelBtn";
            this->cancelBtn->UseVisualStyleBackColor = true;
            // 
            // addItemBtn
            // 
            resources->ApplyResources(this->addItemBtn, L"addItemBtn");
            this->addItemBtn->Name = L"addItemBtn";
            this->addItemBtn->UseVisualStyleBackColor = true;
            this->addItemBtn->Click += gcnew System::EventHandler(this, &SettingsForm::addItemBtn_Click);
            // 
            // groupBox1
            // 
            resources->ApplyResources(this->groupBox1, L"groupBox1");
            this->groupBox1->Controls->Add(this->psuCheck);
            this->groupBox1->Controls->Add(this->controllerCheck);
            this->groupBox1->Controls->Add(this->memoryCheck);
            this->groupBox1->Controls->Add(this->networkCheck);
            this->groupBox1->Controls->Add(this->batteryCheck);
            this->groupBox1->Controls->Add(this->storageCheck);
            this->groupBox1->Controls->Add(this->motherBoardCheck);
            this->groupBox1->Controls->Add(this->gpuCheck);
            this->groupBox1->Controls->Add(this->cpuCheck);
            this->groupBox1->Name = L"groupBox1";
            this->groupBox1->TabStop = false;
            // 
            // psuCheck
            // 
            resources->ApplyResources(this->psuCheck, L"psuCheck");
            this->psuCheck->Name = L"psuCheck";
            this->psuCheck->UseVisualStyleBackColor = true;
            // 
            // controllerCheck
            // 
            resources->ApplyResources(this->controllerCheck, L"controllerCheck");
            this->controllerCheck->Name = L"controllerCheck";
            this->controllerCheck->UseVisualStyleBackColor = true;
            // 
            // memoryCheck
            // 
            resources->ApplyResources(this->memoryCheck, L"memoryCheck");
            this->memoryCheck->Name = L"memoryCheck";
            this->memoryCheck->UseVisualStyleBackColor = true;
            // 
            // networkCheck
            // 
            resources->ApplyResources(this->networkCheck, L"networkCheck");
            this->networkCheck->Name = L"networkCheck";
            this->networkCheck->UseVisualStyleBackColor = true;
            // 
            // batteryCheck
            // 
            resources->ApplyResources(this->batteryCheck, L"batteryCheck");
            this->batteryCheck->Name = L"batteryCheck";
            this->batteryCheck->UseVisualStyleBackColor = true;
            // 
            // storageCheck
            // 
            resources->ApplyResources(this->storageCheck, L"storageCheck");
            this->storageCheck->Name = L"storageCheck";
            this->storageCheck->UseVisualStyleBackColor = true;
            // 
            // motherBoardCheck
            // 
            resources->ApplyResources(this->motherBoardCheck, L"motherBoardCheck");
            this->motherBoardCheck->Name = L"motherBoardCheck";
            this->motherBoardCheck->UseVisualStyleBackColor = true;
            // 
            // gpuCheck
            // 
            resources->ApplyResources(this->gpuCheck, L"gpuCheck");
            this->gpuCheck->Name = L"gpuCheck";
            this->gpuCheck->UseVisualStyleBackColor = true;
            // 
            // cpuCheck
            // 
            resources->ApplyResources(this->cpuCheck, L"cpuCheck");
            this->cpuCheck->Name = L"cpuCheck";
            this->cpuCheck->UseVisualStyleBackColor = true;
            // 
            // label2
            // 
            resources->ApplyResources(this->label2, L"label2");
            this->label2->Name = L"label2";
            // 
            // showTooltipCheck
            // 
            resources->ApplyResources(this->showTooltipCheck, L"showTooltipCheck");
            this->showTooltipCheck->Name = L"showTooltipCheck";
            this->showTooltipCheck->UseVisualStyleBackColor = true;
            // 
            // label3
            // 
            resources->ApplyResources(this->label3, L"label3");
            this->label3->Name = L"label3";
            // 
            // decimalPlaceCombo
            // 
            this->decimalPlaceCombo->FormattingEnabled = true;
            resources->ApplyResources(this->decimalPlaceCombo, L"decimalPlaceCombo");
            this->decimalPlaceCombo->Name = L"decimalPlaceCombo";
            // 
            // specifyValueWidthCheck
            // 
            resources->ApplyResources(this->specifyValueWidthCheck, L"specifyValueWidthCheck");
            this->specifyValueWidthCheck->Name = L"specifyValueWidthCheck";
            this->specifyValueWidthCheck->UseVisualStyleBackColor = true;
            this->specifyValueWidthCheck->CheckedChanged += gcnew System::EventHandler(this, &SettingsForm::specifyValueWidthCheck_CheckedChanged);
            // 
            // valueWidthEdit
            // 
            resources->ApplyResources(this->valueWidthEdit, L"valueWidthEdit");
            this->valueWidthEdit->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) { 20, 0, 0, 0 });
            this->valueWidthEdit->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) { 1, 0, 0, 0 });
            this->valueWidthEdit->Name = L"valueWidthEdit";
            this->valueWidthEdit->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) { 1, 0, 0, 0 });
            this->valueWidthEdit->ValueChanged += gcnew System::EventHandler(this, &SettingsForm::valueWidthEdit_ValueChanged);
            // 
            // label4
            // 
            resources->ApplyResources(this->label4, L"label4");
            this->label4->Name = L"label4";
            // 
            // groupBox2
            // 
            resources->ApplyResources(this->groupBox2, L"groupBox2");
            this->groupBox2->Controls->Add(this->showUnitCheck);
            this->groupBox2->Controls->Add(this->label5);
            this->groupBox2->Controls->Add(this->label3);
            this->groupBox2->Controls->Add(this->label4);
            this->groupBox2->Controls->Add(this->unitCombo);
            this->groupBox2->Controls->Add(this->decimalPlaceCombo);
            this->groupBox2->Controls->Add(this->valueWidthEdit);
            this->groupBox2->Controls->Add(this->specifyValueWidthCheck);
            this->groupBox2->Name = L"groupBox2";
            this->groupBox2->TabStop = false;
            // 
            // showUnitCheck
            // 
            resources->ApplyResources(this->showUnitCheck, L"showUnitCheck");
            this->showUnitCheck->Name = L"showUnitCheck";
            this->showUnitCheck->UseVisualStyleBackColor = true;
            this->showUnitCheck->CheckedChanged += gcnew System::EventHandler(this, &SettingsForm::showUnitCheck_CheckedChanged);
            // 
            // label5
            // 
            resources->ApplyResources(this->label5, L"label5");
            this->label5->Name = L"label5";
            // 
            // unitCombo
            // 
            this->unitCombo->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
            this->unitCombo->FormattingEnabled = true;
            resources->ApplyResources(this->unitCombo, L"unitCombo");
            this->unitCombo->Name = L"unitCombo";
            this->unitCombo->SelectedIndexChanged += gcnew System::EventHandler(this, &SettingsForm::unitCombo_SelectedIndexChanged);
            // 
            // moveUpButton
            // 
            resources->ApplyResources(this->moveUpButton, L"moveUpButton");
            this->moveUpButton->Name = L"moveUpButton";
            this->moveUpButton->UseVisualStyleBackColor = true;
            this->moveUpButton->Click += gcnew System::EventHandler(this, &SettingsForm::moveUpButton_Click);
            // 
            // moveDownButton
            // 
            resources->ApplyResources(this->moveDownButton, L"moveDownButton");
            this->moveDownButton->Name = L"moveDownButton";
            this->moveDownButton->UseVisualStyleBackColor = true;
            this->moveDownButton->Click += gcnew System::EventHandler(this, &SettingsForm::moveDownButton_Click);
            // 
            // SettingsForm
            // 
            resources->ApplyResources(this, L"$this");
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
            this->Controls->Add(this->groupBox2);
            this->Controls->Add(this->showTooltipCheck);
            this->Controls->Add(this->label2);
            this->Controls->Add(this->groupBox1);
            this->Controls->Add(this->cancelBtn);
            this->Controls->Add(this->label1);
            this->Controls->Add(this->addItemBtn);
            this->Controls->Add(this->moveDownButton);
            this->Controls->Add(this->moveUpButton);
            this->Controls->Add(this->removeSelectBtn);
            this->Controls->Add(this->monitorItemListBox);
            this->Name = L"SettingsForm";
            this->Resize += gcnew System::EventHandler(this, &SettingsForm::SettingsForm_Resize);
            this->groupBox1->ResumeLayout(false);
            this->groupBox1->PerformLayout();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->valueWidthEdit))->EndInit();
            this->groupBox2->ResumeLayout(false);
            this->groupBox2->PerformLayout();
            this->ResumeLayout(false);
            this->PerformLayout();

        }
#pragma endregion
    private: System::Void removeSelectBtn_Click(System::Object^ sender, System::EventArgs^ e);
    private: System::Void addItemBtn_Click(System::Object^ sender, System::EventArgs^ e);
    private: System::Void specifyValueWidthCheck_CheckedChanged(System::Object^ sender, System::EventArgs^ e);
    private: System::Void valueWidthEdit_ValueChanged(System::Object^ sender, System::EventArgs^ e);
    private: System::Void unitCombo_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e);
    private: System::Void moveUpButton_Click(System::Object^ sender, System::EventArgs^ e);
    private: System::Void moveDownButton_Click(System::Object^ sender, System::EventArgs^ e);
    private: System::Void SettingsForm_Resize(System::Object^ sender, System::EventArgs^ e);
    private: System::Void showUnitCheck_CheckedChanged(System::Object^ sender, System::EventArgs^ e);
};
}
