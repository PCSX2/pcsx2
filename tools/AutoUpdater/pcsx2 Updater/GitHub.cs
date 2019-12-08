using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Newtonsoft.Json;

namespace pcsx2_Updater
{
    class GitHub : UpdateChecker
    {
        private const string url = "https://api.github.com/repos/PCSX2/pcsx2/tags";
        // Major PCSX2 releases are released 
        protected override void GetNewVersions()
        {
            /*string html = Get(url);

            Regex re = new Regex(regex, RegexOptions.Compiled);
            MatchCollection regexresult = re.Matches(html);
            if (regexresult.Count > 0)
            {
                int current = int.Parse(new string(Current.Version.Where(c => char.IsDigit(c)).ToArray()));

                foreach (Match match in regexresult)
                {
                    var newupdate = new Update
                    {
                        Patch = match.Groups[1].Value,
                        Version = match.Groups[2].Value,
                        Author = match.Groups[3].Value,
                        DateTime = Convert.ToDateTime(match.Groups[4].Value),
                        DownloadUrl = "https://buildbot.orphis.net" + match.Groups[5].Value,
                        Description = match.Groups[6].Value
                    };
                    var latest = int.Parse(new string(newupdate.Version.Where(c => char.IsDigit(c)).ToArray()));
                    if (latest > current && newupdate.DateTime > Current.DateTime && newupdate.DateTime > Config.SkipBuild)
                    {
                        Updates.Add(newupdate);
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                Console.WriteLine("Unable to parse '"+url+"'!");
            }*/

            // Currently in the process of determining how the PCSX2 team plan on releasing future releases.
            // Ideally, they'd release them with a zip download on GitHub. Otherwise we'd have a simple API on their site.

            return;
        }
    }
}
