/*
 * Copyright (C) 2009-2020 PCSX2 Dev Team
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

using System.Windows.Forms;

// Important ! Create the ExtensionMethods class as a "public static" class
public static class ExtensionMethods
{
    public static void EnableContextMenu(this RichTextBox rtb)
    {
        if (rtb.ContextMenuStrip == null)
        {
            // Create a ContextMenuStrip without icons
            ContextMenuStrip cms = new ContextMenuStrip();
            cms.ShowImageMargin = false;

            // Add the Copy option (copies the selected text inside the richtextbox)
            ToolStripMenuItem tsmiCopy = new ToolStripMenuItem("Copy");
            tsmiCopy.Click += (sender, e) => rtb.Copy();
            cms.Items.Add(tsmiCopy);

            // Add the Clear option (clears the text inside the richtextbox)
            ToolStripMenuItem tsmiClear = new ToolStripMenuItem("Clear Log");
            tsmiClear.Click += (sender, e) => rtb.Clear();
            cms.Items.Add(tsmiClear);

            // Add a Separator
            cms.Items.Add(new ToolStripSeparator());

            // Add the Select All Option (selects all the text inside the richtextbox)
            ToolStripMenuItem tsmiSelectAll = new ToolStripMenuItem("Select All");
            tsmiSelectAll.Click += (sender, e) => rtb.SelectAll();
            cms.Items.Add(tsmiSelectAll);

            // When opening the menu, check if the condition is fulfilled 
            // in order to enable the action
            cms.Opening += (sender, e) =>
            {
                tsmiCopy.Enabled = rtb.SelectionLength > 0;
                tsmiClear.Enabled = rtb.TextLength > 0;
                tsmiSelectAll.Enabled = rtb.TextLength > 0 && rtb.SelectionLength < rtb.TextLength;
            };

            rtb.ContextMenuStrip = cms;
        }
    }
}
