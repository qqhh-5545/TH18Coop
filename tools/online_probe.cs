using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using TH18LocalCoop;

internal static class OnlineProbe {
    sealed class FixtureGame : IOnlineGame {
        readonly string native;
        readonly bool diverge;
        Process process;Task<string> output,error;
        internal string Evidence;
        internal string ProfilePath;
        internal bool SharedKeyboard;
        internal FixtureGame(string path,bool bad=false){native=path;diverge=bad;}
        public void Start(OnlineProfile profile,OnlineBridge bridge,CancellationToken cancel) {
            ProfilePath=profile.ProfileDirectory;
            SharedKeyboard=bridge.SharedKeyboard;
            var info=CoopData.StartInfo(native,profile.DataRoot);info.Arguments="--online-fixture";
            info.CreateNoWindow=true;info.RedirectStandardOutput=true;info.RedirectStandardError=true;
            info.StandardOutputEncoding=Encoding.UTF8;info.StandardErrorEncoding=Encoding.UTF8;
            info.EnvironmentVariables["TH18_COOP_ONLINE"]=bridge.Token;
            info.EnvironmentVariables["TH18_COOP_TEST_MUTEX"]="Local\\TH18CoopFixture-"+Directory.GetParent(profile.DataRoot).Name;
            if(diverge)info.EnvironmentVariables["TH18_COOP_TEST_DIVERGE"]="1";
            process=Process.Start(info);output=process.StandardOutput.ReadToEndAsync();error=process.StandardError.ReadToEndAsync();
        }
        public bool Exited {get{return process!=null && process.HasExited;}}
        public void Finish(bool saveAllowed,OnlineProfile profile,Action<string> note) {
            if(process==null)return;
            try {
                if(!process.WaitForExit(5000)){process.Kill();throw new Exception("Native fixture did not exit");}
                Evidence=output.Result+error.Result;note(Evidence.Trim());
                if(!saveAllowed && process.ExitCode!=0)return;
                if(process.ExitCode!=0 || !Evidence.Contains("ONLINE_NATIVE_PASS"))throw new Exception("Native fixture failed: "+Evidence);
                if(saveAllowed)profile.CommitHost();
            } finally {process.Dispose();}
        }
    }
    static NetIdentity Identity(){return new NetIdentity {Core=new string('a',64),Mod=new string('b',64),Rules=new string('c',64)};}
    static byte[] Score(byte marker){var data=new byte[24];data[0]=(byte)'T';data[1]=(byte)'H';data[2]=(byte)'8';data[3]=(byte)'1';data[23]=marker;return data;}
    static string Permanent(string root){return Path.Combine(CoopData.ProfileDirectory(Path.Combine(root,"coop-data")),"scoreth18.dat");}
    static void Put(string root,byte marker){string file=Permanent(root);Directory.CreateDirectory(Path.GetDirectoryName(file));File.WriteAllBytes(file,Score(marker));}
    static void Assert(bool value,string message){if(!value)throw new Exception(message);}
    internal static int Guest(string gameRoot,int port,string native,bool diverge=false) {
        FixtureGame game=null;
        using(var session=new OnlineSession(Path.Combine(gameRoot,"th18.exe"),gameRoot,role=>game=new FixtureGame(native,diverge),Identity(),Path.Combine(gameRoot,"settings"))) {
            session.Join("127.0.0.1",port,"1234ABCD",Console.WriteLine);
            Assert(session.Completed==120,"Guest did not complete 120 steps");
            Console.WriteLine("ONLINE_GUEST_PASS steps="+session.Completed);
        }
        return 0;
    }
    static void DesyncTest(string native,string run,Action<string> note) {
        string hostRoot=Path.Combine(run,"desync-host"),guestRoot=Path.Combine(run,"desync-guest");Put(hostRoot,1);Put(guestRoot,99);
        using(var host=new OnlineSession(Path.Combine(hostRoot,"th18.exe"),hostRoot,role=>new FixtureGame(native),Identity(),Path.Combine(run,"settings"))) {
            int port=host.Listen(IPAddress.Loopback,0);
            var server=Task.Run(()=>host.Host("1234ABCD",note));
            var info=new ProcessStartInfo(Assembly.GetExecutingAssembly().Location,"--online-guest \""+guestRoot+"\" "+port+" \""+native+"\" diverge") {
                UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,
                StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8};
            using(var guest=Process.Start(info)) {
                var output=guest.StandardOutput.ReadToEndAsync();var errors=guest.StandardError.ReadToEndAsync();
                try {
                    bool rejected=false;
                    try {Assert(server.Wait(40000),"Desync host timeout");}
                    catch(AggregateException error){rejected=error.GetBaseException().Message.Contains("ONLINE_DESYNC");}
                    Assert(guest.WaitForExit(10000),"Desync guest timeout");
                    Assert(rejected && guest.ExitCode!=0,"Divergence did not stop both endpoints");
                    Assert(File.ReadAllBytes(Permanent(hostRoot))[23]==1 && File.ReadAllBytes(Permanent(guestRoot))[23]==99,"Failed session committed a permanent score");
                    note(output.Result.Trim());note(errors.Result.Trim());
                    note("PASS deliberate state divergence froze both native fixtures and preserved both permanent scores.");
                } finally {host.Dispose();if(!guest.HasExited){guest.Kill();guest.WaitForExit(2000);}}
            }
        }
    }
    static void DuoTest(string native,string run,Action<string> note,int players=2) {
        string root=Path.Combine(run,players==3?"local-trio":"local-duo"),settings=Path.Combine(run,players==3?"trio-settings":"duo-settings");Put(root,7);
        string config=Path.Combine(CoopData.ProfileDirectory(settings),"th18.cfg");Directory.CreateDirectory(Path.GetDirectoryName(config));
        byte[] original=new byte[136];Array.Copy(BitConverter.GetBytes(0x180002u),original,4);original[0x47]=5;original[0x51]=1;File.WriteAllBytes(config,original);
        var games=new FixtureGame[players];
        using(var duo=new LocalDuo(Path.Combine(root,"th18.exe"),root,role=>games[role]=new FixtureGame(native),Identity(),settings,players:players))
            Assert(duo.Run(note).Wait(40000),"Local dual endpoint test timeout");
        Assert(File.ReadAllBytes(Permanent(root))[23]==7,"Local test committed permanent progress");
        Assert(NetIdentity.Hash(File.ReadAllBytes(config))==NetIdentity.Hash(original),"Local test changed original display settings");
        foreach(var game in games) {
            Assert(game!=null && game.Evidence.Contains("ONLINE_NATIVE_PASS"),"Local duo native fixture failed");
            Assert(!game.SharedKeyboard,"Default local dual-endpoint test must preview native remote controls");
            Assert(game.Evidence.Contains("async_loads=2"),"Local duo did not complete both asynchronous scene loads");
            Assert(game.Evidence.Contains("menu_loads=1"),"Local duo did not return through the independent menu loader");
            byte[] copy=File.ReadAllBytes(Path.Combine(game.ProfilePath,"th18.cfg"));
            Assert(copy[0x47]==9 && (copy[0x51]&1)==0,"Session was not configured as 640x480 without startup prompt");
        }
        Assert(Regex.Match(games[0].Evidence,@"state=(\d+)").Value==Regex.Match(games[1].Evidence,@"state=(\d+)").Value,"Local duo final states differ");
        if(players==3) {
            Assert(Regex.Match(games[0].Evidence,@"state=(\d+)").Value==Regex.Match(games[2].Evidence,@"state=(\d+)").Value,"P3 state diverged");
            Assert(games[1].ProfilePath!=games[2].ProfilePath && games[2].ProfilePath.Contains("guest-p3"),"P2/P3 profiles overlap");
        }
        note("PASS one-click local "+players+" endpoints: serialized startup, owned fixture-mutex release, 640x480 session-only settings, original config and permanent progress preserved.");
    }
    internal static int SelfTest(string native,string gameRoot) {
        native=Path.GetFullPath(native);gameRoot=Path.GetFullPath(gameRoot);
        using(var log=new NetworkLog(gameRoot,"online-selftest")) {
            Action<string> note=s=>{Console.WriteLine(s);log.Write(s);};
            note("Online end-to-end diagnostic: real transport + native bridge, simulated engine; no original game launched.");
            using(var shared=new OnlineBridge(0,1,true,true))Assert(shared.SharedKeyboard,"Explicit legacy keyboard test must retain IJKL");
            Assert(IPAddress.IsLoopback(OnlineSession.ResolveHost("localhost",CancellationToken.None)),"Sakura node hostname must resolve through the online path");
            bool badAddress=false;try {OnlineSession.ResolveHost("https://example.com:28180/",CancellationToken.None);}catch(IOException){badAddress=true;}
            Assert(badAddress,"Address field must reject URLs rather than silently use an unrelated endpoint");
            var diagnostic=new OnlineRequest {Kind=1,Digest=1};
            for(int i=0;i<9;++i)diagnostic.Detail[i]=UInt32.MaxValue-(uint)i;
            var decoded=OnlineRequest.Decode(diagnostic.Encode());
            Assert(decoded.Detail[8]==UInt32.MaxValue-8 && diagnostic.Encode().Length<NetWire.Limit,"Bounded detail wire roundtrip");
            decoded.Digest=2;decoded.Detail[5]=0;
            bool detailRejected=false;
            try {OnlineReply.Merge(diagnostic,decoded);}catch(IOException error){detailRejected=error.Message.Contains("field=p1") && error.Message.Contains("host_detail=") && error.Message.Length<NetWire.Limit-6;}
            Assert(detailRejected,"Desync details must identify the first differing group without exceeding wire bound");
            note("PASS bounded diagnostic detail roundtrip and first differing state-group report; mismatch still stops play.");
            string run=Path.Combine(gameRoot,"coop-data","selftest",Guid.NewGuid().ToString("N"));
            string hostRoot=Path.Combine(run,"host"),guestRoot=Path.Combine(run,"guest");Put(hostRoot,1);Put(guestRoot,99);
            // Explicitly verify that initial snapshot/profile logic cannot overwrite
            // a concurrently changed permanent score.
            var conflict=new OnlineProfile(hostRoot,Guid.NewGuid().ToString("N"),0,Score(1),Path.Combine(run,"settings"));
            Put(hostRoot,2);bool protectedScore=false;
            try {conflict.CommitHost();}catch(IOException){protectedScore=true;}
            Assert(protectedScore && File.ReadAllBytes(Permanent(hostRoot))[23]==2,"Score conflict protection failed");Put(hostRoot,1);
            bool rejected=false;
            try {OnlineReply.Merge(new OnlineRequest {Kind=1,Digest=1},new OnlineRequest {Kind=1,Digest=2});}catch(IOException){rejected=true;}
            Assert(rejected,"Diverged state must never advance");
            FixtureGame hostGame=null;
            using(var host=new OnlineSession(Path.Combine(hostRoot,"th18.exe"),hostRoot,role=>hostGame=new FixtureGame(native),Identity(),Path.Combine(run,"settings"))) {
                int port=host.Listen(IPAddress.Loopback,0);
                var server=Task.Run(()=>host.Host("1234ABCD",note));
                var info=new ProcessStartInfo(Assembly.GetExecutingAssembly().Location,"--online-guest \""+guestRoot+"\" "+port+" \""+native+"\"") {
                    UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,
                    StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8};
                using(var guest=Process.Start(info)) {
                    var output=guest.StandardOutput.ReadToEndAsync();var errors=guest.StandardError.ReadToEndAsync();
                    try {
                        Assert(server.Wait(40000),"Online host test timeout");
                        Assert(guest.WaitForExit(10000),"Online guest test timeout");
                        string report=output.Result+errors.Result;note(report.Trim());
                        Assert(guest.ExitCode==0 && report.Contains("ONLINE_GUEST_PASS"),"Online guest failed: "+report);
                        Assert(hostGame.Evidence.Contains("async_loads=2") && report.Contains("async_loads=2"),"Asynchronous load barrier was not exercised");
                        Assert(hostGame.Evidence.Contains("menu_loads=1") && report.Contains("menu_loads=1"),"Independent return-to-title job was not exercised");
                        Assert(!hostGame.SharedKeyboard,"Loopback/forwarded production connections must keep native online controls");
                        Assert(host.Completed==120,"Host did not complete 120 steps");
                        string hostState=Regex.Match(hostGame.Evidence,@"state=(\d+)").Groups[1].Value;
                        string guestState=Regex.Match(report,@"state=(\d+)").Groups[1].Value;
                        Assert(hostState.Length>0 && hostState==guestState,"Native final state hashes differ");
                        Assert(File.ReadAllBytes(Permanent(hostRoot))[23]==42,"Host progress was not committed");
                        Assert(File.ReadAllBytes(Permanent(guestRoot))[23]==99,"Guest permanent progress was overwritten");
                        note("PASS online end-to-end: two network processes + two native mock-engine processes; 120 pre/post steps, equal final state="+hostState+", asset-load skew, host-save commit, guest-save isolation, conflict/desync rejection.");
                        note("日志："+log.FilePath);
                    } finally {host.Dispose();if(!guest.HasExited){guest.Kill();guest.WaitForExit(2000);}}
                }
            }
            DesyncTest(native,run,note);
            DuoTest(native,run,note);
            DuoTest(native,run,note,3);
            return 0;
        }
    }
}
