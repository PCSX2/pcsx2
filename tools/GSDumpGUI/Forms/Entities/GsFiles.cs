using System.Collections.Generic;
using System.ComponentModel;

namespace GSDumpGUI.Forms.Entities
{
    public abstract class GsFiles
    {
        public BindingList<GsFile> Files { get; } = new BindingList<GsFile>();
        public int SelectedFileIndex { get; private set; } = -1;

        public void SelectFirstIfAvailable()
        {
            if (Files.Count > 0)
                SelectedFileIndex = 0;
        }

        public bool IsSelected => SelectedFileIndex != -1;
        public GsFile Selected => SelectedFileIndex >= 0 ? Files[SelectedFileIndex] : default(GsFile);
    }
}