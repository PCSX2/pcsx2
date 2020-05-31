/*
 * Copyright (C) 2009-2011 Ferreri Alessio
 * Copyright (C) 2009-2018 PCSX2 Dev Team
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.ComponentModel;
using System.Drawing;
using System.Windows.Forms;
using System.IO;
using System.Diagnostics;
using System.Linq;
using System.Linq.Expressions;
using GSDumpGUI.Forms.Entities;
using GSDumpGUI.Forms.Helper;
using GSDumpGUI.Forms.SettingsProvider;
using GSDumpGUI.Properties;
using TCPLibrary.MessageBased.Core;

namespace GSDumpGUI
{
    public partial class GSDumpGUI : Form
    {
        private readonly ILogger _internalLogger;
        private readonly ILogger _gsdxLogger;
        private readonly IGsdxDllFinder _gsdxDllFinder;
        private readonly IGsDumpFinder _gsDumpFinder;
        private readonly IFolderWithFallBackFinder _folderWithFallBackFinder;

        public List<Process> Processes;

        private Int32 _selected;
        public Int32 SelectedRad
        {
            get { return _selected; }
            set
            {
                if (value > 6)
                    value = 0;
                _selected = value;
                switch (_selected)
                {
                    case 0:
                        rdaNone.Checked = true;
                        break;
                    case 1:
                        rdaDX9HW.Checked = true;
                        break;
                    case 2:
                        rdaDX1011HW.Checked = true;
                        break;
                    case 3:
                        rdaOGLHW.Checked = true;
                        break;
                    case 4:
                        rdaDX9SW.Checked = true;
                        break;
                    case 5:
                        rdaDX1011SW.Checked = true;
                        break;
                    case 6:
                        rdaOGLSW.Checked = true;
                        break;
                }
            }
        }

        private readonly Bitmap NoImage;

        private Settings Settings => Settings.Default;

        private readonly GsDumps _availableGsDumps;
        private readonly GsDlls _availableGsDlls;

        private List<FileSystemWatcher> _dllWatcher;
        private List<FileSystemWatcher> _dumpWatcher;

        enum FileChangeEvt { Dll = 1, Dump = 2 };
        private ConcurrentQueue<FileChangeEvt> _watcherEvents;
        private System.Windows.Forms.Timer _fileChangesWatchdog;

        private string _gsdxPathOld, _dumpPathOld;

        public GSDumpGUI()
        {
            PortableXmlSettingsProvider.ApplyProvider(Settings);

            InitializeComponent();
            _internalLogger = new RichTextBoxLogger(txtIntLog);
            _gsdxLogger = new RichTextBoxLogger(txtLog);
            _gsdxDllFinder = new GsdxDllFinder(_internalLogger);
            _gsDumpFinder = new GsDumpFinder(_internalLogger);
            _folderWithFallBackFinder = new FolderWithFallBackFinder();
            _availableGsDumps = new GsDumps();
            _availableGsDlls = new GsDlls();

            _availableGsDumps.OnIndexUpdatedEvent += UpdatePreviewImage;

            this.Text += Environment.Is64BitProcess ? " 64bits" : " 32bits";

            if (String.IsNullOrEmpty(Settings.GSDXDir) || !Directory.Exists(Settings.GSDXDir))
                Settings.GSDXDir = AppDomain.CurrentDomain.BaseDirectory;

            if (String.IsNullOrEmpty(Settings.DumpDir) || !Directory.Exists(Settings.DumpDir))
                Settings.DumpDir = AppDomain.CurrentDomain.BaseDirectory;

            txtGSDXDirectory.Text = Settings.GSDXDir;
            txtDumpsDirectory.Text = Settings.DumpDir;

            BindListControl(lstDumps, _availableGsDumps, g => g.Files, f => f.DisplayText, g => g.SelectedFileIndex);
            BindListControl(lstGSDX, _availableGsDlls, g => g.Files, f => f.DisplayText, g => g.SelectedFileIndex);

            Processes = new List<Process>();

            NoImage = CreateDefaultImage();

            _dllWatcher = new List<FileSystemWatcher>();
            _dumpWatcher = new List<FileSystemWatcher>();
            _watcherEvents = new ConcurrentQueue<FileChangeEvt>();

            _fileChangesWatchdog = new System.Windows.Forms.Timer();
            _fileChangesWatchdog.Tick += new EventHandler(FileChangesWatchdog);
            _fileChangesWatchdog.Interval = 1000;
            _fileChangesWatchdog.Start();
        }

        private void DisposeExtra()
        {
            foreach (FileSystemWatcher w in _dllWatcher)
            {
                w.EnableRaisingEvents = false;
                w.Dispose();
            }

            foreach (FileSystemWatcher w in _dumpWatcher)
            {
                w.EnableRaisingEvents = false;
                w.Dispose();
            }

            _dllWatcher.Clear();
            _dumpWatcher.Clear();

            _fileChangesWatchdog.Stop();
            _fileChangesWatchdog.Dispose();
        }

        private void FileChangesWatchdog(object source, EventArgs e)
        {
            bool dllReload = false;
            bool dumpReload = false;

            FileChangeEvt evt;
            while (_watcherEvents.TryDequeue(out evt))
            {
                if (evt == FileChangeEvt.Dll) dllReload = true;
                else if (evt == FileChangeEvt.Dump) dumpReload = true;
            }

            if (dllReload) ReloadGsdxDlls();
            if (dumpReload) ReloadGsdxDumps();
        }

        private void OnDllDirChange(object source, FileSystemEventArgs e)
        {
            _watcherEvents.Enqueue(FileChangeEvt.Dll);
        }

        private void OnDumpDirChange(object source, FileSystemEventArgs e)
        {
            _watcherEvents.Enqueue(FileChangeEvt.Dump);
        }

        private static void BindListControl<TModel, TElement>(ListControl lb, TModel model, Func<TModel, BindingList<TElement>> collectionAccessor, Expression<Func<TElement, string>> displayTextAccessor, Expression<Func<TModel, int>> selectedIndexAccessor)
        {
            lb.DataSource = new BindingSource
            {
                    DataSource = collectionAccessor(model)
            };
            lb.DisplayMember = ((MemberExpression)displayTextAccessor.Body).Member.Name;
            lb.DataBindings.Add(nameof(lb.SelectedIndex), model, ((MemberExpression)selectedIndexAccessor.Body).Member.Name, false, DataSourceUpdateMode.OnPropertyChanged);
        }

        private static Bitmap CreateDefaultImage()
        {
            var defaultImage = new Bitmap(320, 240, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(defaultImage))
            {
                g.FillRectangle(new SolidBrush(Color.Black), new Rectangle(0, 0, 320, 240));
                g.DrawString("No Image", new Font(FontFamily.GenericSansSerif, 48, FontStyle.Regular), new SolidBrush(Color.White), new PointF(0, 70));
            }

            return defaultImage;
        }

        private void ReloadGsdxDlls()
        {
            _internalLogger.Information("Starting GSdx Loading Procedures");

            var gsdxFolder = _folderWithFallBackFinder.GetViaPatternWithFallback(Settings.GSDXDir, "*.dll", "", "plugins", "dll", "dlls");
            _availableGsDlls.Files.Clear();
            foreach (var file in _gsdxDllFinder.GetEnrichedPathToValidGsdxDlls(gsdxFolder))
                _availableGsDlls.Files.Add(file);

            Settings.GSDXDir = gsdxFolder.FullName;
            _internalLogger.Information("Completed GSdx Loading Procedures");

            string[] paths = { "", "\\plugins", "\\dll", "\\dlls" };

            foreach (FileSystemWatcher w in _dllWatcher)
            {
                w.EnableRaisingEvents = false;
                w.Dispose();
            }

            _dllWatcher.Clear();

            for (int i = 0; i < paths.Length; i++)
            {
                try
                {
                    FileSystemWatcher w = new FileSystemWatcher(Settings.GSDXDir + paths[i], "*.dll");
                    //w.Changed += OnDllDirChange;
                    w.Created += OnDllDirChange;
                    w.Deleted += OnDllDirChange;
                    w.Renamed += OnDllDirChange;
                    w.EnableRaisingEvents = true;
                    _dllWatcher.Add(w);
                }
                catch { }
            }
        }

        private void ReloadGsdxDumps()
        {
            _internalLogger.Information("Starting GSdx Dump Loading Procedures...");

            var dumpFolder = _folderWithFallBackFinder.GetViaPatternWithFallback(Settings.DumpDir, "*.gs", "", "dumps", "gsdumps");

            _availableGsDumps.Files.Clear();
            foreach (var file in _gsDumpFinder.GetValidGsdxDumps(dumpFolder))
                _availableGsDumps.Files.Add(file);

            Settings.DumpDir = dumpFolder.FullName;
            _internalLogger.Information("...Completed GSdx Dump Loading Procedures");

            string[] paths = { "", "\\dumps", "\\gsdumps" };

            foreach (FileSystemWatcher w in _dumpWatcher)
            {
                w.EnableRaisingEvents = false;
                w.Dispose();
            }

            _dumpWatcher.Clear();

            for (int i = 0; i < paths.Length; i++)
            {
                try
                {
                    FileSystemWatcher w = new FileSystemWatcher(Settings.DumpDir + paths[i], "*.gs");
                    //w.Changed += OnDumpDirChange;
                    w.Created += OnDumpDirChange;
                    w.Deleted += OnDumpDirChange;
                    w.Renamed += OnDumpDirChange;
                    w.EnableRaisingEvents = true;
                    _dumpWatcher.Add(w);
                }
                catch { }
            }
        }

        private void GSDumpGUI_Load(object sender, EventArgs e)
        {
            ReloadGsdxDlls();
            ReloadGsdxDumps();

            // Auto select GS dump and GSdx dll
            _availableGsDumps.Selected = _availableGsDumps.Files.FirstOrDefault();
            _availableGsDlls.Selected = _availableGsDlls.Files.FirstOrDefault();
        }

        private void cmdBrowseGSDX_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog();
            ofd.ValidateNames = false;
            ofd.CheckFileExists = false;
            ofd.CheckPathExists = true;
            ofd.InitialDirectory = Settings.GSDXDir;
            ofd.FileName = "Select Folder";

            if(ofd.ShowDialog() == DialogResult.OK)
            {
                string newpath = Path.GetDirectoryName(ofd.FileName);
                if (!Settings.GSDXDir.Equals(newpath, StringComparison.OrdinalIgnoreCase))
                {
                    txtGSDXDirectory.Text = newpath;
                    Settings.GSDXDir = newpath;
                    Settings.Save();
                    ReloadGsdxDlls();
                    _availableGsDlls.Selected = _availableGsDlls.Files.FirstOrDefault();
                }
            }
        }

        private void cmdBrowseDumps_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog();
            ofd.ValidateNames = false;
            ofd.CheckFileExists = false;
            ofd.CheckPathExists = true;
            ofd.InitialDirectory = Settings.DumpDir;
            ofd.FileName = "Select Folder";

            if (ofd.ShowDialog() == DialogResult.OK)
            {
                string newpath = Path.GetDirectoryName(ofd.FileName);
                if (!Settings.DumpDir.Equals(newpath, StringComparison.OrdinalIgnoreCase))
                {
                    txtDumpsDirectory.Text = newpath;
                    Settings.DumpDir = newpath;
                    Settings.Save();
                    ReloadGsdxDumps();
                    _availableGsDumps.Selected = _availableGsDumps.Files.FirstOrDefault();
                }
            }
        }

        private void cmdRun_Click(object sender, EventArgs e)
        {
            // Execute the GSReplay function
            if (!_availableGsDumps.IsSelected)
            {
                MessageBox.Show("Select your Dump first", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            if (!_availableGsDlls.IsSelected)
            {
                MessageBox.Show("Select your GSdx first", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            ExecuteFunction("GSReplay");
        }

        private void ExecuteFunction(String Function)
        {
            txtLog.Text = "";

            CreateDirs();

            // Set the Arguments to pass to the child
            String SelectedRenderer = "";
            switch (SelectedRad)
            {
                case 0:
                    SelectedRenderer = "-1";
                    break;
                case 1:
                    SelectedRenderer = "0";
                    break;
                case 2:
                    SelectedRenderer = "3";
                    break;
                case 3:
                    SelectedRenderer = "12";
                    break;
                case 4:
                    SelectedRenderer = "1";
                    break;
                case 5:
                    SelectedRenderer = "4";
                    break;
                case 6:
                    SelectedRenderer = "13";
                    break;
            }

            if (SelectedRenderer != "-1")
            {
                String GSdxIniPath = AppDomain.CurrentDomain.BaseDirectory + "GSDumpGSDXConfigs\\inis\\gsdx.ini";
                NativeMethods.WritePrivateProfileString("Settings", "Renderer", SelectedRenderer, GSdxIniPath);
            }
            var port = Program.Server.Port;

            // dll path is mandatory for the two operations GSReplay and GSconfigure but dumpPath only for GSReplay
            var dllPath = _availableGsDlls.Selected.File.FullName;
            var dumpPath = _availableGsDumps.Selected?.File?.FullName;
            if (string.IsNullOrWhiteSpace(dumpPath) && "GSReplay".Equals(Function))
                throw new ArgumentException("You need to specify a dump path in case you want to replay a GsDump.", nameof(dumpPath));

            _gsdxLogger.Information("Start new gsdx instance");
            _gsdxLogger.Information($"\tdll: {dllPath}");
            _gsdxLogger.Information($"\tdump: {dumpPath}");

            // Start the child and link the events.
            ProcessStartInfo psi = new ProcessStartInfo();
            psi.UseShellExecute = false;
            psi.RedirectStandardOutput = true;
            psi.RedirectStandardError = true;
            psi.CreateNoWindow = true;
            psi.FileName = Process.GetCurrentProcess().ProcessName;
            psi.Arguments = "\"" + dllPath + "\"" + " \"" + dumpPath + "\"" + " \"" + Function + "\"" + " " + SelectedRenderer + " " + port;
            Process p = Process.Start(psi);
            p.OutputDataReceived += new DataReceivedEventHandler(p_StdOutDataReceived);
            p.ErrorDataReceived += new DataReceivedEventHandler(p_StdErrDataReceived);
            p.BeginOutputReadLine();
            p.BeginErrorReadLine();
            p.Exited += new EventHandler(p_Exited);
            Processes.Add(p);
        }

        private static void CreateDirs()
        {
            // Create and set the config directory.
            String Dir = AppDomain.CurrentDomain.BaseDirectory + "GSDumpGSDXConfigs\\";
            if (!Directory.Exists(Dir))
            {
                Directory.CreateDirectory(Dir);
            }
            Dir += "\\Inis\\";
            if (!Directory.Exists(Dir))
            {
                Directory.CreateDirectory(Dir);
                File.Create(Dir + "\\gsdx.ini").Close();
            }
            Dir = AppDomain.CurrentDomain.BaseDirectory + "GSDumpGSDXConfigs";
            Directory.SetCurrentDirectory(Dir);
        }

        private void p_Exited(object sender, EventArgs e)
        {
            // Remove the child if is closed
            Processes.Remove((Process)sender);
        }

        private void p_StdOutDataReceived(object sender, DataReceivedEventArgs e)
        {
            _gsdxLogger.Information(e.Data);
        }

        private void p_StdErrDataReceived(object sender, DataReceivedEventArgs e)
        {
            _gsdxLogger.Error(e.Data);
        }

        private void cmdConfigGSDX_Click(object sender, EventArgs e)
        {
            // Execute the GSconfigure function
            if (!_availableGsDlls.IsSelected)
            {
                MessageBox.Show("Select your GSdx first", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
                
            ExecuteFunction("GSconfigure");
        }

        private void cmdOpenIni_Click(object sender, EventArgs e)
        {
            // Execute the GSconfigure function
            CreateDirs();
            Process.Start(AppDomain.CurrentDomain.BaseDirectory + "GSDumpGSDXConfigs\\inis\\gsdx.ini");
        }

        private void UpdatePreviewImage(object sender, GsFiles<GsDumpFile>.SelectedIndexUpdatedEventArgs args)
        {
            if (pctBox.Image != NoImage)
                pctBox.Image?.Dispose();
            if (_availableGsDumps.Selected?.PreviewFile == null)
            {
                pctBox.Image = NoImage;
                pctBox.Cursor = Cursors.Default;
            }
            else
            {
                pctBox.Load(_availableGsDumps.Selected.PreviewFile.FullName);
                pctBox.Cursor = Cursors.Hand;
            }

            pctBox.Tag = _availableGsDumps.Selected?.PreviewFile?.FullName;
        }

        private static void PreviewImageClick(object sender, EventArgs e)
        {
            var previewControl = (PictureBox)sender;
            if (previewControl.Tag == null)
                return;
            Process.Start((string)previewControl.Tag);
        }

        private void GSDumpGUI_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Return && !txtGSDXDirectory.Focused && !txtDumpsDirectory.Focused)
                cmdRun_Click(sender, e);

            if (e.KeyCode == Keys.F1)
                cmdConfigGSDX_Click(sender, e);

            if ((e.KeyCode == Keys.F2))
                SelectedRad++;
        }

        private void rda_CheckedChanged(object sender, EventArgs e)
        {
            RadioButton itm = ((RadioButton)(sender));
            if (itm.Checked == true)
                SelectedRad = Convert.ToInt32(itm.Tag);
        }

        private void txtGSDXDirectory_Enter(object sender, EventArgs e)
        {
            _gsdxPathOld = txtGSDXDirectory.Text;
        }

        private void txtDumpsDirectory_Enter(object sender, EventArgs e)
        {
            _dumpPathOld = txtDumpsDirectory.Text;
        }

        private void txtGSDXDirectory_Leave(object sender, EventArgs e)
        {
            string newpath = txtGSDXDirectory.Text;
            if (!_gsdxPathOld.Equals(newpath, StringComparison.OrdinalIgnoreCase))
                txtGSDXDirectory.Text = _gsdxPathOld;
        }

        private void txtDumpsDirectory_Leave(object sender, EventArgs e)
        {
            string newpath = txtDumpsDirectory.Text;
            if(!_dumpPathOld.Equals(newpath, StringComparison.OrdinalIgnoreCase))
                txtDumpsDirectory.Text = _dumpPathOld;
        }

        private void txtGSDXDirectory_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Return)
            {
                string newpath = txtGSDXDirectory.Text;
                if (!String.IsNullOrEmpty(newpath) &&
                    !_gsdxPathOld.Equals(newpath, StringComparison.OrdinalIgnoreCase) &&
                    Directory.Exists(newpath))
                {
                    _gsdxPathOld = newpath;
                    Settings.GSDXDir = newpath;
                    Settings.Save();
                    ReloadGsdxDlls();
                    _availableGsDlls.Selected = _availableGsDlls.Files.FirstOrDefault();
                }
            }
        }

        private void txtDumpsDirectory_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Return)
            {
                string newpath = txtDumpsDirectory.Text;
                if (!String.IsNullOrEmpty(newpath) &&
                    !_dumpPathOld.Equals(newpath, StringComparison.OrdinalIgnoreCase) &&
                    Directory.Exists(newpath))
                {
                    _dumpPathOld = newpath;
                    Settings.DumpDir = newpath;
                    Settings.Save();
                    ReloadGsdxDumps();
                    _availableGsDumps.Selected = _availableGsDumps.Files.FirstOrDefault();
                }
            }
        }

        private void lstProcesses_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (lstProcesses.SelectedIndex != -1)
            {
                chkDebugMode.Enabled = true;

                TCPMessage msg = new TCPMessage();
                msg.MessageType = MessageType.GetDebugMode;
                msg.Parameters.Add(chkDebugMode.Checked);
                Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);

                msg = new TCPMessage();
                msg.MessageType = MessageType.SizeDump;
                Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);

                msg = new TCPMessage();
                msg.MessageType = MessageType.Statistics;
                Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);
            }
            else
            {
                chkDebugMode.Enabled = false;
            }
        }

        private void chkDebugMode_CheckedChanged(object sender, EventArgs e)
        {
            if (lstProcesses.SelectedIndex != -1)
            {
                TCPMessage msg = new TCPMessage();
                msg.MessageType = MessageType.SetDebugMode;
                msg.Parameters.Add(chkDebugMode.Checked);
                Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);
            }
        }

        private void btnStep_Click(object sender, EventArgs e)
        {
            TCPMessage msg = new TCPMessage();
            msg.MessageType = MessageType.Step;
            Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);            
        }

        private void btnRunToSelection_Click(object sender, EventArgs e)
        {
            if (treTreeView.SelectedNode != null)
            {
                TCPMessage msg = new TCPMessage();
                msg.MessageType = MessageType.RunToCursor;
                msg.Parameters.Add(Convert.ToInt32(treTreeView.SelectedNode.Text.Split(new string[]{" - "}, StringSplitOptions.None)[0]));
                Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);
            }
            else
                MessageBox.Show("You have not selected a node to jump to");
        }

        private void cmdGoToStart_Click(object sender, EventArgs e)
        {
            TCPMessage msg = new TCPMessage();
            msg.MessageType = MessageType.RunToCursor;
            msg.Parameters.Add(0);
            Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);
        }

        private void cmdGoToNextVSync_Click(object sender, EventArgs e)
        {
            TCPMessage msg = new TCPMessage();
            msg.MessageType = MessageType.RunToNextVSync;
            Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);
        }

        private void treTreeView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (treTreeView.SelectedNode != null)
            {
                TCPMessage msg = new TCPMessage();
                msg.MessageType = MessageType.PacketInfo;
                msg.Parameters.Add(Convert.ToInt32(treTreeView.SelectedNode.Text.Split(new string[] { " - " }, StringSplitOptions.None)[0]));
                Program.Clients.Find(a => a.IPAddress == lstProcesses.SelectedItem.ToString()).Send(msg);
            }
            treTreeView.SelectedNode = e.Node;
        }

        private void GSDumpGUI_FormClosing(object sender, FormClosingEventArgs e)
        {
            // Make sure all child processes are closed upon closing the main form
            Processes.ForEach(p => 
            {
                try { p.Kill(); } catch { }
                p.Dispose();
            });
        }
    }
}
