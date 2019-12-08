using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace pcsx2_Updater
{
    public static class Config
    {
        public static string InstalledPatchName;
        public static DateTime InstalledPatchDate;
        public static bool Enabled;
        public static bool Refresh;
        public static bool Silent;
        public static Channels Channel;
        public static DateTime SkipBuild;
        private static IniFile ini = new IniFile(AppDomain.CurrentDomain.BaseDirectory + @"AutoUpdate.ini");

        public static void Read()
        {
            if(!File.Exists(AppDomain.CurrentDomain.BaseDirectory + @"AutoUpdate.ini"))
            {
                ini.Write("InstalledPatchName", "0");
                ini.Write("InstalledPatchDate", "1/1/1970");
                ini.Write("Enabled", "true");
                ini.Write("Channel", "Development");
                ini.Write("SkipBuild", "1/1/1970");
            }
            InstalledPatchName = ini.Read("InstalledPatchName");
            InstalledPatchDate = Convert.ToDateTime(ini.Read("InstalledPatchDate"));
            Config.Enabled = Convert.ToBoolean(ini.Read("Enabled"));
            try
            {
                Channel = (Channels)Enum.Parse(typeof(Channels), ini.Read("Channel"));
            }
            catch
            {
                Channel = Channels.Development;
            }
            SkipBuild = Convert.ToDateTime(ini.Read("SkipBuild"));
        }
        public static void Write()
        {
            ini.Write("InstalledPatchName", InstalledPatchName);
            ini.Write("InstalledPatchDate", InstalledPatchDate.ToString());
            ini.Write("Enabled", Enabled.ToString());
            ini.Write("Channel", Channel.ToString());
            ini.Write("SkipBuild", SkipBuild.ToString());
        }
    }
    public enum Channels
    {
        Development,
        Stable
    }
    class IniFile
        /* revision 11 - by Danny Beckett https://stackoverflow.com/questions/217902/reading-writing-an-ini-file#answer-14906422
         * Drastically simplified for this application.
         */
    {
        string Path;

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        static extern long WritePrivateProfileString(string Section, string Key, string Value, string FilePath);

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        static extern int GetPrivateProfileString(string Section, string Key, string Default, StringBuilder RetVal, int Size, string FilePath);

        public IniFile(string IniPath)
        {
            Path = IniPath;
        }

        public string Read(string Key, string Section = "main")
        {
            var RetVal = new StringBuilder(255);
            GetPrivateProfileString(Section, Key, "", RetVal, 255, Path);
            return RetVal.ToString();
        }

        public void Write(string Key, string Value, string Section = "main")
        {
            WritePrivateProfileString(Section, Key, Value, Path);
        }
    }
}
