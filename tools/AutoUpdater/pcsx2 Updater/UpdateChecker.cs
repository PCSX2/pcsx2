using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net.Http;

namespace pcsx2_Updater
{
    abstract class UpdateChecker
    {
        public UpdateChecker()
        {
            Check();
        }
        protected abstract void Check();
    }
}
