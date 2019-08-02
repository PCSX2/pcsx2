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
using System.Security;
using System.Runtime.InteropServices;
using System.Drawing;

namespace GSDumpGUI
{
    static public class NativeMethods
    {
        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        public extern static IntPtr LoadLibrary(string lpLibFileName);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public extern static bool FreeLibrary(IntPtr hLibModule);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
        public extern static IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32")]
        public extern static UInt32 SetErrorMode(UInt32 uMode);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32")]
        public extern static UInt32 GetLastError();

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public extern static bool WritePrivateProfileString(string lpAppName, string lpKeyName, string lpString, string lpFileName);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32")]
        public extern static UInt16 GetAsyncKeyState(Int32 vKey);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32", CharSet = CharSet.Auto, EntryPoint = "SetClassLong")]
        public extern static UInt32 SetClassLong32(IntPtr hWnd, Int32 index, Int32 dwNewLong);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32", CharSet = CharSet.Auto, EntryPoint = "SetClassLongPtr")]
        public extern static UIntPtr SetClassLong64(IntPtr hWnd, Int32 index, IntPtr dwNewLong);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32")]
        public extern static bool IsWindowVisible(IntPtr hWnd);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool PeekMessage(out NativeMessage lpMsg, IntPtr hWnd, UInt32 wMsgFilterMin, UInt32 wMsgFilterMax, UInt32 wRemoveMsg);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool TranslateMessage(ref NativeMessage lpMsg);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        public static extern UInt32 DispatchMessage(ref NativeMessage lpMsg);

        public static UIntPtr SetClassLong(IntPtr hWnd, Int32 index, IntPtr dwNewLong)
        {
            if (Environment.Is64BitProcess) return SetClassLong64(hWnd, index, dwNewLong);
            else return new UIntPtr(SetClassLong32(hWnd, index, dwNewLong.ToInt32()));
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeMessage
    {
        public IntPtr hWnd;
        public uint msg;
        public IntPtr wParam;
        public IntPtr lParam;
        public uint time;
        public Point p;
    }
}
