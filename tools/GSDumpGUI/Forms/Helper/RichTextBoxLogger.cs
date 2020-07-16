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
using System.Drawing;
using System.Windows.Forms;

namespace GSDumpGUI.Forms.Helper
{
    public class RichTextBoxLogger : ILogger
    {
        private readonly RichTextBox  _richTextBox;
        public RichTextBoxLogger(RichTextBox  richTextBox)
        {
            _richTextBox = richTextBox;
            _richTextBox.BackColor = Color.White;
            _richTextBox.Focus();
            _richTextBox.HideSelection = false;
        }

        private void WriteLine(Color color, string line = null)
        {
            _richTextBox.Invoke(new MethodInvoker(delegate
            {
                ThreadLocalWrite(color, line);
            }));
        }

        private void ThreadLocalWrite(Color color, string line)
        {
            if (line == null)
            {
                _richTextBox.AppendText(Environment.NewLine);
                return;
            }

            _richTextBox.SelectionStart = _richTextBox.TextLength;
            _richTextBox.SelectionLength = 0;

            _richTextBox.SelectionColor = color;
            _richTextBox.AppendText(line);
            _richTextBox.SelectionColor = _richTextBox.ForeColor;

            _richTextBox.AppendText(Environment.NewLine);
        }

        public void Information(string line = null) => WriteLine(Color.Black, line);
        public void Warning(string line = null) => WriteLine(Color.DarkGoldenrod, line);
        public void Error(string line = null) => WriteLine(Color.DarkRed, line);
    }
}
