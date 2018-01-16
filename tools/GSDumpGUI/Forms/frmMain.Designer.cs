namespace GSDumpGUI
{
    partial class GSDumpGUI
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(GSDumpGUI));
            this.txtGSDXDirectory = new System.Windows.Forms.TextBox();
            this.lblDirectory = new System.Windows.Forms.Label();
            this.cmdBrowseGSDX = new System.Windows.Forms.Button();
            this.cmdBrowseDumps = new System.Windows.Forms.Button();
            this.lblDumpDirectory = new System.Windows.Forms.Label();
            this.txtDumpsDirectory = new System.Windows.Forms.TextBox();
            this.lstGSDX = new System.Windows.Forms.ListBox();
            this.lstDumps = new System.Windows.Forms.ListBox();
            this.lblDumps = new System.Windows.Forms.Label();
            this.GsdxList = new System.Windows.Forms.Label();
            this.cmdRun = new System.Windows.Forms.Button();
            this.cmdConfigGSDX = new System.Windows.Forms.Button();
            this.txtLog = new System.Windows.Forms.TextBox();
            this.lblLog = new System.Windows.Forms.Label();
            this.cmdOpenIni = new System.Windows.Forms.Button();
            this.pctBox = new System.Windows.Forms.PictureBox();
            this.rdaDX9HW = new System.Windows.Forms.RadioButton();
            this.rdaDX1011HW = new System.Windows.Forms.RadioButton();
            this.rdaOGLHW = new System.Windows.Forms.RadioButton();
            this.rdaDX9SW = new System.Windows.Forms.RadioButton();
            this.rdaDX1011SW = new System.Windows.Forms.RadioButton();
            this.rdaOGLSW = new System.Windows.Forms.RadioButton();
            this.lblOverride = new System.Windows.Forms.Label();
            this.rdaNone = new System.Windows.Forms.RadioButton();
            this.lblInternalLog = new System.Windows.Forms.Label();
            this.txtIntLog = new System.Windows.Forms.TextBox();
            this.lblDebugger = new System.Windows.Forms.Label();
            this.lstProcesses = new System.Windows.Forms.ListBox();
            this.lblChild = new System.Windows.Forms.Label();
            this.lblDumpSize = new System.Windows.Forms.Label();
            this.txtDumpSize = new System.Windows.Forms.Label();
            this.txtGIFPackets = new System.Windows.Forms.Label();
            this.lblGIFPackets = new System.Windows.Forms.Label();
            this.txtPath1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.txtPath2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.txtPath3 = new System.Windows.Forms.Label();
            this.label5 = new System.Windows.Forms.Label();
            this.txtVSync = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.txtReadFifo = new System.Windows.Forms.Label();
            this.label7 = new System.Windows.Forms.Label();
            this.txtRegisters = new System.Windows.Forms.Label();
            this.label6 = new System.Windows.Forms.Label();
            this.chkDebugMode = new System.Windows.Forms.CheckBox();
            this.lblGif = new System.Windows.Forms.Label();
            this.btnStep = new System.Windows.Forms.Button();
            this.btnRunToSelection = new System.Windows.Forms.Button();
            this.treTreeView = new System.Windows.Forms.TreeView();
            this.cmdGoToStart = new System.Windows.Forms.Button();
            this.cmdGoToNextVSync = new System.Windows.Forms.Button();
            this.txtGifPacketSize = new System.Windows.Forms.Label();
            this.lblGIFPacketSize = new System.Windows.Forms.Label();
            this.treeGifPacketContent = new System.Windows.Forms.TreeView();
            this.lblContent = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.pctBox)).BeginInit();
            this.SuspendLayout();
            // 
            // txtGSDXDirectory
            // 
            this.txtGSDXDirectory.Location = new System.Drawing.Point(871, 24);
            this.txtGSDXDirectory.Name = "txtGSDXDirectory";
            this.txtGSDXDirectory.Size = new System.Drawing.Size(243, 20);
            this.txtGSDXDirectory.TabIndex = 0;
            this.txtGSDXDirectory.TabStop = false;
            this.txtGSDXDirectory.Leave += new System.EventHandler(this.txtGSDXDirectory_Leave);
            // 
            // lblDirectory
            // 
            this.lblDirectory.AutoSize = true;
            this.lblDirectory.Location = new System.Drawing.Point(871, 10);
            this.lblDirectory.Name = "lblDirectory";
            this.lblDirectory.Size = new System.Drawing.Size(78, 13);
            this.lblDirectory.TabIndex = 1;
            this.lblDirectory.Text = "GSdx Directory";
            // 
            // cmdBrowseGSDX
            // 
            this.cmdBrowseGSDX.Location = new System.Drawing.Point(1120, 24);
            this.cmdBrowseGSDX.Name = "cmdBrowseGSDX";
            this.cmdBrowseGSDX.Size = new System.Drawing.Size(26, 22);
            this.cmdBrowseGSDX.TabIndex = 2;
            this.cmdBrowseGSDX.TabStop = false;
            this.cmdBrowseGSDX.Text = "...";
            this.cmdBrowseGSDX.UseVisualStyleBackColor = true;
            this.cmdBrowseGSDX.Click += new System.EventHandler(this.cmdBrowseGSDX_Click);
            // 
            // cmdBrowseDumps
            // 
            this.cmdBrowseDumps.Location = new System.Drawing.Point(1120, 64);
            this.cmdBrowseDumps.Name = "cmdBrowseDumps";
            this.cmdBrowseDumps.Size = new System.Drawing.Size(26, 23);
            this.cmdBrowseDumps.TabIndex = 5;
            this.cmdBrowseDumps.TabStop = false;
            this.cmdBrowseDumps.Text = "...";
            this.cmdBrowseDumps.UseVisualStyleBackColor = true;
            this.cmdBrowseDumps.Click += new System.EventHandler(this.cmdBrowseDumps_Click);
            // 
            // lblDumpDirectory
            // 
            this.lblDumpDirectory.AutoSize = true;
            this.lblDumpDirectory.Location = new System.Drawing.Point(871, 51);
            this.lblDumpDirectory.Name = "lblDumpDirectory";
            this.lblDumpDirectory.Size = new System.Drawing.Size(85, 13);
            this.lblDumpDirectory.TabIndex = 4;
            this.lblDumpDirectory.Text = "Dumps Directory";
            // 
            // txtDumpsDirectory
            // 
            this.txtDumpsDirectory.Location = new System.Drawing.Point(871, 67);
            this.txtDumpsDirectory.Name = "txtDumpsDirectory";
            this.txtDumpsDirectory.Size = new System.Drawing.Size(243, 20);
            this.txtDumpsDirectory.TabIndex = 3;
            this.txtDumpsDirectory.TabStop = false;
            this.txtDumpsDirectory.Leave += new System.EventHandler(this.txtDumpsDirectory_Leave);
            // 
            // lstGSDX
            // 
            this.lstGSDX.FormattingEnabled = true;
            this.lstGSDX.Location = new System.Drawing.Point(451, 24);
            this.lstGSDX.Name = "lstGSDX";
            this.lstGSDX.Size = new System.Drawing.Size(411, 173);
            this.lstGSDX.TabIndex = 1;
            // 
            // lstDumps
            // 
            this.lstDumps.FormattingEnabled = true;
            this.lstDumps.Location = new System.Drawing.Point(12, 24);
            this.lstDumps.Name = "lstDumps";
            this.lstDumps.Size = new System.Drawing.Size(433, 173);
            this.lstDumps.TabIndex = 0;
            this.lstDumps.SelectedIndexChanged += new System.EventHandler(this.lstDumps_SelectedIndexChanged);
            // 
            // lblDumps
            // 
            this.lblDumps.AutoSize = true;
            this.lblDumps.Location = new System.Drawing.Point(9, 8);
            this.lblDumps.Name = "lblDumps";
            this.lblDumps.Size = new System.Drawing.Size(59, 13);
            this.lblDumps.TabIndex = 9;
            this.lblDumps.Text = "Dumps List";
            // 
            // GsdxList
            // 
            this.GsdxList.AutoSize = true;
            this.GsdxList.Location = new System.Drawing.Point(451, 8);
            this.GsdxList.Name = "GsdxList";
            this.GsdxList.Size = new System.Drawing.Size(52, 13);
            this.GsdxList.TabIndex = 10;
            this.GsdxList.Text = "GSdx List";
            // 
            // cmdRun
            // 
            this.cmdRun.Location = new System.Drawing.Point(871, 167);
            this.cmdRun.Name = "cmdRun";
            this.cmdRun.Size = new System.Drawing.Size(275, 30);
            this.cmdRun.TabIndex = 11;
            this.cmdRun.TabStop = false;
            this.cmdRun.Text = "Run";
            this.cmdRun.UseVisualStyleBackColor = true;
            this.cmdRun.Click += new System.EventHandler(this.cmdRun_Click);
            // 
            // cmdConfigGSDX
            // 
            this.cmdConfigGSDX.Location = new System.Drawing.Point(1051, 93);
            this.cmdConfigGSDX.Name = "cmdConfigGSDX";
            this.cmdConfigGSDX.Size = new System.Drawing.Size(95, 32);
            this.cmdConfigGSDX.TabIndex = 12;
            this.cmdConfigGSDX.TabStop = false;
            this.cmdConfigGSDX.Text = "Configure GSdx";
            this.cmdConfigGSDX.UseVisualStyleBackColor = true;
            this.cmdConfigGSDX.Click += new System.EventHandler(this.cmdConfigGSDX_Click);
            // 
            // txtLog
            // 
            this.txtLog.Location = new System.Drawing.Point(15, 225);
            this.txtLog.Multiline = true;
            this.txtLog.Name = "txtLog";
            this.txtLog.ReadOnly = true;
            this.txtLog.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.txtLog.Size = new System.Drawing.Size(430, 160);
            this.txtLog.TabIndex = 13;
            this.txtLog.TabStop = false;
            // 
            // lblLog
            // 
            this.lblLog.AutoSize = true;
            this.lblLog.Location = new System.Drawing.Point(12, 209);
            this.lblLog.Name = "lblLog";
            this.lblLog.Size = new System.Drawing.Size(58, 13);
            this.lblLog.TabIndex = 14;
            this.lblLog.Text = "Log GSdx";
            // 
            // cmdOpenIni
            // 
            this.cmdOpenIni.Location = new System.Drawing.Point(1051, 130);
            this.cmdOpenIni.Name = "cmdOpenIni";
            this.cmdOpenIni.Size = new System.Drawing.Size(95, 32);
            this.cmdOpenIni.TabIndex = 15;
            this.cmdOpenIni.TabStop = false;
            this.cmdOpenIni.Text = "Open GSdx.ini";
            this.cmdOpenIni.UseVisualStyleBackColor = true;
            this.cmdOpenIni.Click += new System.EventHandler(this.cmdOpenIni_Click);
            // 
            // pctBox
            // 
            this.pctBox.Location = new System.Drawing.Point(871, 225);
            this.pctBox.Name = "pctBox";
            this.pctBox.Size = new System.Drawing.Size(275, 160);
            this.pctBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.StretchImage;
            this.pctBox.TabIndex = 16;
            this.pctBox.TabStop = false;
            this.pctBox.Click += new System.EventHandler(this.pctBox_Click);
            // 
            // rdaDX9HW
            // 
            this.rdaDX9HW.AutoSize = true;
            this.rdaDX9HW.Location = new System.Drawing.Point(874, 111);
            this.rdaDX9HW.Name = "rdaDX9HW";
            this.rdaDX9HW.Size = new System.Drawing.Size(75, 17);
            this.rdaDX9HW.TabIndex = 17;
            this.rdaDX9HW.Tag = "1";
            this.rdaDX9HW.Text = "D3D9 HW";
            this.rdaDX9HW.UseVisualStyleBackColor = true;
            this.rdaDX9HW.CheckedChanged += new System.EventHandler(this.rda_CheckedChanged);
            // 
            // rdaDX1011HW
            // 
            this.rdaDX1011HW.AutoSize = true;
            this.rdaDX1011HW.Location = new System.Drawing.Point(874, 129);
            this.rdaDX1011HW.Name = "rdaDX1011HW";
            this.rdaDX1011HW.Size = new System.Drawing.Size(81, 17);
            this.rdaDX1011HW.TabIndex = 18;
            this.rdaDX1011HW.Tag = "2";
            this.rdaDX1011HW.Text = "D3D11 HW";
            this.rdaDX1011HW.UseVisualStyleBackColor = true;
            this.rdaDX1011HW.CheckedChanged += new System.EventHandler(this.rda_CheckedChanged);
            // 
            // rdaOGLHW
            // 
            this.rdaOGLHW.AutoSize = true;
            this.rdaOGLHW.Location = new System.Drawing.Point(874, 147);
            this.rdaOGLHW.Name = "rdaOGLHW";
            this.rdaOGLHW.Size = new System.Drawing.Size(69, 17);
            this.rdaOGLHW.TabIndex = 19;
            this.rdaOGLHW.Tag = "3";
            this.rdaOGLHW.Text = "OGL HW";
            this.rdaOGLHW.UseVisualStyleBackColor = true;
            this.rdaOGLHW.CheckedChanged += new System.EventHandler(this.rda_CheckedChanged);
            // 
            // rdaDX9SW
            // 
            this.rdaDX9SW.AutoSize = true;
            this.rdaDX9SW.Location = new System.Drawing.Point(971, 111);
            this.rdaDX9SW.Name = "rdaDX9SW";
            this.rdaDX9SW.Size = new System.Drawing.Size(74, 17);
            this.rdaDX9SW.TabIndex = 20;
            this.rdaDX9SW.Tag = "4";
            this.rdaDX9SW.Text = "D3D9 SW";
            this.rdaDX9SW.UseVisualStyleBackColor = true;
            this.rdaDX9SW.CheckedChanged += new System.EventHandler(this.rda_CheckedChanged);
            // 
            // rdaDX1011SW
            // 
            this.rdaDX1011SW.AutoSize = true;
            this.rdaDX1011SW.Location = new System.Drawing.Point(971, 129);
            this.rdaDX1011SW.Name = "rdaDX1011SW";
            this.rdaDX1011SW.Size = new System.Drawing.Size(80, 17);
            this.rdaDX1011SW.TabIndex = 21;
            this.rdaDX1011SW.Tag = "5";
            this.rdaDX1011SW.Text = "D3D11 SW";
            this.rdaDX1011SW.UseVisualStyleBackColor = true;
            this.rdaDX1011SW.CheckedChanged += new System.EventHandler(this.rda_CheckedChanged);
            // 
            // rdaOGLSW
            // 
            this.rdaOGLSW.AutoSize = true;
            this.rdaOGLSW.Location = new System.Drawing.Point(971, 147);
            this.rdaOGLSW.Name = "rdaOGLSW";
            this.rdaOGLSW.Size = new System.Drawing.Size(68, 17);
            this.rdaOGLSW.TabIndex = 22;
            this.rdaOGLSW.Tag = "6";
            this.rdaOGLSW.Text = "OGL SW";
            this.rdaOGLSW.UseVisualStyleBackColor = true;
            this.rdaOGLSW.CheckedChanged += new System.EventHandler(this.rda_CheckedChanged);
            // 
            // lblOverride
            // 
            this.lblOverride.AutoSize = true;
            this.lblOverride.Location = new System.Drawing.Point(871, 93);
            this.lblOverride.Name = "lblOverride";
            this.lblOverride.Size = new System.Drawing.Size(94, 13);
            this.lblOverride.TabIndex = 23;
            this.lblOverride.Text = "Renderer Override";
            // 
            // rdaNone
            // 
            this.rdaNone.AutoSize = true;
            this.rdaNone.Checked = true;
            this.rdaNone.Location = new System.Drawing.Point(971, 93);
            this.rdaNone.Name = "rdaNone";
            this.rdaNone.Size = new System.Drawing.Size(51, 17);
            this.rdaNone.TabIndex = 24;
            this.rdaNone.TabStop = true;
            this.rdaNone.Tag = "0";
            this.rdaNone.Text = "None";
            this.rdaNone.UseVisualStyleBackColor = true;
            this.rdaNone.CheckedChanged += new System.EventHandler(this.rda_CheckedChanged);
            // 
            // lblInternalLog
            // 
            this.lblInternalLog.AutoSize = true;
            this.lblInternalLog.Location = new System.Drawing.Point(451, 209);
            this.lblInternalLog.Name = "lblInternalLog";
            this.lblInternalLog.Size = new System.Drawing.Size(63, 13);
            this.lblInternalLog.TabIndex = 25;
            this.lblInternalLog.Text = "Log Internal";
            // 
            // txtIntLog
            // 
            this.txtIntLog.Location = new System.Drawing.Point(451, 225);
            this.txtIntLog.Multiline = true;
            this.txtIntLog.Name = "txtIntLog";
            this.txtIntLog.ReadOnly = true;
            this.txtIntLog.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.txtIntLog.Size = new System.Drawing.Size(411, 160);
            this.txtIntLog.TabIndex = 24;
            this.txtIntLog.TabStop = false;
            // 
            // lblDebugger
            // 
            this.lblDebugger.AutoSize = true;
            this.lblDebugger.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.lblDebugger.Location = new System.Drawing.Point(417, 398);
            this.lblDebugger.Name = "lblDebugger";
            this.lblDebugger.Size = new System.Drawing.Size(62, 13);
            this.lblDebugger.TabIndex = 26;
            this.lblDebugger.Text = "Debugger";
            // 
            // lstProcesses
            // 
            this.lstProcesses.FormattingEnabled = true;
            this.lstProcesses.Location = new System.Drawing.Point(12, 430);
            this.lstProcesses.Name = "lstProcesses";
            this.lstProcesses.Size = new System.Drawing.Size(248, 277);
            this.lstProcesses.TabIndex = 27;
            this.lstProcesses.SelectedIndexChanged += new System.EventHandler(this.lstProcesses_SelectedIndexChanged);
            // 
            // lblChild
            // 
            this.lblChild.AutoSize = true;
            this.lblChild.Location = new System.Drawing.Point(9, 414);
            this.lblChild.Name = "lblChild";
            this.lblChild.Size = new System.Drawing.Size(82, 13);
            this.lblChild.TabIndex = 28;
            this.lblChild.Text = "Child Processes";
            // 
            // lblDumpSize
            // 
            this.lblDumpSize.AutoSize = true;
            this.lblDumpSize.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.lblDumpSize.Location = new System.Drawing.Point(279, 430);
            this.lblDumpSize.Name = "lblDumpSize";
            this.lblDumpSize.Size = new System.Drawing.Size(67, 13);
            this.lblDumpSize.TabIndex = 29;
            this.lblDumpSize.Text = "Dump Size";
            // 
            // txtDumpSize
            // 
            this.txtDumpSize.AutoSize = true;
            this.txtDumpSize.Location = new System.Drawing.Point(279, 445);
            this.txtDumpSize.Name = "txtDumpSize";
            this.txtDumpSize.Size = new System.Drawing.Size(0, 13);
            this.txtDumpSize.TabIndex = 30;
            this.txtDumpSize.Text = "N/A";
            // 
            // txtGIFPackets
            // 
            this.txtGIFPackets.AutoSize = true;
            this.txtGIFPackets.Location = new System.Drawing.Point(279, 478);
            this.txtGIFPackets.Name = "txtGIFPackets";
            this.txtGIFPackets.Size = new System.Drawing.Size(0, 13);
            this.txtGIFPackets.TabIndex = 33;
            this.txtGIFPackets.Text = "N/A";
            // 
            // lblGIFPackets
            // 
            this.lblGIFPackets.AutoSize = true;
            this.lblGIFPackets.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.lblGIFPackets.Location = new System.Drawing.Point(279, 463);
            this.lblGIFPackets.Name = "lblGIFPackets";
            this.lblGIFPackets.Size = new System.Drawing.Size(110, 13);
            this.lblGIFPackets.TabIndex = 32;
            this.lblGIFPackets.Text = "Total GIF Packets";
            // 
            // txtPath1
            // 
            this.txtPath1.AutoSize = true;
            this.txtPath1.Location = new System.Drawing.Point(279, 512);
            this.txtPath1.Name = "txtPath1";
            this.txtPath1.Size = new System.Drawing.Size(0, 13);
            this.txtPath1.TabIndex = 35;
            this.txtPath1.Text = "N/A";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label2.Location = new System.Drawing.Point(279, 497);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(114, 13);
            this.label2.TabIndex = 34;
            this.label2.Text = "Path1 GIF Packets";
            // 
            // txtPath2
            // 
            this.txtPath2.AutoSize = true;
            this.txtPath2.Location = new System.Drawing.Point(279, 546);
            this.txtPath2.Name = "txtPath2";
            this.txtPath2.Size = new System.Drawing.Size(0, 13);
            this.txtPath2.TabIndex = 37;
            this.txtPath2.Text = "N/A";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label3.Location = new System.Drawing.Point(279, 531);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(114, 13);
            this.label3.TabIndex = 36;
            this.label3.Text = "Path2 GIF Packets";
            // 
            // txtPath3
            // 
            this.txtPath3.AutoSize = true;
            this.txtPath3.Location = new System.Drawing.Point(279, 580);
            this.txtPath3.Name = "txtPath3";
            this.txtPath3.Size = new System.Drawing.Size(0, 13);
            this.txtPath3.TabIndex = 39;
            this.txtPath3.Text = "N/A";
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label5.Location = new System.Drawing.Point(279, 565);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(114, 13);
            this.label5.TabIndex = 38;
            this.label5.Text = "Path3 GIF Packets";
            // 
            // txtVSync
            // 
            this.txtVSync.AutoSize = true;
            this.txtVSync.Location = new System.Drawing.Point(279, 615);
            this.txtVSync.Name = "txtVSync";
            this.txtVSync.Size = new System.Drawing.Size(0, 13);
            this.txtVSync.TabIndex = 41;
            this.txtVSync.Text = "N/A";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label4.Location = new System.Drawing.Point(279, 600);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(91, 13);
            this.label4.TabIndex = 40;
            this.label4.Text = "Vsync Packets";
            // 
            // txtReadFifo
            // 
            this.txtReadFifo.AutoSize = true;
            this.txtReadFifo.Location = new System.Drawing.Point(279, 649);
            this.txtReadFifo.Name = "txtReadFifo";
            this.txtReadFifo.Size = new System.Drawing.Size(0, 13);
            this.txtReadFifo.TabIndex = 43;
            this.txtReadFifo.Text = "N/A";
            // 
            // label7
            // 
            this.label7.AutoSize = true;
            this.label7.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label7.Location = new System.Drawing.Point(279, 634);
            this.label7.Name = "label7";
            this.label7.Size = new System.Drawing.Size(114, 13);
            this.label7.TabIndex = 42;
            this.label7.Text = "ReadFIFO Packets";
            // 
            // txtRegisters
            // 
            this.txtRegisters.AutoSize = true;
            this.txtRegisters.Location = new System.Drawing.Point(279, 684);
            this.txtRegisters.Name = "txtRegisters";
            this.txtRegisters.Size = new System.Drawing.Size(0, 13);
            this.txtRegisters.TabIndex = 45;
            this.txtRegisters.Text = "N/A";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label6.Location = new System.Drawing.Point(279, 669);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(110, 13);
            this.label6.TabIndex = 44;
            this.label6.Text = "Registers Packets";
            // 
            // chkDebugMode
            // 
            this.chkDebugMode.AutoSize = true;
            this.chkDebugMode.Enabled = false;
            this.chkDebugMode.Location = new System.Drawing.Point(759, 430);
            this.chkDebugMode.Name = "chkDebugMode";
            this.chkDebugMode.Size = new System.Drawing.Size(88, 17);
            this.chkDebugMode.TabIndex = 46;
            this.chkDebugMode.Text = "Debug Mode";
            this.chkDebugMode.UseVisualStyleBackColor = true;
            this.chkDebugMode.CheckedChanged += new System.EventHandler(this.chkDebugMode_CheckedChanged);
            // 
            // lblGif
            // 
            this.lblGif.AutoSize = true;
            this.lblGif.Enabled = false;
            this.lblGif.Location = new System.Drawing.Point(417, 415);
            this.lblGif.Name = "lblGif";
            this.lblGif.Size = new System.Drawing.Size(66, 13);
            this.lblGif.TabIndex = 48;
            this.lblGif.Text = "GIF Packets";
            // 
            // btnStep
            // 
            this.btnStep.Enabled = false;
            this.btnStep.Location = new System.Drawing.Point(759, 499);
            this.btnStep.Name = "btnStep";
            this.btnStep.Size = new System.Drawing.Size(108, 40);
            this.btnStep.TabIndex = 49;
            this.btnStep.TabStop = false;
            this.btnStep.Text = "Step";
            this.btnStep.UseVisualStyleBackColor = true;
            this.btnStep.Click += new System.EventHandler(this.btnStep_Click);
            // 
            // btnRunToSelection
            // 
            this.btnRunToSelection.Enabled = false;
            this.btnRunToSelection.Location = new System.Drawing.Point(759, 545);
            this.btnRunToSelection.Name = "btnRunToSelection";
            this.btnRunToSelection.Size = new System.Drawing.Size(108, 40);
            this.btnRunToSelection.TabIndex = 50;
            this.btnRunToSelection.TabStop = false;
            this.btnRunToSelection.Text = "Run To Selection";
            this.btnRunToSelection.UseVisualStyleBackColor = true;
            this.btnRunToSelection.Click += new System.EventHandler(this.btnRunToSelection_Click);
            // 
            // treTreeView
            // 
            this.treTreeView.Enabled = false;
            this.treTreeView.Location = new System.Drawing.Point(420, 431);
            this.treTreeView.Name = "treTreeView";
            this.treTreeView.Size = new System.Drawing.Size(332, 276);
            this.treTreeView.TabIndex = 51;
            this.treTreeView.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.treTreeView_AfterSelect);
            // 
            // cmdGoToStart
            // 
            this.cmdGoToStart.Enabled = false;
            this.cmdGoToStart.Location = new System.Drawing.Point(759, 453);
            this.cmdGoToStart.Name = "cmdGoToStart";
            this.cmdGoToStart.Size = new System.Drawing.Size(108, 40);
            this.cmdGoToStart.TabIndex = 52;
            this.cmdGoToStart.TabStop = false;
            this.cmdGoToStart.Text = "Go to Start";
            this.cmdGoToStart.UseVisualStyleBackColor = true;
            this.cmdGoToStart.Click += new System.EventHandler(this.cmdGoToStart_Click);
            // 
            // cmdGoToNextVSync
            // 
            this.cmdGoToNextVSync.Enabled = false;
            this.cmdGoToNextVSync.Location = new System.Drawing.Point(759, 591);
            this.cmdGoToNextVSync.Name = "cmdGoToNextVSync";
            this.cmdGoToNextVSync.Size = new System.Drawing.Size(108, 40);
            this.cmdGoToNextVSync.TabIndex = 53;
            this.cmdGoToNextVSync.TabStop = false;
            this.cmdGoToNextVSync.Text = "Go to next VSync";
            this.cmdGoToNextVSync.UseVisualStyleBackColor = true;
            this.cmdGoToNextVSync.Click += new System.EventHandler(this.cmdGoToNextVSync_Click);
            // 
            // txtGifPacketSize
            // 
            this.txtGifPacketSize.AutoSize = true;
            this.txtGifPacketSize.Location = new System.Drawing.Point(873, 430);
            this.txtGifPacketSize.Name = "txtGifPacketSize";
            this.txtGifPacketSize.Size = new System.Drawing.Size(0, 13);
            this.txtGifPacketSize.TabIndex = 55;
            // 
            // lblGIFPacketSize
            // 
            this.lblGIFPacketSize.AutoSize = true;
            this.lblGIFPacketSize.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.lblGIFPacketSize.Location = new System.Drawing.Point(871, 398);
            this.lblGIFPacketSize.Name = "lblGIFPacketSize";
            this.lblGIFPacketSize.Size = new System.Drawing.Size(95, 13);
            this.lblGIFPacketSize.TabIndex = 54;
            this.lblGIFPacketSize.Text = "Gif Packet Size";
            // 
            // treeGifPacketContent
            // 
            this.treeGifPacketContent.Enabled = false;
            this.treeGifPacketContent.Location = new System.Drawing.Point(874, 431);
            this.treeGifPacketContent.Name = "treeGifPacketContent";
            this.treeGifPacketContent.Size = new System.Drawing.Size(272, 276);
            this.treeGifPacketContent.TabIndex = 57;
            // 
            // lblContent
            // 
            this.lblContent.AutoSize = true;
            this.lblContent.Enabled = false;
            this.lblContent.Location = new System.Drawing.Point(871, 414);
            this.lblContent.Name = "lblContent";
            this.lblContent.Size = new System.Drawing.Size(101, 13);
            this.lblContent.TabIndex = 56;
            this.lblContent.Text = "GIF Packet Content";
            // 
            // GSDumpGUI
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1158, 718);
            this.Controls.Add(this.treeGifPacketContent);
            this.Controls.Add(this.lblContent);
            this.Controls.Add(this.txtGifPacketSize);
            this.Controls.Add(this.lblGIFPacketSize);
            this.Controls.Add(this.cmdGoToNextVSync);
            this.Controls.Add(this.cmdGoToStart);
            this.Controls.Add(this.treTreeView);
            this.Controls.Add(this.btnRunToSelection);
            this.Controls.Add(this.btnStep);
            this.Controls.Add(this.lblGif);
            this.Controls.Add(this.chkDebugMode);
            this.Controls.Add(this.txtRegisters);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.txtReadFifo);
            this.Controls.Add(this.label7);
            this.Controls.Add(this.txtVSync);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.txtPath3);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.txtPath2);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.txtPath1);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.txtGIFPackets);
            this.Controls.Add(this.lblGIFPackets);
            this.Controls.Add(this.txtDumpSize);
            this.Controls.Add(this.lblDumpSize);
            this.Controls.Add(this.lstProcesses);
            this.Controls.Add(this.lblChild);
            this.Controls.Add(this.lblDebugger);
            this.Controls.Add(this.lblInternalLog);
            this.Controls.Add(this.txtIntLog);
            this.Controls.Add(this.rdaNone);
            this.Controls.Add(this.lblOverride);
            this.Controls.Add(this.rdaOGLSW);
            this.Controls.Add(this.rdaDX1011SW);
            this.Controls.Add(this.rdaDX9SW);
            this.Controls.Add(this.rdaOGLHW);
            this.Controls.Add(this.rdaDX1011HW);
            this.Controls.Add(this.rdaDX9HW);
            this.Controls.Add(this.lstGSDX);
            this.Controls.Add(this.pctBox);
            this.Controls.Add(this.cmdOpenIni);
            this.Controls.Add(this.lblLog);
            this.Controls.Add(this.txtLog);
            this.Controls.Add(this.cmdConfigGSDX);
            this.Controls.Add(this.cmdRun);
            this.Controls.Add(this.GsdxList);
            this.Controls.Add(this.lblDumps);
            this.Controls.Add(this.lstDumps);
            this.Controls.Add(this.cmdBrowseDumps);
            this.Controls.Add(this.lblDumpDirectory);
            this.Controls.Add(this.txtDumpsDirectory);
            this.Controls.Add(this.cmdBrowseGSDX);
            this.Controls.Add(this.lblDirectory);
            this.Controls.Add(this.txtGSDXDirectory);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.KeyPreview = true;
            this.MaximizeBox = false;
            this.Name = "GSDumpGUI";
            this.Text = "GSDumpGUI";
            this.Load += new System.EventHandler(this.GSDumpGUI_Load);
            this.KeyDown += new System.Windows.Forms.KeyEventHandler(this.GSDumpGUI_KeyDown);
            ((System.ComponentModel.ISupportInitialize)(this.pctBox)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TextBox txtGSDXDirectory;
        private System.Windows.Forms.Label lblDirectory;
        private System.Windows.Forms.Button cmdBrowseGSDX;
        private System.Windows.Forms.Button cmdBrowseDumps;
        private System.Windows.Forms.Label lblDumpDirectory;
        private System.Windows.Forms.TextBox txtDumpsDirectory;
        private System.Windows.Forms.ListBox lstGSDX;
        private System.Windows.Forms.ListBox lstDumps;
        private System.Windows.Forms.Label lblDumps;
        private System.Windows.Forms.Label GsdxList;
        private System.Windows.Forms.Button cmdRun;
        private System.Windows.Forms.Button cmdConfigGSDX;
        private System.Windows.Forms.TextBox txtLog;
        private System.Windows.Forms.Label lblLog;
        private System.Windows.Forms.Button cmdOpenIni;
        private System.Windows.Forms.PictureBox pctBox;
        private System.Windows.Forms.RadioButton rdaDX9HW;
        private System.Windows.Forms.RadioButton rdaDX1011HW;
        private System.Windows.Forms.RadioButton rdaOGLHW;
        private System.Windows.Forms.RadioButton rdaDX9SW;
        private System.Windows.Forms.RadioButton rdaDX1011SW;
        private System.Windows.Forms.RadioButton rdaOGLSW;
        private System.Windows.Forms.Label lblOverride;
        private System.Windows.Forms.RadioButton rdaNone;
        private System.Windows.Forms.Label lblInternalLog;
        private System.Windows.Forms.TextBox txtIntLog;
        private System.Windows.Forms.Label lblDebugger;
        private System.Windows.Forms.Label lblChild;
        public System.Windows.Forms.ListBox lstProcesses;
        private System.Windows.Forms.Label lblDumpSize;
        public System.Windows.Forms.Label txtDumpSize;
        public System.Windows.Forms.Label txtGIFPackets;
        private System.Windows.Forms.Label lblGIFPackets;
        public System.Windows.Forms.Label txtPath1;
        private System.Windows.Forms.Label label2;
        public System.Windows.Forms.Label txtPath2;
        private System.Windows.Forms.Label label3;
        public System.Windows.Forms.Label txtPath3;
        private System.Windows.Forms.Label label5;
        public System.Windows.Forms.Label txtVSync;
        private System.Windows.Forms.Label label4;
        public System.Windows.Forms.Label txtReadFifo;
        private System.Windows.Forms.Label label7;
        public System.Windows.Forms.Label txtRegisters;
        private System.Windows.Forms.Label label6;
        public System.Windows.Forms.CheckBox chkDebugMode;
        public System.Windows.Forms.TreeView treTreeView;
        public System.Windows.Forms.Label lblGif;
        public System.Windows.Forms.Button btnStep;
        public System.Windows.Forms.Button btnRunToSelection;
        public System.Windows.Forms.Button cmdGoToStart;
        public System.Windows.Forms.Button cmdGoToNextVSync;
        public System.Windows.Forms.Label txtGifPacketSize;
        private System.Windows.Forms.Label lblGIFPacketSize;
        public System.Windows.Forms.TreeView treeGifPacketContent;
        public System.Windows.Forms.Label lblContent;
    }
}

