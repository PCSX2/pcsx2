using System;
using System.IO;
using System.IO.Compression;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net;
using System.Windows.Forms;

namespace pcsx2_Updater
{
    public partial class Downloader : Form
    {
        private string tempPath;
        public Downloader(string Target)
        {
            tempPath = Path.GetTempPath();

            InitializeComponent();
            using (WebClient wc = new WebClient())
            {
                wc.DownloadProgressChanged += wc_DownloadProgressChanged;
                wc.DownloadFileCompleted += wc_DownloadFileCompleted;
                wc.DownloadFileAsync(new System.Uri(Target), tempPath + @"\pcsx2_update.zip");
            }
        }
        // Event to track the progress
        void wc_DownloadProgressChanged(object sender, DownloadProgressChangedEventArgs e)
        {
            labelstatus.Text = "Downloading pcsx2_update.zip...";
            this.Text = "Downloading... " + e.ProgressPercentage.ToString() + "%";
            progressBar.Value = e.ProgressPercentage;
        }

        void wc_DownloadFileCompleted(object sender, AsyncCompletedEventArgs e)
        {
            labelstatus.Text = "Extracting pcsx2_update.zip...";
            this.Text = "Extracting...";
            progressBar.Style = ProgressBarStyle.Marquee;
            try
            {
                ZipFile.ExtractToDirectory(tempPath + @"\pcsx2_update.zip", tempPath + @"\pcsx2_update\");
            }
            catch (InvalidDataException)
            {
                MessageBox.Show("pcsx2_update.zip appears to be corrupted!","Failed to update!",MessageBoxButtons.OK,MessageBoxIcon.Error);
                Close();
                return;
            }

            this.Text = "Installing...";
            string extractfolder = Directory.GetDirectories(tempPath + @"\pcsx2_update\").Where(s => s.StartsWith("pcsx2")).First();
            File.Move(extractfolder, AppDomain.CurrentDomain.BaseDirectory);
            MessageBox.Show("Installed sucessfully!", "pcsx2 Updater", MessageBoxButtons.OK, MessageBoxIcon.Information);
            Close();
        }
    }
}
