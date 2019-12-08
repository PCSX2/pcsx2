using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace pcsx2_Updater
{
    static class Program
    {
        /// <summary>
        /// AutoUpdate.exe [--silent -s]
        /// When launching as silent, the updater will exit quietly if anything fails,
        /// or no updates are found. Intended for pcsx2.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            Config.Silent = false;
            if (args.Length > 0)
            {
                foreach(string arg in args)
                {
                    switch (arg)
                    {
                        case "--silent":
                        case "-s":
                            Config.Silent = true;
                            break;
                        default:
                            Console.WriteLine("Unknown argument " + arg);
                            break;
                    }
                }
            }
            Config.Read();
            if (Config.Enabled || !Config.Silent) // Allows for manual update checking
            {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Config.Refresh = true;
                while (Config.Refresh)
                {
                    Config.Refresh = false;
                    switch (Config.Channel)
                    {
                        case Channels.Development:
                            UpdateChecker upd = new Orphis();
                            break;
                        case Channels.Stable:
                            UpdateChecker upd = new GitHub();
                            break;
                        default:
                            if (!Config.Silent) MessageBox.Show("Unknown update channel in config!");
                            Console.WriteLine("Unknown update channel!");
                            break;
                    }
                }
            }
            else
            {
                Console.WriteLine("AutoUpdate has been disabled in the config file.");
            }
        }
    }
}
