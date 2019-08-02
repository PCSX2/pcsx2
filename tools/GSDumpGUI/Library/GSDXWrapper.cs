/*
 * Copyright (C) 2009-2011 Ferreri Alessio
 * Copyright (C) 2009-2018 PCSX2 Dev Team
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
using System.IO;
using TCPLibrary.MessageBased.Core;
using System.Threading;

namespace GSDumpGUI
{
    public delegate   void GSgifTransfer(IntPtr data, int size);
    public delegate   void GSgifTransfer1(IntPtr data, int size);
    public delegate   void GSgifTransfer2(IntPtr data, int size);
    public delegate   void GSgifTransfer3(IntPtr data, int size);
    public delegate   void GSVSync(byte field);
    public delegate   void GSreset();
    public delegate   void GSreadFIFO2(IntPtr data, int size);
    public delegate   void GSsetGameCRC(int crc, int options);
    public delegate    int GSfreeze(int mode, IntPtr data);
    public delegate    int GSopen(IntPtr hwnd, String Title, int renderer);
    public delegate   void GSclose();
    public delegate   void GSshutdown();
    public delegate   void GSConfigure();
    public delegate   void GSsetBaseMem(IntPtr data);
    public delegate IntPtr PSEgetLibName();
    public delegate   void GSinit();
    public delegate UInt32 GSmakeSnapshot(string path);

    public class InvalidGSPlugin : Exception
    {
        public InvalidGSPlugin(string reason) : base(reason) {}
    }

    public class GSDXWrapper
    {
        static public bool DumpTooOld = false;

        private GSConfigure gsConfigure;
        private PSEgetLibName PsegetLibName;
        private GSgifTransfer GSgifTransfer;
        private GSgifTransfer1 GSgifTransfer1;
        private GSgifTransfer2 GSgifTransfer2;
        private GSgifTransfer3 GSgifTransfer3;
        private GSVSync GSVSync;
        private GSreadFIFO2 GSreadFIFO2;
        private GSsetGameCRC GSsetGameCRC;
        private GSfreeze GSfreeze;
        private GSopen GSopen;
        private GSclose GSclose;
        private GSshutdown GSshutdown;
        private GSsetBaseMem GSsetBaseMem;
        private GSinit GSinit;
        private GSreset GSreset;
        private GSmakeSnapshot GSmakeSnapshot;
        private Boolean Loaded;

        private String DLL;
        private IntPtr DLLAddr;

        private Boolean Running;

        public Queue<TCPMessage> QueueMessage;
        public Boolean DebugMode;
        public GSData CurrentGIFPacket;
        public bool ThereIsWork;
        public AutoResetEvent ExternalEvent;
        public int RunTo;

        public void Load(String DLL)
        {
            var formerDirectory = Directory.GetCurrentDirectory();
            try
            {
                this.DLL = DLL;
                NativeMethods.SetErrorMode(0x8007);

                if (Loaded)
                    Unload();

                string dir = DLL;
                dir = Path.GetDirectoryName(dir);
                if (dir == null) return;

                Directory.SetCurrentDirectory(dir);
                IntPtr hmod = NativeMethods.LoadLibrary(DLL);
                if (hmod != IntPtr.Zero)
                {
                    DLLAddr = hmod;

                    IntPtr funcaddrLibName = NativeMethods.GetProcAddress(hmod, "PS2EgetLibName");
                    IntPtr funcaddrConfig = NativeMethods.GetProcAddress(hmod, "GSconfigure");

                    IntPtr funcaddrGIF = NativeMethods.GetProcAddress(hmod, "GSgifTransfer");
                    IntPtr funcaddrGIF1 = NativeMethods.GetProcAddress(hmod, "GSgifTransfer1");
                    IntPtr funcaddrGIF2 = NativeMethods.GetProcAddress(hmod, "GSgifTransfer2");
                    IntPtr funcaddrGIF3 = NativeMethods.GetProcAddress(hmod, "GSgifTransfer3");
                    IntPtr funcaddrVSync = NativeMethods.GetProcAddress(hmod, "GSvsync");
                    IntPtr funcaddrSetBaseMem = NativeMethods.GetProcAddress(hmod, "GSsetBaseMem");
                    IntPtr funcaddrGSReset = NativeMethods.GetProcAddress(hmod, "GSreset");
                    IntPtr funcaddrOpen = NativeMethods.GetProcAddress(hmod, "GSopen");
                    IntPtr funcaddrSetCRC = NativeMethods.GetProcAddress(hmod, "GSsetGameCRC");
                    IntPtr funcaddrClose = NativeMethods.GetProcAddress(hmod, "GSclose");
                    IntPtr funcaddrShutdown = NativeMethods.GetProcAddress(hmod, "GSshutdown");
                    IntPtr funcaddrFreeze = NativeMethods.GetProcAddress(hmod, "GSfreeze");
                    IntPtr funcaddrGSreadFIFO2 = NativeMethods.GetProcAddress(hmod, "GSreadFIFO2");
                    IntPtr funcaddrinit = NativeMethods.GetProcAddress(hmod, "GSinit");
                    IntPtr funcmakeSnapshot = NativeMethods.GetProcAddress(hmod, "GSmakeSnapshot");

                    if (!((funcaddrConfig.ToInt64() > 0) && (funcaddrLibName.ToInt64() > 0) && (funcaddrGIF.ToInt64() > 0)))
                        throw new InvalidGSPlugin("");

                    gsConfigure = (GSConfigure) Marshal.GetDelegateForFunctionPointer(funcaddrConfig, typeof(GSConfigure));
                    PsegetLibName = (PSEgetLibName) Marshal.GetDelegateForFunctionPointer(funcaddrLibName, typeof(PSEgetLibName));

                    this.GSgifTransfer = (GSgifTransfer) Marshal.GetDelegateForFunctionPointer(funcaddrGIF, typeof(GSgifTransfer));
                    this.GSgifTransfer1 = (GSgifTransfer1) Marshal.GetDelegateForFunctionPointer(funcaddrGIF1, typeof(GSgifTransfer1));
                    this.GSgifTransfer2 = (GSgifTransfer2) Marshal.GetDelegateForFunctionPointer(funcaddrGIF2, typeof(GSgifTransfer2));
                    this.GSgifTransfer3 = (GSgifTransfer3) Marshal.GetDelegateForFunctionPointer(funcaddrGIF3, typeof(GSgifTransfer3));
                    this.GSVSync = (GSVSync) Marshal.GetDelegateForFunctionPointer(funcaddrVSync, typeof(GSVSync));
                    this.GSsetBaseMem = (GSsetBaseMem) Marshal.GetDelegateForFunctionPointer(funcaddrSetBaseMem, typeof(GSsetBaseMem));
                    this.GSopen = (GSopen) Marshal.GetDelegateForFunctionPointer(funcaddrOpen, typeof(GSopen));
                    this.GSsetGameCRC = (GSsetGameCRC) Marshal.GetDelegateForFunctionPointer(funcaddrSetCRC, typeof(GSsetGameCRC));
                    this.GSclose = (GSclose) Marshal.GetDelegateForFunctionPointer(funcaddrClose, typeof(GSclose));
                    this.GSshutdown = (GSshutdown) Marshal.GetDelegateForFunctionPointer(funcaddrShutdown, typeof(GSshutdown));
                    this.GSfreeze = (GSfreeze) Marshal.GetDelegateForFunctionPointer(funcaddrFreeze, typeof(GSfreeze));
                    this.GSreset = (GSreset) Marshal.GetDelegateForFunctionPointer(funcaddrGSReset, typeof(GSreset));
                    this.GSreadFIFO2 = (GSreadFIFO2) Marshal.GetDelegateForFunctionPointer(funcaddrGSreadFIFO2, typeof(GSreadFIFO2));
                    this.GSinit = (GSinit) Marshal.GetDelegateForFunctionPointer(funcaddrinit, typeof(GSinit));
                    this.GSmakeSnapshot = (GSmakeSnapshot)Marshal.GetDelegateForFunctionPointer(funcmakeSnapshot, typeof(GSmakeSnapshot));

                    Loaded = true;
                }

                if (!Loaded)
                {
                    Exception lasterror = Marshal.GetExceptionForHR(Marshal.GetHRForLastWin32Error());
                    System.IO.File.AppendAllText(AppDomain.CurrentDomain.BaseDirectory + "log.txt", DLL + " failed to load. Error " + lasterror.ToString() + Environment.NewLine);
                    NativeMethods.SetErrorMode(0x0000);
                    Unload();
                    throw new InvalidGSPlugin(lasterror.ToString());
                }

                NativeMethods.SetErrorMode(0x0000);
            }
            finally
            {
                Directory.SetCurrentDirectory(formerDirectory);
            }
        }

        public void Unload()
        {
            NativeMethods.FreeLibrary(DLLAddr);
            Loaded = false;
        }

        public void GSConfig()
        {
            if (!Loaded)
                throw new Exception("GSdx is not loaded");
            gsConfigure.Invoke();
        }

        public String PSEGetLibName()
        {
            if (!Loaded)
                throw new Exception("GSdx is not loaded");
            return Marshal.PtrToStringAnsi(PsegetLibName.Invoke());
        }

        public unsafe void Run(GSDump dump, int rendererOverride)
        {
            QueueMessage = new Queue<TCPMessage>();
            Running = true;
            ExternalEvent = new AutoResetEvent(true);

            GSinit();
            byte[] tempregisters = new byte[8192];
            Array.Copy(dump.Registers, tempregisters, 8192);
            fixed (byte* pointer = tempregisters)
            {
                GSsetBaseMem(new IntPtr(pointer));
                IntPtr hWnd = IntPtr.Zero;

                if (GSopen(new IntPtr(&hWnd), "", rendererOverride) != 0)
                    return;

                GSsetGameCRC(dump.CRC, 0);

                NativeMethods.SetClassLong(hWnd,/*GCL_HICON*/ -14, Program.hMainIcon);

                fixed (byte* freeze = dump.StateData)
                {
                    byte[] GSFreez;

                    if (Environment.Is64BitProcess)
                    {
                        GSFreez = new byte[16];
                        Array.Copy(BitConverter.GetBytes((Int64)dump.StateData.Length), 0, GSFreez, 0, 8);
                        Array.Copy(BitConverter.GetBytes(new IntPtr(freeze).ToInt64()), 0, GSFreez, 8, 8);
                    }
                    else
                    {
                        GSFreez = new byte[8];
                        Array.Copy(BitConverter.GetBytes((Int32)dump.StateData.Length), 0, GSFreez, 0, 4);
                        Array.Copy(BitConverter.GetBytes(new IntPtr(freeze).ToInt32()), 0, GSFreez, 4, 4);
                    }

                    fixed (byte* fr = GSFreez)
                    {
                        int ris = GSfreeze(0, new IntPtr(fr));
                        if (ris == -1)
                        {
                            DumpTooOld = true;
                            Running = false;
                        }
                        GSVSync(1);

                        GSreset();
                        Marshal.Copy(dump.Registers, 0, new IntPtr(pointer), 8192);
                        GSsetBaseMem(new IntPtr(pointer));
                        GSfreeze(0, new IntPtr(fr));

                        GSData gsd_vsync = new GSData();
                        gsd_vsync.id = GSType.VSync;

                        int gs_idx = 0;
                        int debug_idx = 0;
                        NativeMessage msg;

                        while (Running)
                        {
                            while (NativeMethods.PeekMessage(out msg, hWnd, 0, 0, 1)) // PM_REMOVE
                            {
                                NativeMethods.TranslateMessage(ref msg);
                                NativeMethods.DispatchMessage(ref msg);

                                if(msg.msg == 0x0100) // WM_KEYDOWN
                                {
                                    switch(msg.wParam.ToInt32() & 0xFF)
                                    {
                                        case 0x1B: Running = false; break; // VK_ESCAPE;
                                        case 0x77: GSmakeSnapshot(""); break; // VK_F8;
                                    }
                                }
                            }

                            if (!Running || !NativeMethods.IsWindowVisible(hWnd))
                                break;

                            if (DebugMode)
                            {
                                if (QueueMessage.Count > 0)
                                {
                                    TCPMessage Mess = QueueMessage.Dequeue();
                                    switch (Mess.MessageType)
                                    {
                                        case MessageType.Step:
                                            if (debug_idx >= dump.Data.Count)
                                                debug_idx = 0;
                                            RunTo = debug_idx;
                                            break;
                                        case MessageType.RunToCursor:
                                            RunTo = (int)Mess.Parameters[0];
                                            if(debug_idx > RunTo)
                                                debug_idx = 0;
                                            break;
                                        case MessageType.RunToNextVSync:
                                            if (debug_idx >= dump.Data.Count)
                                                debug_idx = 1;
                                            RunTo = dump.Data.FindIndex(debug_idx, a => a.id == GSType.VSync);
                                            break;
                                        default:
                                            break;
                                    }
                                }

                                if (debug_idx <= RunTo)
                                {
                                    while (debug_idx <= RunTo)
                                    {
                                        GSData itm = dump.Data[debug_idx];
                                        CurrentGIFPacket = itm;
                                        Step(itm, pointer);
                                        debug_idx++;
                                    }

                                    int idxnextReg = dump.Data.FindIndex(debug_idx, a => a.id == GSType.Registers);
                                    if (idxnextReg != -1)
                                        Step(dump.Data[idxnextReg], pointer);

                                    TCPMessage Msg = new TCPMessage();
                                    Msg.MessageType = MessageType.RunToCursor;
                                    Msg.Parameters.Add(debug_idx - 1);
                                    Program.Client.Send(Msg);

                                    ExternalEvent.Set();
                                }

                                Step(gsd_vsync, pointer);
                            }
                            else
                            {
                                while (gs_idx < dump.Data.Count)
                                {
                                    GSData itm = dump.Data[gs_idx++];
                                    CurrentGIFPacket = itm;
                                    Step(itm, pointer);

                                    if (gs_idx < dump.Data.Count && dump.Data[gs_idx].id == GSType.VSync)
                                        break;
                                }

                                if (gs_idx >= dump.Data.Count) gs_idx = 0;
                            }
                        }

                        GSclose();
                        GSshutdown();
                    }
                }
            }
        }

        private unsafe void Step(GSData itm, byte* registers)
        {
            /*"C:\Users\Alessio\Desktop\Plugins\Dll\GSdx-SSE4.dll" "C:\Users\Alessio\Desktop\Plugins\Dumps\gsdx_20101222215004.gs" "GSReplay" 0*/
            switch (itm.id)
            {
                case GSType.Transfer:
                    switch (((GSTransfer)itm).Path)
                    {
                        case GSTransferPath.Path1Old:
                            byte[] data = new byte[16384];
                            int addr = 16384 - itm.data.Length;
                            Array.Copy(itm.data, 0, data, addr, itm.data.Length);
                            fixed (byte* gifdata = data)
                            {
                                GSgifTransfer1(new IntPtr(gifdata), addr);
                            }
                            break;
                        case GSTransferPath.Path2:
                            fixed (byte* gifdata = itm.data)
                            {
                                GSgifTransfer2(new IntPtr(gifdata), (itm.data.Length) / 16);
                            }
                            break;
                        case GSTransferPath.Path3:
                            fixed (byte* gifdata = itm.data)
                            {
                                GSgifTransfer3(new IntPtr(gifdata), (itm.data.Length) / 16);
                            }
                            break;
                        case GSTransferPath.Path1New:
                            fixed (byte* gifdata = itm.data)
                            {
                                GSgifTransfer(new IntPtr(gifdata), (itm.data.Length) / 16);
                            }
                            break;
                    }
                    break;
                case GSType.VSync:
                    GSVSync((*((int*)(registers + 4096)) & 0x2000) > 0 ? (byte)1 : (byte)0);
                    break;
                case GSType.ReadFIFO2:
                    fixed (byte* FIFO = itm.data)
                    {
                        byte[] arrnew = new byte[*((int*)FIFO)];
                        fixed (byte* arrn = arrnew)
                        {
                            GSreadFIFO2(new IntPtr(arrn), *((int*)FIFO));
                        }
                    }
                    break;
                case GSType.Registers:
                    Marshal.Copy(itm.data, 0, new IntPtr(registers), 8192);
                    break;
                default:
                    break;
            }            
        }

        public void Stop()
        {
            Running = false;
        }

        internal List<Object> GetGifPackets(GSDump dump)
        {
            List<Object> Data = new List<Object>();
            for (int i = 0; i < dump.Data.Count; i++)
            {
                String act = i.ToString() + "|";
                act += dump.Data[i].id.ToString() + "|";
                if (dump.Data[i].GetType().IsSubclassOf(typeof(GSData)))
                {
                    act += ((GSTransfer)dump.Data[i]).Path.ToString() + "|";
                    act += ((GSTransfer)dump.Data[i]).data.Length;
                }
                else
                {
                    act += ((GSData)dump.Data[i]).data.Length;
                }
                Data.Add(act);
            }
            return Data;
        }

        internal object GetGifPacketInfo(GSDump dump, int i)
        {
            if (dump.Data[i].id == GSType.Transfer)
            {
                try
                {
                    GIFTag val = GIFTag.ExtractGifTag(dump.Data[i].data, ((GSTransfer)dump.Data[i]).Path);
                    return val;
                }
                catch (Exception)
                {
                    return new GIFTag();
                }
            }
            else
            {
                switch (dump.Data[i].id)
                {
                    case GSType.VSync:
                        return dump.Data[i].data.Length + "|Field = " + dump.Data[i].data[0].ToString();
                    case GSType.ReadFIFO2:
                        return dump.Data[i].data.Length + "|ReadFIFO2 : Size = " + BitConverter.ToInt32(dump.Data[i].data, 0).ToString() + " byte";
                    case GSType.Registers:
                        return dump.Data[i].data.Length + "|Registers";
                    default:
                        return "";
                }
            }
        }
    }
}
