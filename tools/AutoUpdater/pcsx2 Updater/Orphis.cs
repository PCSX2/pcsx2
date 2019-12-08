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
        // Complex and very specific regex, works great as long as Orphis' site doesn't change.
        private const string regex = @"<tr>\s*<td><a href='https:\/\/github.com\/PCSX2\/pcsx2\/commit\/.*'>(v([\d.]*-dev-[\d]*)-g[0-9a-f]{9})<\/a><\/td>\s{5}<td>([\w\s]*)<\/td>\s*<td>([0-9-: ]{19})<\/td>\s*<td><a href='([0-9a-z\/.?=&;-]*)' onclick='[\w"":., ();-]*'>Download<\/a><\/td>\s*<td class=""svnlog"">(.*)<\/td>\s*<\/tr>";
        // https://regex101.com/r/px5x7z/1
        /*  GROUPS
         *  1: Patch
         *  2: Version
         *  3: Author
         *  4: DateTime
         *  5: Download link
         *  6: Patch notes
         */
        protected override void GetNewVersions()
        {
            string html = Get(url);

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
                        DownloadUrl = "https://buildbot.orphis.net" + match.Groups[5].Value.Replace("&amp;","&"), // Some error on Orphis' site breaks the download urls. Browsers autocorrect it.
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
            }
        }
    }
}
