using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using pcsx2_Updater;

namespace pcsx2_Updater
{
    class GitHub : UpdateChecker
    {
        private const string url = "https://api.github.com/repos/yiays/pcsx2/releases";
        //private const string verfile = "https://github.com/yiays/pcsx2/raw/{0}/pcsx2/SysForwardDefs.h";
        //private const string verregex = @".*PCSX2_VersionHi\s*=\s*(\d);\n.*PCSX2_VersionMid\s*=\s*(\d);\n.*PCSX2_VersionLo\s*=\s*(\d);\n.*PCSX2_isReleaseVersion\s*=\s*(\d);";
        // Regex to get canonical version for this commit in SysForwardDefs.h
        // https://regex101.com/r/SwOmqj/1
        /* GROUPS
         * 1: (int) Major Version
         * 2: (int) Minor Version
         * 3: (int) Patch Version
         * 4: (bool) Is a Release Version
         */

        //private const string vertext_release = "{0}.{1}.{2}";
        //private const string vertext_dev = "{0}.{1}.{2}-dev-{3}";

        protected override List<Update> GetNewVersions()
        {
            string json = Get(url);

            var releases = JArray.Parse(json);

            return releases.Select(release =>
            {
                var author = release["author"]["login"].ToString();
                var datetime = Convert.ToDateTime(release["published_at"]);
                var description = release["body"].ToString();

                var desclist = description.Replace("\r", "").Split('\n').ToList();
                if(desclist.Count() >= 3)
                {
                    //try
                    //{
                        author = desclist[0];
                        datetime = DateTime.ParseExact(desclist[1], "ddd MMM dd HH:mm:ss yyyy zzz", new CultureInfo("en-US"));
                        description = string.Join("\n", desclist.GetRange(2, desclist.Count()-2));
                    //}
                    //catch {
                       // Console.WriteLine("Unable to extract metadata from release #"+release["id"].ToString());
                    //}
                }

                var downloadurl = "";
                var downloadurls = release["assets"].Where(releasefile => releasefile["name"].ToString().Contains("-windows-x86.zip"))
                                                .Select(releasefile => releasefile["browser_download_url"].ToString()).ToList();

                if (downloadurls.Count() > 0)
                {
                    downloadurl = downloadurls[0];
                    Console.WriteLine(downloadurls);
                }

                return new Update()
                {
                    Patch = release["tag_name"].ToString(),
                    Version = release["tag_name"].ToString(),
                    IsRelease = !bool.Parse(release["prerelease"].ToString()),
                    Author = author,
                    DateTime = datetime,
                    DownloadUrl = downloadurl,
                    FileType = "zip",
                    Description = description
                };
            }).Where(update => update.DateTime > Current.DateTime).ToList();
        }
    }
}