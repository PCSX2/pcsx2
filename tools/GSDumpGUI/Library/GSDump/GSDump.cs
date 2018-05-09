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
using System.IO;

namespace GSDumpGUI
{
    public class GSDump
    {
        public Int32 CRC;
        public byte[] GSFreeze;
        public byte[] StateData;
        public byte[] Registers; // 8192 bytes

        public int Size
        {
            get
            {
                int size = 0;
                size = 4;
                size += StateData.Length;
                size += Registers.Length;
                foreach (var itm in Data)
                {
                    size += itm.data.Length;
                }
                return size;
            }
        }

        public List<GSData> Data;

        public GSDump()
        {
            Data = new List<GSData>();
        }

        public GSDump Clone()
        {
            GSDump newDump = new GSDump();
            newDump.CRC = this.CRC;

            byte[] state = new byte[StateData.Length];
            Array.Copy(StateData,state, StateData.Length);
            newDump.StateData = state;

            newDump.Registers = new byte[8192];
            Array.Copy(this.Registers, newDump.Registers, 8192);

            foreach (var itm in this.Data)
            {
                if (itm.GetType().IsInstanceOfType(typeof(GSTransfer)))
                {
                    GSTransfer gt = new GSTransfer();
                    gt.id = itm.id;
                    gt.Path = ((GSTransfer)itm).Path;
                    gt.data = new byte[itm.data.Length];
                    Array.Copy(itm.data, gt.data, itm.data.Length);
                    newDump.Data.Add(gt);
                }
                else
                {
                    GSData gt = new GSData();
                    gt.id = itm.id;
                    gt.data = new byte[itm.data.Length];
                    Array.Copy(itm.data, gt.data, itm.data.Length);
                    newDump.Data.Add(gt);
                }
            }
            return newDump;
        }

        static public GSDump LoadDump(String FileName)
        {
            GSDump dmp = new GSDump();

            BinaryReader br = new BinaryReader(System.IO.File.Open(FileName, FileMode.Open));
            dmp.CRC = br.ReadInt32();

            Int32 ss = br.ReadInt32();
            dmp.StateData = br.ReadBytes(ss);         
        
            dmp.Registers = br.ReadBytes(8192);

            while (br.PeekChar() != -1)
            {
                GSType id = (GSType)br.ReadByte();
                switch (id)
                {
                    case GSType.Transfer:
                        GSTransfer data = new GSTransfer();

                        byte index = br.ReadByte();

                        data.id = id;
                        data.Path = (GSTransferPath)index;

                        Int32 size = br.ReadInt32();

                        List<byte> Data = new List<byte>();
                        Data.AddRange(br.ReadBytes(size));
                        data.data = Data.ToArray();
                        dmp.Data.Add(data);
                        break;
                    case GSType.VSync:
                        GSData dataV = new GSData();
                        dataV.id = id;
                        dataV.data = br.ReadBytes(1);
                        dmp.Data.Add(dataV);
                        break;
                    case GSType.ReadFIFO2:
                        GSData dataR = new GSData();
                        dataR.id = id;
                        Int32 sF = br.ReadInt32();
                        dataR.data = BitConverter.GetBytes(sF);
                        dmp.Data.Add(dataR);
                        break;
                    case GSType.Registers:
                        GSData dataRR = new GSData();
                        dataRR.id = id;
                        dataRR.data = br.ReadBytes(8192);
                        dmp.Data.Add(dataRR);
                        break;
                    default:
                        break;
                }
            }
            br.Close();

            return dmp;
        }
    }
}
