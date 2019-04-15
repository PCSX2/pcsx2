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
using System.IO;
using System.Linq;

namespace GSDumpGUI.Forms.Helper
{
    public class FolderWithFallBackFinder : IFolderWithFallBackFinder
    {
        public DirectoryInfo GetViaPatternWithFallback(string defaultDir, string filePattern, params string[] fallBackFolder)
        {
            if (!string.IsNullOrWhiteSpace(defaultDir))
                return new DirectoryInfo(defaultDir);

            DirectoryInfo gsdxDllDirectory;
            if (TryGetExistingDirectory(fallBackFolder, filePattern, out gsdxDllDirectory))
                return gsdxDllDirectory;
            return new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
        }

        private static bool TryGetExistingDirectory(string[] relativePaths, string pattern, out DirectoryInfo validDirectory)
        {
            if (relativePaths == null)
                throw new ArgumentNullException(nameof(relativePaths));
            foreach (var relativePath in relativePaths)
            {

                var candidate = new DirectoryInfo(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, relativePath));
                if (candidate.Exists && candidate.GetFiles(pattern).Any())
                {
                    validDirectory = candidate;
                    return true;
                }
            }

            validDirectory = null;
            return false;
        }
    }
}
