using System;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace TH18LocalCoop {
    internal sealed class NetworkLog : IDisposable {
        readonly StreamWriter writer;
        readonly object gate=new object();bool closed;
        internal readonly string FilePath;
        internal NetworkLog(string gameDirectory,string kind) {
            string directory=Path.Combine(gameDirectory,"coop-data","logs");
            Directory.CreateDirectory(directory);
            FilePath=Path.Combine(directory,"network-"+kind+"-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+
                Process.GetCurrentProcess().Id+"-"+Guid.NewGuid().ToString("N").Substring(0,8)+".log");
            writer=new StreamWriter(new FileStream(FilePath,FileMode.CreateNew,FileAccess.Write,FileShare.Read),new UTF8Encoding(false));
            writer.AutoFlush=true;
            Write("TH18 co-op "+NetIdentity.Version+"; "+kind+"; run mode is recorded below.");
        }
        internal void Write(string message) {
            lock(gate){if(!closed)writer.WriteLine(DateTimeOffset.Now.ToString("o")+"  "+message);}
        }
        public void Dispose() {lock(gate){if(!closed){closed=true;writer.Dispose();}}}
    }
}
