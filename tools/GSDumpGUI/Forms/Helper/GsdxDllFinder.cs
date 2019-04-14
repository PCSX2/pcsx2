/*
 * Copyright (C) 2009-2011 Ferreri Alessio
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

using System.Collections.Generic;
using System.IO;
using GSDumpGUI.Forms.Entities;

namespace GSDumpGUI.Forms.Helper
{
    public class GsdxDllFinder : IGsdxDllFinder
    {
        private readonly ILogger _logger;

        public GsdxDllFinder(ILogger logger)
        {
            _logger = logger;
        }

        public IEnumerable<GsFile> GetEnrichedPathToValidGsdxDlls(DirectoryInfo directory)
        {
            var availableDlls = directory.GetFiles("*.dll", SearchOption.TopDirectoryOnly);

            var wrap = new GSDXWrapper();
            foreach (var availableDll in availableDlls)
            {
                GsFile dll;
                try
                {
                    wrap.Load(availableDll.FullName);

                    dll = new GsFile
                    {
                            DisplayText = availableDll.Name + " | " + wrap.PSEGetLibName(),
                            File = availableDll
                    };
                    _logger.Information($"'{availableDll}' correctly identified as '{wrap.PSEGetLibName()}'");

                    wrap.Unload();
                }
                catch (InvalidGSPlugin)
                {
                    _logger.Warning($"Failed to load '{availableDll}'. Is it really a GSdx DLL?");
                    continue;
                }

                yield return dll;
            }
        }
    }
}
