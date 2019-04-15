/*
 * Copyright (C) 2009-2019 PCSX2 Dev Team
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
using System.ComponentModel;

namespace GSDumpGUI.Forms.Entities
{
    public abstract class GsFiles<TUnderlying>
            where TUnderlying : GsFile
    {
        private int _selectedFileIndex = -1;

        public class SelectedIndexUpdatedEventArgs
        {
            public SelectedIndexUpdatedEventArgs(int formerIndex, int updatedIndex)
            {
                FormerIndex = formerIndex;
                UpdatedIndex = updatedIndex;
            }

            public int FormerIndex { get; }
            public int UpdatedIndex { get; }
        }

        public delegate void SelectedIndexUpdateEventHandler(object sender, SelectedIndexUpdatedEventArgs args);

        public event SelectedIndexUpdateEventHandler OnIndexUpdatedEvent;
        public BindingList<TUnderlying> Files { get; } = new BindingList<TUnderlying>();

        public int SelectedFileIndex
        {
            get
            {
                return _selectedFileIndex;
            }
            set
            {
                var oldValue = _selectedFileIndex;
                _selectedFileIndex = value;
                OnIndexUpdatedEvent?.Invoke(this, new SelectedIndexUpdatedEventArgs(oldValue, value));
            }
        }

        public bool IsSelected => SelectedFileIndex != -1 && Files.Count > SelectedFileIndex;

        public TUnderlying Selected
        {
            get
            {
                return SelectedFileIndex >= 0 ? Files[SelectedFileIndex] : null;
            }
            set
            {
                SelectedFileIndex = Files.IndexOf(value);
            }
        }
    }
}