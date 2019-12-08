namespace pcsx2_Updater
{
    partial class frmMain
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.btnUpdate = new System.Windows.Forms.Button();
            this.labelUpdateIntro = new System.Windows.Forms.Label();
            this.btnLater = new System.Windows.Forms.Button();
            this.checkEnabled = new System.Windows.Forms.CheckBox();
            this.btnSkip = new System.Windows.Forms.Button();
            this.labelVersion = new System.Windows.Forms.Label();
            this.cbChannel = new System.Windows.Forms.ComboBox();
            this.label3 = new System.Windows.Forms.Label();
            this.tableUpdates = new System.Windows.Forms.DataGridView();
            this.Patch = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.Author = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.DateTime = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.Description = new System.Windows.Forms.DataGridViewTextBoxColumn();
            ((System.ComponentModel.ISupportInitialize)(this.tableUpdates)).BeginInit();
            this.SuspendLayout();
            // 
            // btnUpdate
            // 
            this.btnUpdate.Location = new System.Drawing.Point(713, 447);
            this.btnUpdate.Name = "btnUpdate";
            this.btnUpdate.Size = new System.Drawing.Size(89, 23);
            this.btnUpdate.TabIndex = 1;
            this.btnUpdate.Text = "Update Now";
            this.btnUpdate.UseVisualStyleBackColor = true;
            this.btnUpdate.Click += new System.EventHandler(this.btnUpdate_Click);
            // 
            // labelUpdateIntro
            // 
            this.labelUpdateIntro.AutoSize = true;
            this.labelUpdateIntro.Location = new System.Drawing.Point(2, 9);
            this.labelUpdateIntro.Name = "labelUpdateIntro";
            this.labelUpdateIntro.Size = new System.Drawing.Size(400, 13);
            this.labelUpdateIntro.TabIndex = 2;
            this.labelUpdateIntro.Text = "A new pre-release build of PCSX2 has been released, the changes are listed below." +
    "";
            // 
            // btnLater
            // 
            this.btnLater.Location = new System.Drawing.Point(618, 447);
            this.btnLater.Name = "btnLater";
            this.btnLater.Size = new System.Drawing.Size(89, 23);
            this.btnLater.TabIndex = 3;
            this.btnLater.Text = "Update Later";
            this.btnLater.UseVisualStyleBackColor = true;
            this.btnLater.Click += new System.EventHandler(this.BtnLater_Click);
            // 
            // checkEnabled
            // 
            this.checkEnabled.AutoSize = true;
            this.checkEnabled.Checked = true;
            this.checkEnabled.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkEnabled.Location = new System.Drawing.Point(3, 452);
            this.checkEnabled.Name = "checkEnabled";
            this.checkEnabled.Size = new System.Drawing.Size(267, 17);
            this.checkEnabled.TabIndex = 4;
            this.checkEnabled.Text = "Show this window whenever an update is detected";
            this.checkEnabled.UseVisualStyleBackColor = true;
            this.checkEnabled.CheckedChanged += new System.EventHandler(this.CheckEnabled_CheckedChanged);
            // 
            // btnSkip
            // 
            this.btnSkip.Location = new System.Drawing.Point(523, 447);
            this.btnSkip.Name = "btnSkip";
            this.btnSkip.Size = new System.Drawing.Size(89, 23);
            this.btnSkip.TabIndex = 5;
            this.btnSkip.Text = "Skip this build";
            this.btnSkip.UseVisualStyleBackColor = true;
            this.btnSkip.Click += new System.EventHandler(this.BtnSkip_Click);
            // 
            // labelVersion
            // 
            this.labelVersion.AutoSize = true;
            this.labelVersion.Location = new System.Drawing.Point(587, 3);
            this.labelVersion.Name = "labelVersion";
            this.labelVersion.Size = new System.Drawing.Size(215, 26);
            this.labelVersion.TabIndex = 6;
            this.labelVersion.Text = "Installed build: v1.5.0-dev-3279-gf2b402b0c\r\nLatest build: v1.5.0-dev-3283-ge2d89" +
    "9231";
            this.labelVersion.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // cbChannel
            // 
            this.cbChannel.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cbChannel.FormattingEnabled = true;
            this.cbChannel.Items.AddRange(new object[] {
            "Development",
            "Stable"});
            this.cbChannel.Location = new System.Drawing.Point(379, 448);
            this.cbChannel.Name = "cbChannel";
            this.cbChannel.Size = new System.Drawing.Size(121, 21);
            this.cbChannel.TabIndex = 7;
            this.cbChannel.SelectedIndexChanged += new System.EventHandler(this.CbChannel_SelectedIndexChanged);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(292, 453);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(85, 13);
            this.label3.TabIndex = 8;
            this.label3.Text = "Current channel:";
            this.label3.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // tableUpdates
            // 
            this.tableUpdates.AllowUserToAddRows = false;
            this.tableUpdates.AllowUserToDeleteRows = false;
            this.tableUpdates.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.tableUpdates.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.Patch,
            this.Author,
            this.DateTime,
            this.Description});
            this.tableUpdates.Location = new System.Drawing.Point(3, 32);
            this.tableUpdates.MultiSelect = false;
            this.tableUpdates.Name = "tableUpdates";
            this.tableUpdates.ReadOnly = true;
            this.tableUpdates.RowTemplate.ReadOnly = true;
            this.tableUpdates.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.tableUpdates.ShowEditingIcon = false;
            this.tableUpdates.Size = new System.Drawing.Size(799, 409);
            this.tableUpdates.TabIndex = 9;
            // 
            // Patch
            // 
            this.Patch.HeaderText = "Revision";
            this.Patch.Name = "Patch";
            this.Patch.ReadOnly = true;
            this.Patch.Width = 150;
            // 
            // Author
            // 
            this.Author.HeaderText = "Author";
            this.Author.Name = "Author";
            this.Author.ReadOnly = true;
            // 
            // DateTime
            // 
            this.DateTime.HeaderText = "Date";
            this.DateTime.Name = "DateTime";
            this.DateTime.ReadOnly = true;
            this.DateTime.Width = 130;
            // 
            // Description
            // 
            this.Description.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.Fill;
            this.Description.HeaderText = "Description";
            this.Description.Name = "Description";
            this.Description.ReadOnly = true;
            // 
            // frmMain
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(804, 473);
            this.Controls.Add(this.tableUpdates);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.cbChannel);
            this.Controls.Add(this.labelVersion);
            this.Controls.Add(this.btnSkip);
            this.Controls.Add(this.checkEnabled);
            this.Controls.Add(this.btnLater);
            this.Controls.Add(this.labelUpdateIntro);
            this.Controls.Add(this.btnUpdate);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
            this.Name = "frmMain";
            this.ShowIcon = false;
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.Text = "PCSX2 Update Available!";
            this.TopMost = true;
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.frmMain_FormClosing);
            this.Load += new System.EventHandler(this.frmMain_Load);
            ((System.ComponentModel.ISupportInitialize)(this.tableUpdates)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion
        private System.Windows.Forms.Button btnUpdate;
        private System.Windows.Forms.Label labelUpdateIntro;
        private System.Windows.Forms.Button btnLater;
        private System.Windows.Forms.CheckBox checkEnabled;
        private System.Windows.Forms.Button btnSkip;
        private System.Windows.Forms.Label labelVersion;
        private System.Windows.Forms.ComboBox cbChannel;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.DataGridView tableUpdates;
        private System.Windows.Forms.DataGridViewTextBoxColumn Patch;
        private System.Windows.Forms.DataGridViewTextBoxColumn Author;
        private System.Windows.Forms.DataGridViewTextBoxColumn DateTime;
        private System.Windows.Forms.DataGridViewTextBoxColumn Description;
    }
}

