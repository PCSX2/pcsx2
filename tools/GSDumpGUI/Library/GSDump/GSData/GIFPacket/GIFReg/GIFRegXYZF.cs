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

namespace GSDumpGUI
{
    [Serializable]
    public class GIFRegXYZF : GIFReg
    {
        public double X;
        public double Y;
        public UInt32 Z;
        public UInt16 F;

        public bool IsXYZF;

        public GIFRegXYZF(byte addr, UInt64 LowData, UInt64 HighData, bool PackedFormat) : base(addr, LowData, HighData, PackedFormat) { }

        static public GIFReg UnpackXYZ(GIFTag tag, byte addr, UInt64 LowData, UInt64 HighData, bool PackedFormat)
        {
            GIFRegXYZF xyzf = new GIFRegXYZF(addr, LowData, HighData, PackedFormat);

            xyzf.IsXYZF = false;
            if (PackedFormat && addr == (int)GIFRegDescriptor.XYZ2 && GetBit(HighData, 47, 1) == 1)
                xyzf.Descriptor = GIFRegDescriptor.XYZ3;
            else
                xyzf.Descriptor = (GIFRegDescriptor)addr;

            if (PackedFormat)
            {
                xyzf.X = GetBit(LowData, 0, 16) / 16d;
                xyzf.Y = GetBit(LowData, 32, 16) / 16d;
                xyzf.Z = (UInt32)(GetBit(HighData, 0, 32));
            }
            else
            {
                xyzf.X = GetBit(LowData, 0, 16) / 16d;
                xyzf.Y = GetBit(LowData, 16, 16) / 16d;
                xyzf.Z = (UInt32)(GetBit(LowData, 32, 32));
            }
            return xyzf;
        }

        static public GIFReg Unpack(GIFTag tag, byte addr, UInt64 LowData, UInt64 HighData, bool PackedFormat)
        {
            GIFRegXYZF xyzf = new GIFRegXYZF(addr, LowData, HighData, PackedFormat);

            xyzf.IsXYZF = true;
            if (PackedFormat && addr == (int)GIFRegDescriptor.XYZF2 && GetBit(HighData, 47, 1) == 1)
                xyzf.Descriptor = GIFRegDescriptor.XYZF3;
            else
                xyzf.Descriptor = (GIFRegDescriptor)addr;

            if (PackedFormat)
            {
                xyzf.X = GetBit(LowData, 0, 16) / 16d;
                xyzf.Y = GetBit(LowData, 32, 16) / 16d;
                xyzf.Z = (UInt32)(GetBit(HighData, 4, 24));
                xyzf.F = (UInt16)(GetBit(HighData, 36, 8));
            }
            else
            {
                xyzf.X = GetBit(LowData, 0, 16) / 16d;
                xyzf.Y = GetBit(LowData, 16, 16) / 16d;
                xyzf.Z = (UInt32)(GetBit(LowData, 32, 24));
                xyzf.F = (UInt16)(GetBit(LowData, 56, 8));
            }
            return xyzf;
        }

        public override string ToString()
        {
            return Descriptor.ToString() + "@X : " + X.ToString("F4") + "@Y : " + Y.ToString("F4") + "@Z : " + Z.ToString() + (IsXYZF ? "@F : " + F.ToString() : "");
        }
    }
}
