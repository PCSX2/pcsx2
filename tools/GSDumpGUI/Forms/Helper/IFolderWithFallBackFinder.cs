using System.IO;

namespace GSDumpGUI.Forms.Helper
{
    public interface IFolderWithFallBackFinder
    {
        DirectoryInfo GetViaPatternWithFallback(string defaultDir, string filePattern, params string[] fallBackFolder);
    }
}