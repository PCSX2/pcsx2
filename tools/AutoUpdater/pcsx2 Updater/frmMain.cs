using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using CefSharp;
using CefSharp.WinForms;

namespace pcsx2_Updater
{
    public partial class frmMain : Form
    {
        public frmMain()
        {
            InitializeComponent();
            CefSettings settings = new CefSettings();
            Cef.Initialize(settings);
        }

        private void frmMain_Load(object sender, EventArgs e)
        {
            while (!Cef.IsInitialized) { }
            Web.Load("https://yiays.com/");
        }
        private void frmMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            Cef.Shutdown();
        }
    }

}
