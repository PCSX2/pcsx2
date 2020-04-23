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
        public frmMain(List<Update> New)
        {
            InitializeComponent();
            updates = New;
        }

        private void frmMain_Load(object sender, EventArgs e)
        {
            labelCurrentBuild.Text = "Current build: " + updates.Last().Patch;
            labelLatestBuild.Text = "Latest build: " + updates[0].Patch;

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

            tableUpdates_SelectionChanged(sender, e);
        }
        private void frmMain_FormClosing(object sender, FormClosingEventArgs e)
        {

        }

        private void btnUpdate_Click(object sender, EventArgs e)
        {
            var selection = updates[tableUpdates.CurrentRow.Index];

            if(selection.DownloadUrl == "")
            {
                MessageBox.Show("The download link for this particular build has expired! Please pick another one.");
                return;
            }

            Downloader d = new Downloader(selection);
            d.ShowDialog();
            if(d.DialogResult == DialogResult.OK)
            {
                Config.InstalledPatchName = selection.Patch;
                Config.InstalledPatchDate = selection.DateTime;
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
            Config.SkipBuild = updates[tableUpdates.CurrentRow.Index].DateTime;
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

        private void tableUpdates_SelectionChanged(object sender, EventArgs e)
        {
            if(tableUpdates.CurrentRow.Index == tableUpdates.Rows.GetLastRow(DataGridViewElementStates.Visible))
            {
                btnUpdate.Enabled = false;
            }
            else
            {
                btnUpdate.Enabled = true;
            }
        }
    }

}
