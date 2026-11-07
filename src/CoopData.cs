using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Security.Cryptography;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using Microsoft.Win32;

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
    internal sealed class ThcrapSettings {
        internal string Mode="Auto",Loader="",Config="";
        internal static ThcrapSettings Load(string root) {
            var result=new ThcrapSettings();if(String.IsNullOrEmpty(root))return result;
            string path=Path.Combine(root,"thcrap.ini");if(!File.Exists(path))return result;
            foreach(string line in File.ReadAllLines(path)){int split=line.IndexOf('=');if(split<0)continue;string key=line.Substring(0,split).Trim(),value=line.Substring(split+1).Trim();if(key=="Mode")result.Mode=value;else if(key=="Loader")result.Loader=value;else if(key=="Config")result.Config=value;}
            if(result.Mode!="Auto" && result.Mode!="Off" && result.Mode!="Custom")throw new IOException("thcrap.ini 的 Mode 无效。");return result;
        }
        internal void Save(string root) {
            foreach(string value in new[]{Mode,Loader,Config})if(value.IndexOfAny(new[]{'\r','\n'})>=0)throw new IOException("路径不能包含换行。");
            string path=Path.Combine(root,"thcrap.ini"),pending=path+".tmp";
            File.WriteAllText(pending,"[Thcrap]\nMode="+Mode+"\nLoader="+Loader+"\nConfig="+Config+"\n",new UTF8Encoding(false));
            if(File.Exists(path))File.Replace(pending,path,null);else File.Move(pending,path);
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
        // No Steam/global environment or launch-option edits: keep all overrides child-only.
        internal static ProcessStartInfo ThcrapInfo(string entry,string dataRoot,string options) {
            string[] args=WindowsArgs(options);
            if(args.Length!=3 || !String.Equals(Path.GetFileName(args[0]),"thcrap_loader.exe",StringComparison.OrdinalIgnoreCase) ||
               !String.Equals(args[2],"%command%",StringComparison.OrdinalIgnoreCase) || !Path.IsPathRooted(args[0]) || !File.Exists(args[0]))
                throw new IOException("Steam 启动选项不是受支持的 thcrap 格式：需要 thcrap_loader.exe、配置文件、%command%。");
            string loader=Path.GetFullPath(args[0]),directory=Path.GetDirectoryName(loader);
            string config=Path.IsPathRooted(args[1])?args[1]:Path.Combine(directory,args[1]);
            if(!File.Exists(config) && !Path.IsPathRooted(args[1]))config=Path.Combine(directory,"config",args[1]);
            if(!File.Exists(config))throw new IOException("找不到 thcrap 配置："+args[1]);
            var info=new ProcessStartInfo(loader) {UseShellExecute=false,WorkingDirectory=directory,
                Arguments=QuoteArg(Path.GetFullPath(config))+" "+QuoteArg(Path.GetFullPath(entry))};
            info.EnvironmentVariables["APPDATA"]=Path.GetFullPath(dataRoot);
            return info;
        }
        internal static string QuoteArg(string value) {
            // Windows CRT quoting: backslashes must be doubled before a quote/end.
            var result=new StringBuilder("\"");int slashes=0;
            foreach(char c in value){if(c=='\\'){++slashes;continue;}if(c=='"'){result.Append('\\',slashes*2+1).Append(c);}else{result.Append('\\',slashes).Append(c);}slashes=0;}
            return result.Append('\\',slashes*2).Append('"').ToString();
        }
        [System.Runtime.InteropServices.DllImport("shell32.dll",CharSet=System.Runtime.InteropServices.CharSet.Unicode,SetLastError=true)]
        static extern IntPtr CommandLineToArgvW(string text,out int count);
        [System.Runtime.InteropServices.DllImport("kernel32.dll")]static extern IntPtr LocalFree(IntPtr memory);
        internal static string[] WindowsArgs(string text) {
            int count;IntPtr memory=CommandLineToArgvW(text,out count);if(memory==IntPtr.Zero)throw new IOException("无法解析启动选项。");
            try {var args=new string[count];for(int i=0;i<count;++i)args[i]=System.Runtime.InteropServices.Marshal.PtrToStringUni(System.Runtime.InteropServices.Marshal.ReadIntPtr(memory,i*IntPtr.Size));return args;}
            finally {LocalFree(memory);}
        }
        static Dictionary<string,object> ReadVdf(string text) {
            var tokens=new List<string>();
            foreach(Match m in Regex.Matches(text,@"""((?:\\.|[^""\\])*)""|[{}]|//[^\r\n]*")) {
                if(m.Value.StartsWith("//"))continue;
                tokens.Add(m.Value.StartsWith("\"")?Regex.Replace(m.Groups[1].Value,@"\\(.)",x=>x.Groups[1].Value):m.Value);
            }
            int index=0;return VdfObject(tokens,ref index);
        }
        static Dictionary<string,object> VdfObject(List<string> tokens,ref int index) {
            var result=new Dictionary<string,object>(StringComparer.OrdinalIgnoreCase);
            while(index<tokens.Count) {
                string key=tokens[index++];if(key=="}")break;if(index>=tokens.Count)break;
                string value=tokens[index++];result[key]=value=="{"?(object)VdfObject(tokens,ref index):value;
            }
            return result;
        }
        static object VdfGet(object root,params string[] keys) {
            foreach(string key in keys){var map=root as Dictionary<string,object>;if(map==null || !map.TryGetValue(key,out root))return null;}return root;
        }
        internal static string SteamOptions(string entry) {
            DirectoryInfo current=new DirectoryInfo(Path.GetDirectoryName(entry));
            while(current!=null && !String.Equals(current.Name,"steamapps",StringComparison.OrdinalIgnoreCase))current=current.Parent;
            if(current==null)return null;
            string manifest=Path.Combine(current.FullName,"appmanifest_1566410.acf");if(!File.Exists(manifest))return null;
            string owner=VdfGet(ReadVdf(File.ReadAllText(manifest)),"AppState","LastOwner") as string;ulong id;
            if(!UInt64.TryParse(owner,out id) || id<76561197960265728UL)return null;
            string account=(id-76561197960265728UL).ToString(System.Globalization.CultureInfo.InvariantCulture);
            var roots=new List<string>();if(current.Parent!=null)roots.Add(current.Parent.FullName);
            using(var key=Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam")) {
                string registered=key==null?null:key.GetValue("SteamPath") as string;if(!String.IsNullOrWhiteSpace(registered))roots.Add(registered);
            }
            foreach(string root in roots) {
                string config=Path.Combine(root,"userdata",account,"config","localconfig.vdf");if(!File.Exists(config))continue;
                return VdfGet(ReadVdf(File.ReadAllText(config)),"UserLocalConfigStore","Software","Valve","Steam","apps","1566410","LaunchOptions") as string;
            }
            return null;
        }
        internal static ProcessStartInfo StartInfo(string entry,string dataRoot,string modRoot=null) {
            bool steam=String.Equals(Path.GetFileName(entry),"th18.exe",StringComparison.OrdinalIgnoreCase) && File.Exists(entry) && GameSelection.Digest(entry)==GameSelection.SteamCoreHash;
            var settings=ThcrapSettings.Load(modRoot);
            string options=settings.Mode=="Custom"?QuoteArg(settings.Loader)+" "+QuoteArg(settings.Config)+" %command%":settings.Mode=="Off"?null:steam?SteamOptions(entry):null;
            var info=!String.IsNullOrWhiteSpace(options)?ThcrapInfo(entry,dataRoot,options):new ProcessStartInfo(entry) {UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(entry)};
            info.EnvironmentVariables["APPDATA"]=Path.GetFullPath(dataRoot);
            if(steam) {info.EnvironmentVariables["SteamAppId"]="1566410";info.EnvironmentVariables["SteamGameId"]="1566410";}
            if(String.Equals(Path.GetFileName(entry),"th18c.exe",StringComparison.OrdinalIgnoreCase))info.Arguments="th18.exe";
            return info;
        }
        internal static bool MatchesNativePath(byte[] bytes,string dataRoot) {
            int end=Array.IndexOf(bytes,(byte)0);if(end<0)return false;
            string actual=Encoding.Default.GetString(bytes,0,end).TrimEnd('\\','/');
            string expected=ProfileDirectory(Path.GetFullPath(dataRoot)).TrimEnd('\\','/');
            if(String.Equals(actual,expected,StringComparison.OrdinalIgnoreCase))return true;
            try {actual=new UTF8Encoding(false,true).GetString(bytes,0,end).TrimEnd('\\','/');return String.Equals(actual,expected,StringComparison.OrdinalIgnoreCase);}
            catch(DecoderFallbackException){return false;}
        }
    }
}
