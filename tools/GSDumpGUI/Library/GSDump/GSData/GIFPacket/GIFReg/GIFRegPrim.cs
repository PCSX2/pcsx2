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
    public class GIFRegPRIM : GIFReg
    {
        public GS_PRIM PrimitiveType;
        public GSIIP IIP;
        public bool TME;
        public bool FGE;
        public bool ABE;
        public bool AA1;
        public GSFST FST;
        public GSCTXT CTXT;
        public GSFIX FIX;

        public GIFRegPRIM(byte addr, UInt64 LowData, UInt64 HighData, bool PackedFormat) : base(addr, LowData, HighData, PackedFormat) { }

        static public GIFReg Unpack(GIFTag tag, byte addr, UInt64 LowData, UInt64 HighData, bool PackedFormat)
        {
            GIFRegPRIM pr = new GIFRegPRIM(addr, LowData, HighData, PackedFormat);
            pr.Descriptor = (GIFRegDescriptor)addr;
            pr.PrimitiveType = (GS_PRIM)GetBit(LowData, 0, 3);
            pr.IIP = (GSIIP)GetBit(LowData, 3, 1);
            pr.TME = Convert.ToBoolean(GetBit(LowData, 4, 1));
            pr.FGE = Convert.ToBoolean(GetBit(LowData, 5, 1));
            pr.ABE = Convert.ToBoolean(GetBit(LowData, 6, 1));
            pr.AA1 = Convert.ToBoolean(GetBit(LowData, 7, 1));
            pr.FST = (GSFST)(GetBit(LowData, 8, 1));
            pr.CTXT = (GSCTXT)(GetBit(LowData, 9, 1));
            pr.FIX = (GSFIX)(GetBit(LowData, 10, 1));
            return pr;
        }

        public override string ToString()
        {
            return Descriptor.ToString() + "@Primitive Type : " + PrimitiveType.ToString() + "@IIP : " + IIP.ToString() + "@TME : " + TME.ToString() + "@FGE : " + FGE.ToString()
                + "@ABE : " + ABE.ToString() + "@AA1 : " + AA1.ToString() + "@FST : " + FST.ToString() + "@CTXT : " + CTXT.ToString() + "@FIX : " + FIX.ToString();
        }
    }

    public enum GSIIP
    {
        FlatShading=0,
        Gouraud=1
    }

    public enum GSFST
    {
        STQValue=0,
        UVValue=1
    }

    public enum GSCTXT
    {
        Context1 =0,
        Context2 =1
    }

    public enum GSFIX
    {
        Unfixed =0,
        Fixed = 1
    }
}
