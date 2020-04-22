using System;
using System.Collections.Generic;
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
        private const string url = "https://api.github.com/repos/yiays/pcsx2/actions/runs";
        private const string verfile = "https://github.com/yiays/pcsx2/raw/{0}/pcsx2/SysForwardDefs.h";
        private const string verregex = @".*PCSX2_VersionHi\s*=\s*(\d);\n.*PCSX2_VersionMid\s*=\s*(\d);\n.*PCSX2_VersionLo\s*=\s*(\d);\n.*PCSX2_isReleaseVersion\s*=\s*(\d);";
        // Regex to get canonical version for this commit in SysForwardDefs.h
        // https://regex101.com/r/SwOmqj/1
        /* GROUPS
         * 1: (int) Major Version
         * 2: (int) Minor Version
         * 3: (int) Patch Version
         * 4: (bool) Is a Release Version
         */

        private const string vertext_release = "{0}.{1}.{2}";
        private const string vertext_dev = "{0}.{1}.{2}-dev-{3}";

        protected override List<Update> GetNewVersions()
        {
            string json = Get(url);

            var runs = JObject.Parse(json)["workflow_runs"]
                              .Where(run => run["status"].ToString() == "completed")
                              .Take(1);

            Regex rever = new Regex(verregex, RegexOptions.Compiled);

            return runs.Select(run =>
            {
                var verstr = Get(String.Format(verfile, run["head_sha"].ToString()));
                MatchCollection regexresult = rever.Matches(verstr);

                var version = "0.0.0";
                var patch = "vunknown0";
                var isrelease = false;

                if (regexresult.Count > 0)
                {
                    if (regexresult[0].Groups[4].Value == "1")
                    {
                        // This is a release version
                        version = String.Format(vertext_release, regexresult[0].Groups[1].Value, regexresult[0].Groups[2].Value, regexresult[0].Groups[3].Value);
                        patch = "v" + version;
                        isrelease = true;
                    }
                    else
                    {
                        version = String.Format(vertext_dev, regexresult[0].Groups[1].Value, regexresult[0].Groups[2].Value, regexresult[0].Groups[3].Value, run["run_number"]);
                        patch = "v" + version + "-g" + run["head_sha"].ToString().Substring(0, 9);
                        isrelease = false;
                    }
                }

                var downloadurl = "";
                var artifactjson = UpdateChecker.Get(run["artifacts_url"].ToString());
                var downloadurls = JObject.Parse(artifactjson)["artifacts"]
                                          .Where(artifact => artifact["name"].ToString() == "Win32-Release" && !Convert.ToBoolean(artifact["expired"]))
                                          .Select(artifact => artifact["archive_download_url"].ToString()).ToList();

                if (downloadurls.Count() > 0)
                {
                    downloadurl = downloadurls[0];
                    Console.WriteLine(downloadurls[0]);
                }

                return new Update
                {
                    Patch = patch,
                    Version = version,
                    IsRelease = isrelease,
                    Author = run["head_commit"]["author"]["name"].ToString(),
                    DateTime = Convert.ToDateTime(run["head_commit"]["timestamp"]),
                    DownloadUrl = downloadurl,
                    FileType = "zip",
                    Description = run["head_commit"]["message"].ToString()
                };
            }).Where(update => update.DateTime > Current.DateTime).ToList();
        }
    }
}