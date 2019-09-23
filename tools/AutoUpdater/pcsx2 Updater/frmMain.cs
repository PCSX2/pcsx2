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
        private string downloadurl;
        public frmMain(string Current, string Latest, string DownloadTemplate)
        {
            downloadurl = String.Format(DownloadTemplate,Latest);
            InitializeComponent();
            labelVersion.Text = "Installed build: " + Current + "\rLatest build: " + Latest;
        }

        private void frmMain_Load(object sender, EventArgs e)
        {

        }
        private void frmMain_FormClosing(object sender, FormClosingEventArgs e)
        {

        }

        private void Button1_Click(object sender, EventArgs e)
        {
            Downloader d = new Downloader(downloadurl);
            d.ShowDialog();
        }
    }

}
