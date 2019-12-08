using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace pcsx2_Updater
{
    public partial class frmMain : Form
    {
        private List<Update> updates;
        public frmMain(Update Current, List<Update> New)
        {
            InitializeComponent();
            labelVersion.Text = "Installed build: " + Current.Patch + "\rLatest build: " + New[0].Patch;
            updates = New;
        }

        private void frmMain_Load(object sender, EventArgs e)
        {
            tableUpdates.AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.AllCells;
            tableUpdates.Columns[3].DefaultCellStyle.WrapMode = DataGridViewTriState.True;
            foreach(Update update in updates)
            {
                tableUpdates.Rows.Add(update.Patch, update.Author, update.DateTime, update.Description);
            }

            cbChannel.SelectedItem = cbChannel.Items[cbChannel.Items.IndexOf(Config.Channel.ToString())];

            checkEnabled.Checked = Config.Enabled;

            switch (Config.Channel)
            {
                case Channels.Development:
                    labelUpdateIntro.Text = "A new development build of PCSX2 has been released, all the changes are listed below.";
                    break;
                case Channels.Stable:
                    labelUpdateIntro.Text = "A new release of PCSX2 is available now! The most notable changes are listed below.";
                    break;
                default:
                    labelUpdateIntro.Text = "A new update for PCSX2 is available in the " + Config.Channel.ToString() + " channel.";
                    break;
            }
        }
        private void frmMain_FormClosing(object sender, FormClosingEventArgs e)
        {

        }

        private void btnUpdate_Click(object sender, EventArgs e)
        {
            Downloader d = new Downloader(updates[0].DownloadUrl);
            d.ShowDialog();
            if(d.DialogResult == DialogResult.OK)
            {
                Config.InstalledPatchName = updates[0].Patch;
                Config.InstalledPatchDate = updates[0].DateTime;
                Config.Write();
                Close();
            }
        }

        private void CbChannel_SelectedIndexChanged(object sender, EventArgs e)
        {
            if(Config.Channel != (Channels)Enum.Parse(typeof(Channels), cbChannel.SelectedItem.ToString()))
            {
                Config.Channel = (Channels)Enum.Parse(typeof(Channels), cbChannel.SelectedItem.ToString());
                Config.Write();
                MessageBox.Show("Changed channels successfully. You'll need to check for updates again.");
                //Config.Refresh = true;
                Close();
            }
        }

        private void BtnLater_Click(object sender, EventArgs e)
        {
            Close();   
        }

        private void BtnSkip_Click(object sender, EventArgs e)
        {
            Config.SkipBuild = updates[0].DateTime;
            Config.Write();
            Close();
        }

        private void CheckEnabled_CheckedChanged(object sender, EventArgs e)
        {
            if(Config.Enabled != checkEnabled.Checked)
            {
                Config.Enabled = checkEnabled.Checked;
                Config.Write();
            }
        }
    }

}
