using System;
using System.IO;
using System.IO.Compression;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Diagnostics;
using System.Net;
using System.Windows.Forms;

namespace pcsx2_Updater
{
    public partial class Downloader : Form
    {
        private string tempPath;
        private string fileType;
        public Downloader(Update update)
        {
            // Updater is created

            // Provides a default response of Cancel assuming something fails
            DialogResult = DialogResult.Cancel;

            // Typically %UserProfile%\AppData\Local\Temp
            tempPath = Path.GetTempPath();

            fileType = update.FileType;

            InitializeComponent();

            if(Directory.Exists(tempPath + @"pcsx2_update"))
                Directory.Delete(tempPath + @"pcsx2_update", true);

            using (WebClient wc = new WebClient())
            {
                wc.DownloadProgressChanged += wc_DownloadProgressChanged;
                wc.DownloadFileCompleted += wc_DownloadFileCompleted;
                wc.DownloadFileAsync(new System.Uri(update.DownloadUrl), tempPath + @"pcsx2_update." + fileType);
            }
        }
        // Event to track the progress
        void wc_DownloadProgressChanged(object sender, DownloadProgressChangedEventArgs e)
        {
            labelstatus.Text = "Downloading pcsx2_update." + fileType + "...";
            this.Text = "Downloading... " + e.ProgressPercentage.ToString() + "%";
            progressBar.Value = e.ProgressPercentage;
        }

        void wc_DownloadFileCompleted(object sender, AsyncCompletedEventArgs e)
        {
            labelstatus.Text = "Extracting pcsx2_update."+fileType+"...";
            this.Text = "Extracting...";
            progressBar.Style = ProgressBarStyle.Marquee;
            
            switch (fileType)
            {
                case "7z":
                    sevenzipExtractor();
                    break;
                case "zip":
                    zipExtractor();
                    break;
                default:
                    MessageBox.Show("Extracting update failed!\nThere's no known handler for extracting the provided format!", "Failed to update pcsx2", MessageBoxButtons.OK);
                    Close();
                    break;
            }

            this.Text = "Installing update";
            labelstatus.Text = "Moving extracted files to application folder...";
            Console.WriteLine("Installing update to " + AppDomain.CurrentDomain.BaseDirectory + ".");

            if(File.Exists(tempPath + @"pcsx2_update\AutoUpdate.ini")) // Update existing config, don't overwrite it.
                File.Delete(tempPath + @"pcsx2_update\AutoUpdate.ini");

            string origin = tempPath + @"pcsx2_update\";
            string target = AppDomain.CurrentDomain.BaseDirectory;
            DirectoryInfo dirInfo = new DirectoryInfo(target);
            List<String> updateFiles = Directory.GetFiles(origin, "*.*", SearchOption.AllDirectories).ToList();

            foreach (string file in updateFiles)
            {
                FileInfo newFile = new FileInfo(file);
                FileInfo oldFile = new FileInfo(dirInfo + "\\" + newFile.FullName.Substring(origin.Length));
                // to remove name collisions
                if (!oldFile.Directory.Exists)
                    oldFile.Directory.Create();
                else if (oldFile.Exists)
                    oldFile.Delete();
                newFile.MoveTo(dirInfo + "\\" + newFile.FullName.Substring(origin.Length));
            }

            /*
            foreach (string dir in Directory.EnumerateDirectories(tempPath + @"pcsx2_update\"))
            {
                FileSystem.MoveDirectory(dir, AppDomain.CurrentDomain.BaseDirectory, true);
            }
            foreach (string file in Directory.EnumerateFiles(tempPath + @"pcsx2_update\"))
            {
                FileSystem.MoveFile(file, AppDomain.CurrentDomain.BaseDirectory, true);
            }
            */

            MessageBox.Show("Installed successfully!", "pcsx2 Updater", MessageBoxButtons.OK, MessageBoxIcon.Information);
            Console.WriteLine("PCSX2 has been updated successfully!");
            DialogResult = DialogResult.OK;
            Close();
        }

        void sevenzipExtractor()
        {
            try
            {
                // TODO: Use existing xz/lzma repo to extract 7z files.
                ProcessStartInfo pro = new ProcessStartInfo();
                pro.WindowStyle = ProcessWindowStyle.Hidden;
                pro.FileName = @"7za.exe";
                pro.Arguments = string.Format("x \"{0}\" -y -o\"{1}\"", tempPath + @"pcsx2_update." + fileType, tempPath + @"pcsx2_update\");
                Process x = Process.Start(pro);
                x.WaitForExit();
            }
            catch (Exception Ex)
            {
                MessageBox.Show("Extracting update failed!\nThe update appears to be corrupted.\n\n" + Ex.ToString(), "Failed to update pcsx2", MessageBoxButtons.OK);
                Console.WriteLine("Failed to extract archive! " + Ex.ToString());
                Close();
            }
        }
        void zipExtractor()
        {
            try
            {
                Directory.CreateDirectory(tempPath + @"\pcsx2_update\");
                ZipFile.ExtractToDirectory(tempPath + @"pcsx2_update." + fileType, tempPath + @"pcsx2_update\");
            }
            catch(Exception e)
            {
                MessageBox.Show("Extracting update failed!\nSomething went wrong while extracting the update.\r\r"+e.ToString(), "Failed to update pcsx2", MessageBoxButtons.OK);
                Console.WriteLine("Failed to extract archive! " + e.ToString());
                Close();
            }
        }

        private void btnCancel_Click(object sender, EventArgs e)
        {
            Close();
        }
    }
}
