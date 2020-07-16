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
    public class GIFRegRGBAQ : GIFReg
    {
        public byte R;
        public byte G;
        public byte B;
        public byte A;
        public float Q;

        public GIFRegRGBAQ(byte addr, UInt64 LowData, UInt64 HighData, bool PackedFormat) : base(addr, LowData, HighData, PackedFormat) { }

        static public GIFReg Unpack(GIFTag tag, byte addr, UInt64 LowData, UInt64 HighData, bool PackedFormat)
        {
            GIFRegRGBAQ r = new GIFRegRGBAQ(addr, LowData, HighData, PackedFormat);
            r.Descriptor = (GIFRegDescriptor)addr;
            if (PackedFormat)
            {
                r.R = (byte)GetBit(LowData, 0, 8);
                r.G = (byte)GetBit(LowData, 32, 8);
                r.B = (byte)GetBit(HighData, 0, 8);
                r.A = (byte)GetBit(HighData, 32, 8);
                r.Q = tag.Q;
            }
            else
            {
                r.R = (byte)GetBit(LowData, 0, 8);
                r.G = (byte)GetBit(LowData, 8, 8);
                r.B = (byte)GetBit(LowData, 16, 8);
                r.A = (byte)GetBit(LowData, 24, 8);
                r.Q = BitConverter.ToSingle(BitConverter.GetBytes(LowData), 4);
            }
            return r;
        }

        public override string ToString()
        {
            return Descriptor.ToString() + "@Red : " + R.ToString() + "@Green : " + G.ToString() + "@Blue : " + B.ToString() + "@Alpha : " + A.ToString();
        }
    }
}
