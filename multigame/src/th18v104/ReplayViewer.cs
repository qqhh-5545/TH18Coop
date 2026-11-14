using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace TH18LocalCoop.V104 {
    internal static class ReplayViewer {
        const int Limit=128*1024*1024;
        internal static bool Compatible(string path,string identity) {
            using(var file=File.Open(path,FileMode.Open,FileAccess.Read,FileShare.Read)) {
                if(file.Length<332 || file.Length>Limit)return false;
                var reader=new BinaryReader(file);
                if(reader.ReadUInt32()!=0x72383174)return false;
                file.Position=12;uint nativeEnd=reader.ReadUInt32();
                file.Position=file.Length-16;
                if(Encoding.ASCII.GetString(reader.ReadBytes(8))!="T18COOP1")return false;
                uint size=reader.ReadUInt32(),hash=reader.ReadUInt32();
                if(size<280 || size>file.Length-52 || nativeEnd!=file.Length-16-size)return false;
                file.Position=nativeEnd;byte[] body=reader.ReadBytes(280);
                if(body.Length!=280 || Encoding.ASCII.GetString(body,0,8)!="T18COOP1" || BitConverter.ToUInt32(body,8)!=1)return false;
                uint players=BitConverter.ToUInt32(body,12);if(players<2 || players>3)return false;
                int end=Array.IndexOf(body,(byte)0,24,224);
                if(end<24 || Encoding.ASCII.GetString(body,24,end-24)!=identity)return false;
                file.Position=nativeEnd;uint actual=2166136261,left=size;var buffer=new byte[65536];
                unchecked {while(left>0){int got=file.Read(buffer,0,(int)Math.Min(left,(uint)buffer.Length));if(got==0)return false;for(int i=0;i<got;++i)actual=(actual^buffer[i])*16777619;left-=(uint)got;}}
                return actual==hash;
            }
        }
        internal static void Run(string entry,string modRoot,Action<string> note,bool testWindow=false) {
            entry=GameSelection.Validate(entry);string gameRoot=Path.GetDirectoryName(entry);
            OnlineGame.RequireNoRunningGame(gameRoot);
            string archive=Path.Combine(gameRoot,"coop-data","replay");
            string identity=NetIdentity.Load(Path.Combine(gameRoot,"th18.exe"),modRoot).Encode();
            string[] files=Directory.Exists(archive)?Directory.GetFiles(archive,"*.rpy").OrderByDescending(File.GetLastWriteTimeUtc).ToArray():new string[0];
            var accepted=files.Where(file=>{try{return Compatible(file,identity);}catch(IOException){return false;}catch(UnauthorizedAccessException){return false;}}).Take(75).ToArray();
            if(accepted.Length==0)throw new IOException("没有可播放的多人录像。请先用此版本完成或结束一局；旧版输入日志不能播放。");
            var profile=new OnlineProfile(gameRoot,Guid.NewGuid().ToString("N"),0,new byte[0],Environment.GetEnvironmentVariable("APPDATA"),replayOnly:true);
            if(testWindow)profile.UseLocalTestWindow();
            string target=Path.Combine(profile.ProfileDirectory,"replay");
            for(int i=0;i<accepted.Length;++i)File.Copy(accepted[i],Path.Combine(target,i<25?"th18_"+(i+1).ToString("00")+".rpy":"th18_ud"+(i-25).ToString("0000")+".rpy"));
            var game=new OnlineGame(entry,modRoot);bool completed=false;
            try {
                note("正在打开录像…");game.Start(profile,null,CancellationToken.None);
                note("录像模式 · "+accepted.Length+"份录像");
                while(!game.Exited)Thread.Sleep(250);
                if(File.Exists(Path.Combine(profile.ProfileDirectory,"replay-error.txt")))throw new IOException("录像数据校验未通过，播放已停止；详细原因见 coop.log，原录像已保留。");
                completed=true;
                note("录像已关闭。");
            } finally {
                game.Finish(false,profile,delegate(string unused){});
                // This newly created GUID/viewer subtree is never the archive,
                // online session, permanent score or original APPDATA.
                string scope=Path.GetFullPath(Path.Combine(gameRoot,"coop-data","replay-sessions"))+Path.DirectorySeparatorChar;
                string temporary=Path.GetFullPath(profile.DataRoot);
                if(completed && profile.ReplayOnly && temporary.StartsWith(scope,StringComparison.OrdinalIgnoreCase) && Path.GetFileName(temporary)=="viewer") {
                    try {Directory.Delete(temporary,true);Directory.Delete(Path.GetDirectoryName(temporary),false);}catch(IOException){}catch(UnauthorizedAccessException){}
                }
            }
        }
    }
}
