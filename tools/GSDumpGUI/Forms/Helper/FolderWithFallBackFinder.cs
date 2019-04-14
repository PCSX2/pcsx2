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
            return new DirectoryInfo(Directory.GetCurrentDirectory());
        }

        private static bool TryGetExistingDirectory(string[] relativePaths, string pattern, out DirectoryInfo validDirectory)
        {
            if (relativePaths == null)
                throw new ArgumentNullException(nameof(relativePaths));
            foreach (var relativePath in relativePaths)
            {

                var candidate = new DirectoryInfo(Path.Combine(Directory.GetCurrentDirectory(), relativePath));
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
