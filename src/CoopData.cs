using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Security.Cryptography;

namespace TH18LocalCoop {
    internal static class GameSelection {
        internal const string CoreHash="5aeb74b19939a29a8cf06e4fe7777aaf9139e93478507e039f43318171020e9f";
        internal const string ChineseHash="8c1dc7ebf7aab0c8962b2a71cb848d59ef180e1f0613a3c410ca043767044c5b";
        internal const string SteamCoreHash="9ed66e6952459e81515c17a671410bee7014a83e3c6cc6a7e360e7b4904c62f4";
        internal static bool SupportedCore(string digest) {return digest==CoreHash || digest==SteamCoreHash;}
        internal static string CanonicalCore(string digest) {if(!SupportedCore(digest))throw new IOException("游戏核心版本不匹配。");return CoreHash;}
        internal static bool IsSteam(string directory) {
            string full=Path.GetFullPath(directory).TrimEnd('\\','/')+Path.DirectorySeparatorChar;
            return full.IndexOf("\\steamapps\\",StringComparison.OrdinalIgnoreCase)>=0 || File.Exists(Path.Combine(full,"steam_appid.txt")) || File.Exists(Path.Combine(full,"steam_api.dll")) || File.Exists(Path.Combine(full,"steam_api64.dll"));
        }
        internal static string Digest(string file) {using(var sha=SHA256.Create())using(var stream=File.OpenRead(file))return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","").ToLowerInvariant();}
        internal static string Validate(string entry) {
            if(String.IsNullOrWhiteSpace(entry))throw new IOException("请先选择虹龙洞游戏目录。");
            entry=Path.GetFullPath(entry.Trim());string name=Path.GetFileName(entry),directory=Path.GetDirectoryName(entry);
            if(!String.Equals(name,"th18.exe",StringComparison.OrdinalIgnoreCase) && !String.Equals(name,"th18c.exe",StringComparison.OrdinalIgnoreCase))throw new IOException("请选择 th18.exe 或 th18c.exe。");
            if(entry.Length>=170)throw new IOException("游戏路径过长，请移至较短目录，以便保存联机会话与录像。");
            if(Encoding.Default.GetString(Encoding.Default.GetBytes(directory))!=directory)throw new IOException("原版游戏不能识别此路径的部分字符，请使用当前系统可表示的目录名。");
            string core=Path.Combine(directory,"th18.exe");
            if(!File.Exists(core) || !SupportedCore(Digest(core)))throw new IOException("游戏核心版本不匹配，请选择受支持的虹龙洞目录。");
            if(!File.Exists(entry) || (String.Equals(name,"th18c.exe",StringComparison.OrdinalIgnoreCase)?Digest(entry)!=ChineseHash:!SupportedCore(Digest(entry))))throw new IOException("汉化 / 日文启动入口版本不匹配。");
            return entry;
        }
    }
    internal static class CoopData {
        internal static string ProfileDirectory(string root) {
            return Path.Combine(root,"ShanghaiAlice","th18");
        }
        internal static void SyncLocalConfig(string localAppData,string dataRoot) {
            if(String.IsNullOrEmpty(localAppData))return;
            string source=Path.Combine(ProfileDirectory(localAppData),"th18.cfg");
            string target=Path.Combine(ProfileDirectory(dataRoot),"th18.cfg");
            if(!File.Exists(source) || String.Equals(Path.GetFullPath(source),Path.GetFullPath(target),StringComparison.OrdinalIgnoreCase))return;
            // custom.exe remains the settings editor. Only its config is imported;
            // scores, unlocks, replays and the parent's environment stay separate.
            byte[] settings=File.ReadAllBytes(source);
            if(File.Exists(target)) {
                byte[] current=File.ReadAllBytes(target);
                bool same=current.Length==settings.Length;
                for(int i=0;same && i<current.Length;++i)same=current[i]==settings[i];
                if(same)return;
                string backup=target+".before-local-sync";
                if(!File.Exists(backup))File.Copy(target,backup);
            }
            Directory.CreateDirectory(Path.GetDirectoryName(target));
            string pending=target+"."+Guid.NewGuid().ToString("N")+".tmp";
            try {
                File.WriteAllBytes(pending,settings);
                if(File.Exists(target))File.Replace(pending,target,null);
                else File.Move(pending,target);
            } finally {if(File.Exists(pending))File.Delete(pending);}
        }
        internal static ProcessStartInfo StartInfo(string entry,string dataRoot) {
            var info=new ProcessStartInfo(entry) {UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(entry)};
            // A child-only environment: the original game resolves APPDATA
            // before loading scores, config or replay metadata.
            info.EnvironmentVariables["APPDATA"]=Path.GetFullPath(dataRoot);
            // Verified th18c WinMain accepts argv[1] as the game application
            // path and takes its direct create/inject branch instead of the GUI.
            if(String.Equals(Path.GetFileName(entry),"th18c.exe",StringComparison.OrdinalIgnoreCase))
                // th18c calls CreateProcessW(application, NULL, ...). A simple
                // sibling name avoids an unquoted child command line when the
                // game directory contains spaces; WorkingDirectory is exact.
                info.Arguments="th18.exe";
            return info;
        }
        internal static bool MatchesNativePath(byte[] bytes,string dataRoot) {
            int end=Array.IndexOf(bytes,(byte)0);if(end<0)return false;
            string actual=Encoding.Default.GetString(bytes,0,end).TrimEnd('\\','/');
            string expected=ProfileDirectory(Path.GetFullPath(dataRoot)).TrimEnd('\\','/');
            return String.Equals(actual,expected,StringComparison.OrdinalIgnoreCase);
        }
    }
}
