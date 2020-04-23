using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Net;

namespace pcsx2_Updater
{
    abstract class UpdateChecker
    {
        private const string regexcurrentversion = @"v([\d.]*-dev-[\d]*)";
        protected Update Current = new Update()
        {
            Patch = Config.InstalledPatchName,
            Version = Config.InstalledPatchName, // Assumed to be a Stable build number by default
            DateTime = Config.InstalledPatchDate
        };
        public List<Update> Updates;
        public UpdateChecker()
        {
            Regex re = new Regex(regexcurrentversion);
            Match result = re.Match(Config.InstalledPatchName);
            if (result.Success)
            {
                Current.Version = result.Groups[0].Value;
            }

            Updates = GetNewVersions();

            switch (Config.Channel)
            {
                case Channels.Development:
                    // Keep stable releases listed in the Development channel
                    break;
                case Channels.Stable:
                    Updates = Updates.Where(update => update.IsRelease).ToList();
                    break;
            }

            if (Updates.Count > 0)
            {
                Current.Description = "Currently Installed";
                Updates.Add(Current);
                Application.Run(new frmMain(Updates));
            }
            else
            {
                if (!Config.Silent) MessageBox.Show("No new updates found!");
                Console.WriteLine("No new updates found!");
            }
        }
        protected abstract List<Update> GetNewVersions();
        public static string Get(string Url)
        {
            HttpWebRequest request = (HttpWebRequest)WebRequest.Create(Url);
            request.UserAgent = "request";
            var response = (HttpWebResponse)request.GetResponse();
            if (response.StatusCode == HttpStatusCode.OK)
            {
                Stream receiveStream = response.GetResponseStream();
                StreamReader readStream = null;

                if (response.CharacterSet == null)
                {
                    readStream = new StreamReader(receiveStream);
                }
                else
                {
                    readStream = new StreamReader(receiveStream, Encoding.GetEncoding(response.CharacterSet));
                }

                string data = readStream.ReadToEnd();

                response.Close();
                readStream.Close();

                return data;
            }
            MessageBox.Show("Failed to check for updates!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return "";
        }
    }
    public class Update
    {
        public string Patch;
        public string Version;
        public bool IsRelease;
        public string Author;
        public DateTime DateTime;
        public string DownloadUrl;
        public string FileType;
        public string Description;
    }
}
