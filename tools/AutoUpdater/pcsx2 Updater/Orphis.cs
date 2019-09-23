using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Net;
using System.Net.Http;

namespace pcsx2_Updater
{
    class Orphis : UpdateChecker
    {
        private const string url = "https://buildbot.orphis.net/pcsx2/index.php";
        private const string findgit = @"<td><a href='https:\/\/github.com\/PCSX2\/pcsx2\/commit\/.*'>([A-z0-9.-]{26})<\/a><\/td>";
        private const string findver = @"v([0-9].[0-9].[0-9]-dev-[0-9]*)";
        private const string downloadtemplate = "https://buildbot.orphis.net/pcsx2/index.php?m=dl&rev={0}&platform=windows-x86";
        protected override void Check()
        {
            string txtcurrent = pcsx2_Updater.Properties.Resources.CurrentCommit;

            string html = Get(url);

            Regex re = new Regex(findgit);
            Match relatestgit = re.Match(html);
            if (relatestgit.Success)
            {
                re = new Regex(findver);
                Match recurrent = re.Match(txtcurrent);
                Match relatest = re.Match(relatestgit.Groups[1].Value);
                if(recurrent.Success & relatest.Success)
                {
                    int current = int.Parse(new string(recurrent.Groups[1].Value.Where(c => char.IsDigit(c)).ToArray()));
                    int latest = int.Parse(new string(relatest.Groups[1].Value.Where(c => char.IsDigit(c)).ToArray()));

                    if (latest > current)
                    {
                        Application.Run(new frmMain(txtcurrent,relatestgit.Groups[1].Value,downloadtemplate));
                    }
                    else
                    {
                        Console.WriteLine("Newer version not found.");
                    }
                }
                else
                {
                    Console.WriteLine("Unable to parse version information!");
                }
            }
            else
            {
                Console.WriteLine("Unable to parse '"+url+"'!");
            }
        }
        string Get(string Url)
        {
            HttpWebRequest request = (HttpWebRequest)WebRequest.Create(Url);
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
            MessageBox.Show("Failed to check for updates!","Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
            return "";
        }
    }
}
