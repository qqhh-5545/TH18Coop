using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace TH18LocalCoop {
    internal static class Th14Launcher {
        internal const int DataAddress=0x4f5a45;
        [DllImport("kernel32.dll")]static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
        [DllImport("kernel32.dll")]static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr count,out UIntPtr read);
        [DllImport("kernel32.dll")]static extern bool CloseHandle(IntPtr handle);
        internal static string Profile(string appData){return Path.Combine(appData,"ShanghaiAlice","th14");}
        internal static void SyncConfig(string appData,string dataRoot,bool windowed) {
            string source=Path.Combine(Profile(appData),"th14.cfg"),target=Path.Combine(Profile(dataRoot),"th14.cfg");
            if(!File.Exists(source))throw new IOException("请先用辉针城 custom.exe 保存一次配置。");
            byte[] data=File.ReadAllBytes(source);
            if(data.Length!=92 || BitConverter.ToUInt32(data,0)!=0x140002)throw new IOException("辉针城配置格式不匹配。");
            // Only explicit automated tests override the private display mode.
            // Normal launches preserve every custom.exe setting byte-for-byte.
            if(windowed){data[0x1f]=3;data[0x20]=0;}
            Directory.CreateDirectory(Path.GetDirectoryName(target));File.WriteAllBytes(target,data);
        }
        internal static void SyncLaunchConfig(string appData,string dataRoot) {
            SyncConfig(appData,dataRoot,Environment.GetEnvironmentVariable("TH14_COOP_TEST")=="1");
        }
        internal static ProcessStartInfo StartInfo(string entry,string dataRoot,string modRoot=null) {
            var patch=Th14Thcrap.Load(modRoot);
            if(patch.Enabled){var patched=patch.StartInfo(entry,dataRoot);if(Environment.GetEnvironmentVariable("TH14_COOP_TEST")=="1")patched.WindowStyle=ProcessWindowStyle.Minimized;return patched;}
            var info=new ProcessStartInfo(entry){UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(entry),WindowStyle=ProcessWindowStyle.Normal};
            if(Environment.GetEnvironmentVariable("TH14_COOP_TEST")=="1")info.WindowStyle=ProcessWindowStyle.Minimized;
            info.EnvironmentVariables["APPDATA"]=Path.GetFullPath(dataRoot);
            if(String.Equals(Path.GetFileName(entry),"th14c.exe",StringComparison.OrdinalIgnoreCase))info.Arguments="th14.exe";
            return info;
        }
        internal static bool Matches(Process game,string engine,string dataRoot) {
            try {
                if(game.HasExited || !String.Equals(game.MainModule.FileName,engine,StringComparison.OrdinalIgnoreCase))return false;
                IntPtr handle=OpenProcess(0x410,false,game.Id);if(handle==IntPtr.Zero)return false;
                try {
                    byte[] bytes=new byte[4096];UIntPtr read;
                    if(!ReadProcessMemory(handle,new IntPtr(DataAddress),bytes,new UIntPtr(4096),out read))return false;
                    int end=Array.IndexOf(bytes,(byte)0);if(end<0)return false;
                    if(!MatchesProfile(bytes,dataRoot))return false;
                    // The data path exists before D3D and the Chinese bootstrap
                    // finish. Attach only after the native title is initialized.
                    byte[] word=new byte[4];
                    if(!ReadProcessMemory(handle,new IntPtr(0x4d8f68),word,new UIntPtr(4),out read) || read.ToUInt32()!=4 || BitConverter.ToUInt32(word,0)==0)return false;
                    if(!ReadProcessMemory(handle,new IntPtr(0x4db6a4),word,new UIntPtr(4),out read) || read.ToUInt32()!=4)return false;
                    uint menu=BitConverter.ToUInt32(word,0);if(menu==0)return false;
                    return ReadProcessMemory(handle,new IntPtr(unchecked((int)(menu+0x1c))),word,new UIntPtr(4),out read) && read.ToUInt32()==4 && BitConverter.ToUInt32(word,0)==1;
                }finally{CloseHandle(handle);}
            }catch{return false;}
        }
        internal static bool MatchesProfile(byte[] bytes,string dataRoot) {
            int end=Array.IndexOf(bytes,(byte)0);if(end<0)return false;
            string expected=Profile(Path.GetFullPath(dataRoot)).TrimEnd('\\','/');
            if(String.Equals(Encoding.Default.GetString(bytes,0,end).TrimEnd('\\','/'),expected,StringComparison.OrdinalIgnoreCase))return true;
            try {return String.Equals(new UTF8Encoding(false,true).GetString(bytes,0,end).TrimEnd('\\','/'),expected,StringComparison.OrdinalIgnoreCase);}
            catch(DecoderFallbackException){return false;}
        }
        internal static async Task Load(string root,int pid) {
            var info=new ProcessStartInfo(Path.Combine(root,GameCatalog.TH14.Loader),"--attach "+pid){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=root,RedirectStandardOutput=true,RedirectStandardError=true};
            using(var process=Process.Start(info)) {
                Task<string> output=process.StandardOutput.ReadToEndAsync(),error=process.StandardError.ReadToEndAsync();
                while(!process.HasExited)await Task.Delay(50);
                string result=(await output)+(await error);if(process.ExitCode!=0)throw new IOException(result);
            }
        }
        internal static async Task Run(string entry,string root,Action<string> note) {
            entry=GameCatalog.TH14.Validate(entry);string gameRoot=Path.GetDirectoryName(entry),engine=Path.Combine(gameRoot,"th14.exe"),dataRoot=Path.Combine(gameRoot,"coop-data");
            if(!File.Exists(Path.Combine(root,GameCatalog.TH14.Module)) || !File.Exists(Path.Combine(root,GameCatalog.TH14.Loader)))throw new IOException("辉针城模块尚未构建。");
            foreach(var game in Process.GetProcessesByName("th14"))using(game) {
                if(!game.HasExited && String.Equals(game.MainModule.FileName,engine,StringComparison.OrdinalIgnoreCase))throw new IOException("请先退出这个目录中已运行的辉针城。");
            }
            SyncLaunchConfig(Environment.GetEnvironmentVariable("APPDATA"),dataRoot);
            note("正在启动辉针城…");using(var first=Process.Start(StartInfo(entry,dataRoot,root))){}
            for(int i=0;i<160;++i) {
                await Task.Delay(250);
                foreach(var game in Process.GetProcessesByName("th14"))using(game)if(Matches(game,engine,dataRoot)) {
                    await Load(root,game.Id);note("已启动。");return;
                }
            }
            throw new IOException("未找到本次独立存档的辉针城实例。");
        }
    }
}
