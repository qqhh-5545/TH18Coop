using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
namespace TH18LocalCoop {
    internal sealed class Th14OnlineGame : IOnlineGame {
        const string ChineseHash="ec04fc884f658eee5096a3cff97688634216e23a52e29e77e0c272d5c069b2af";
        readonly string entry,modRoot,engine;
        Process game;
        NetworkLog launchLog;Process entryProcess;int exitLogged,entryExitLogged;
        void Trace(string text){var log=launchLog;if(log!=null)try {log.Write(text);}catch { }}
        void RecordExit() {
            try {
                if(game!=null && game.HasExited && Interlocked.Exchange(ref exitLogged,1)==0)
                    Trace("CORE_EXIT pid="+game.Id+" exit=0x"+unchecked((uint)game.ExitCode).ToString("x8"));
            } catch(InvalidOperationException){ }catch(System.ComponentModel.Win32Exception){ }
        }
        internal Th14OnlineGame(string entryPath,string root) {
            entry=GameCatalog.TH14.Validate(entryPath);modRoot=root;engine=Path.Combine(Path.GetDirectoryName(entry),"th14.exe");
            string expected=String.Equals(Path.GetFileName(entry),"th14c.exe",StringComparison.OrdinalIgnoreCase)?ChineseHash:GameCatalog.TH14.CoreHash;
            if(NetIdentity.Hash(File.ReadAllBytes(entry))!=expected)throw new IOException("游戏启动入口版本不匹配。");
        }
        internal static void RequireNoRunningGame(string gameRoot) {
            string engine=Path.Combine(gameRoot,"th14.exe");
            foreach(var process in Process.GetProcessesByName("th14"))using(process) {
                try {
                    if(!process.HasExited && String.Equals(process.MainModule.FileName,engine,StringComparison.OrdinalIgnoreCase))
                        throw new IOException("请先退出这个目录中已打开的原版或旧版游戏，再启动新的联机或录像。");
                } catch(System.ComponentModel.Win32Exception){ }
            }
        }
        bool Match(Process candidate,string dataRoot) {
            return Th14Launcher.Matches(candidate,engine,dataRoot);
        }
        void Loader(string arguments) {
            var info=new ProcessStartInfo(Path.Combine(modRoot,"TH14Coop.Console.exe"),arguments) {
                UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=modRoot};
            using(var process=Process.Start(info)) {
                var output=process.StandardOutput.ReadToEndAsync();var error=process.StandardError.ReadToEndAsync();
                if(!process.WaitForExit(15000)){process.Kill();throw new IOException("扩展加载超时。");}
                if(process.ExitCode!=0)throw new IOException(output.Result+error.Result);
            }
        }
        public void Start(OnlineProfile profile,OnlineBridge bridge,CancellationToken cancel) {
            launchLog=new NetworkLog(Path.GetDirectoryName(entry),profile.ReplayOnly?"replay":profile.IsHost?"launch-p1":"launch-p2");
            var info=Th14Launcher.StartInfo(entry,profile.DataRoot,modRoot);
            info.EnvironmentVariables["TH14_COOP_REPLAY_IDENTITY"]=NetIdentity.Load(engine,modRoot).Encode();
            if(bridge!=null)info.EnvironmentVariables["TH14_COOP_ONLINE"]=bridge.Token;
            else {info.EnvironmentVariables.Remove("TH14_COOP_ONLINE");if(!profile.ReplayOnly)throw new IOException("缺少联机连接。");info.EnvironmentVariables["TH14_COOP_REPLAY_VIEWER"]="1";}
            Trace("START entry="+entry+" arguments="+info.Arguments+" profile="+profile.ProfileDirectory);
            entryProcess=Process.Start(info);Trace("ENTRY_PID="+entryProcess.Id);
            for(int i=0;i<480 && game==null;++i) {
                cancel.ThrowIfCancellationRequested();
                if(entryProcess.HasExited && Interlocked.Exchange(ref entryExitLogged,1)==0)
                    Trace("ENTRY_EXIT pid="+entryProcess.Id+" exit=0x"+unchecked((uint)entryProcess.ExitCode).ToString("x8"));
                foreach(var candidate in Process.GetProcessesByName("th14")) {
                    if(Match(candidate,profile.DataRoot)){game=candidate;break;}candidate.Dispose();
                }
                if(game==null)Thread.Sleep(250);
            }
            if(game==null)throw new IOException("未找到本次在线会话的游戏，请查看日志并退出本次游戏后重试。");
            Trace("CORE_FOUND pid="+game.Id);game.Exited+=delegate {RecordExit();};game.EnableRaisingEvents=true;
            Loader("--attach "+game.Id);
            Trace("COOP_ATTACHED pid="+game.Id);
        }
        public bool Exited {get {bool exited=game!=null && game.HasExited;if(exited){RecordExit();if(game.ExitCode!=0)throw new IOException("游戏异常退出，代码 0x"+unchecked((uint)game.ExitCode).ToString("x8")+"；本次进度不会提交。");}return exited;}}
        public void Finish(bool saveAllowed,OnlineProfile profile,Action<string> note) {
            if(game==null){Trace("STARTUP_FAILED before a matching core was found");if(entryProcess!=null)entryProcess.Dispose();if(launchLog!=null)launchLog.Dispose();return;}
            Process own=game;
            int finished=0;
            Action complete=()=> {
                if(Interlocked.Exchange(ref finished,1)!=0)return;
                try {
                    RecordExit();
                    if(saveAllowed && own.ExitCode==0 && profile.IsHost){profile.CommitHost();note("本次主机进度已保存。客机长期档保持原样。");}
                    else note("本次会话文件保留在："+profile.ProfileDirectory);
                } catch(Exception error){note("保存进度："+error.Message);}
                finally {own.Dispose();if(entryProcess!=null)entryProcess.Dispose();if(launchLog!=null)launchLog.Dispose();}
            };
            if(own.HasExited)complete();
            else {own.Exited+=delegate {complete();};own.EnableRaisingEvents=true;if(own.HasExited)complete();}
        }
    }
}
