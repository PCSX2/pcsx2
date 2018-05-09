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
        [DllImport("kernel32")]
        public extern static IntPtr LoadLibrary(string lpLibFileName);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32")]
        public extern static bool FreeLibrary(IntPtr hLibModule);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32", CharSet = CharSet.Ansi)]
        public extern static IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32", CharSet = CharSet.Ansi)]
        public extern static int SetErrorMode(int Value);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32", CharSet = CharSet.Ansi)]
        public extern static int GetLastError();

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("kernel32", CharSet = CharSet.Ansi)]
        public extern static int WritePrivateProfileString(string lpAppName, string lpKeyName, string lpString, string lpFileName);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32", CharSet = CharSet.Ansi)]
        public extern static short GetAsyncKeyState(int key);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32", CharSet = CharSet.Ansi)]
        public extern static int SetClassLong(IntPtr HWND, int index, long newlong);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32", CharSet = CharSet.Ansi)]
        public extern static bool IsWindowVisible(IntPtr HWND);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = false)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool PeekMessage(out NativeMessage message, IntPtr hwnd, uint messageFilterMin, uint messageFilterMax, uint flags);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = false)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool TranslateMessage(ref NativeMessage message);

        [SuppressUnmanagedCodeSecurityAttribute]
        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = false)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool DispatchMessage(ref NativeMessage message);
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
