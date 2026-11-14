using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using TH18LocalCoop.V104;

class NetProbe {
    static int assertions;
    static string stage="startup";
    static NetworkLog savedLog;
    static void Check(bool value,string message) {++assertions;if(!value)throw new Exception(message);}
    static NetIdentity Fixture() {
        return new NetIdentity {Core=new string('a',64),Mod=new string('b',64),Rules=new string('c',64)};
    }
    static void WireTests() {
        using(var bytes=new MemoryStream()) {
            NetWire.Send(bytes,new string('A',512));NetWire.Send(bytes,"NEXT");bytes.Position=0;
            Check(NetWire.Read(bytes)==new string('A',512),"maximum packet");
            Check(NetWire.Read(bytes)=="NEXT","coalesced packet boundary");
        }
        foreach(string malformed in new[]{new string('A',513)+"\n","BAD\r\n","NO-TERMINATOR","X\0\n"}) {
            bool rejected=false;
            try {using(var bytes=new MemoryStream(Encoding.ASCII.GetBytes(malformed)))NetWire.Read(bytes);}catch(IOException){rejected=true;}
            Check(rejected,"malformed bounded packet");
        }
        bool invalid=false;try {NetIdentity.Decode("x|short|short|short");}catch(IOException){invalid=true;}
        Check(invalid,"invalid identity");
        Check(!NetSession.ValidCode("") && !NetSession.ValidCode("NOT-HEX!") && NetSession.ValidCode(NetSession.NewCode()),"connection code validation");
        using(var cancel=new NetSession()) {
            cancel.Listen(IPAddress.Loopback,0);
            var waiting=Task.Run(()=>{try {cancel.Host(Fixture(),"1234ABCD",s=>{});}catch(SocketException){}catch(ObjectDisposedException){}});
            cancel.Dispose();Check(waiting.Wait(2000),"stop waiting listener");
        }
    }
    static void BadHello(int port,string message) {
        using(var raw=new TcpClient()) {
            raw.ReceiveTimeout=5000;raw.Connect(IPAddress.Loopback,port);
            // The over-limit case deliberately ends at the byte that triggers
            // rejection; don't leave an unread newline to cause a TCP reset.
            byte[] bytes=Encoding.ASCII.GetBytes(message+(message.Length>NetWire.Limit?"":"\n"));raw.GetStream().Write(bytes,0,bytes.Length);
            Check(NetWire.Read(raw.GetStream()).StartsWith("ERROR|"),"bad handshake rejection");
        }
    }
    static void DisconnectTests() {
        foreach(bool invalidSequence in new[]{false,true})using(var host=new NetSession()) {
            int port=host.Listen(IPAddress.Loopback,0);string failure=null;
            var task=Task.Run(()=>{try {host.Host(Fixture(),"1234ABCD",s=>{});}catch(IOException error){failure=error.Message;}});
            using(var peer=new TcpClient()) {
                peer.ReceiveTimeout=5000;peer.Connect(IPAddress.Loopback,port);
                var stream=peer.GetStream();NetWire.Send(stream,"TH18COOP|1|"+Fixture().Encode()+"|1234ABCD");
                Check(NetWire.Read(stream).StartsWith("WELCOME|"),"disconnect-test handshake");
                if(invalidSequence)NetWire.Send(stream,"PING|99|1");
            }
            Check(task.Wait(3000),"host detects lost or invalid peer");
            Check(failure==(invalidSequence?"SEQUENCE":"DISCONNECTED"),"disconnect or out-of-order probe detected");
        }
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();
        try {
            int port=((IPEndPoint)listener.LocalEndpoint).Port;
            var task=Task.Run(()=> {
                using(var peer=listener.AcceptTcpClient()) {
                    peer.ReceiveTimeout=5000;var stream=peer.GetStream();NetWire.Read(stream);
                    NetWire.Send(stream,"WELCOME|1|"+Fixture().Encode()+"|P2");NetWire.Read(stream);
                    NetWire.Send(stream,"PONG|999|123");
                }
            });
            bool refused=false;
            using(var client=new NetSession())try {client.Join("127.0.0.1",port,Fixture(),"1234ABCD",s=>{},1);}catch(IOException error){refused=error.Message=="SEQUENCE";}
            Check(refused,"client rejects corrupted acknowledgement");Check(task.Wait(3000),"corrupt-peer exit");
        } finally {listener.Stop();}
    }
    static void InputPeerTests() {
        using(var host=new NetSession()) {
            int port=host.Listen(IPAddress.Loopback,0);string failure=null;
            var task=Task.Run(()=>{try {host.Host(Fixture(),"1234ABCD",s=>{},InputTape.Fixture());}catch(IOException error){failure=error.Message;}});
            using(var peer=new TcpClient()) {
                peer.ReceiveTimeout=5000;peer.Connect(IPAddress.Loopback,port);var stream=peer.GetStream();
                NetWire.Send(stream,"TH18COOP|1|"+Fixture().Encode()+"|1234ABCD");NetWire.Read(stream);
                NetWire.Send(stream,"INPUT|0|3323");string reply=NetWire.Read(stream);
                Check(reply.StartsWith("FRAME|"),"input merge reply");
                InputFrame merged=InputFrame.Decode(reply.Substring(6));
                Check(merged.P1==0 && merged.P2==0xcfb && merged.Sequence==0,"host P1 and remote P2 ownership");
                NetWire.Send(stream,"INPUT|0|0");
            }
            Check(task.Wait(3000) && failure=="SEQUENCE","duplicate input frame rejected");
        }
    }
    static void TapeFailureTests(string inputPath) {
        byte[] original=File.ReadAllBytes(inputPath);
        string temporary=inputPath+"."+Guid.NewGuid().ToString("N")+".bad";
        try {
            foreach(int variant in new[]{0,1,2}) {
                byte[] bytes=(byte[])original.Clone();
                if(variant==0)Array.Resize(ref bytes,bytes.Length-1);
                if(variant==1)bytes[60]^=1; // First record checksum.
                if(variant==2)bytes[0]^=1;
                File.WriteAllBytes(temporary,bytes);bool rejected=false;
                try {InputTape.Read(temporary);}catch(IOException){rejected=true;}
                Check(rejected,"truncated/corrupt/header native tape rejection");
            }
        } finally {if(File.Exists(temporary))File.Delete(temporary);}
    }
    static void SelfTest(NetIdentity identity,string inputPath=null) {
        stage="wire";WireTests();stage="disconnect";DisconnectTests();stage="input-peer";InputPeerTests();stage="host-start";
        InputTape tape=inputPath==null?InputTape.Fixture():InputTape.Read(inputPath);
        if(inputPath!=null)TapeFailureTests(inputPath);
        Check(tape.Build==identity.Build && tape.Frames.Count==240,"input tape build/count");
        Check(tape.Frames[40].Owner==2 && tape.Frames[80].Stage==1,"native fixture phase/stage boundaries");
        string encodedFrame=tape.Frames[17].Encode();
        Check(InputFrame.Decode(encodedFrame).Encode()==encodedFrame,"input frame wire roundtrip");
        bool corrupted=false;try {InputFrame.Decode(encodedFrame.Substring(0,encodedFrame.LastIndexOf('|')+1)+"0");}catch(IOException){corrupted=true;}
        Check(corrupted,"corrupted input checksum");
        string code=NetSession.NewCode();
        string encoded=Convert.ToBase64String(Encoding.ASCII.GetBytes(identity.Encode()));
        var info=new ProcessStartInfo(Assembly.GetExecutingAssembly().Location,"--host-test "+code+" "+encoded+(inputPath==null?"":" \""+Path.GetFullPath(inputPath)+"\"")) {
            UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,
            StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8};
        using(var host=Process.Start(info)) {
            var errors=host.StandardError.ReadToEndAsync();
            try {
                var ready=host.StandardOutput.ReadLineAsync();
                Check(ready.Wait(5000) && ready.Result!=null && ready.Result.StartsWith("PORT:"),"background host ready");
                int port=Int32.Parse(ready.Result.Substring(5));
                var remainder=host.StandardOutput.ReadToEndAsync();
                foreach(string reason in new[]{"CODE","VERSION","CORE","MOD","RULES"}) {
                    stage="reject-"+reason;
                    var other=NetIdentity.Decode(identity.Encode());string token=code;
                    if(reason=="CODE")token=code=="00000000"?"FFFFFFFF":"00000000";
                    if(reason=="VERSION")other.Build="mismatch";
                    if(reason=="CORE")other.Core=new string(identity.Core[0]=='f'?'e':'f',64);
                    if(reason=="MOD")other.Mod=new string(identity.Mod[0]=='f'?'e':'f',64);
                    if(reason=="RULES")other.Rules=new string(identity.Rules[0]=='f'?'e':'f',64);
                    bool refused=false;
                    using(var client=new NetSession())try {client.Join("127.0.0.1",port,other,token,s=>{},1);}catch(IOException error){refused=error.Message==reason;}
                    Check(refused,"compatibility rejection "+reason);
                }
                stage="bad-protocol";BadHello(port,"TH18COOP|99|broken");
                stage="oversized-message";BadHello(port,new string('X',513));
                stage="round-trip";
                using(var client=new NetSession()) {
                    client.Join("127.0.0.1",port,identity,code,s=>{},240,tape);
                    Check(client.Messages==240,"240 ordered frames combining host P1 and guest P2");
                }
                Check(host.WaitForExit(5000),"clean host exit");
                string output=remainder.Result;string errorText=errors.Result;
                Check(host.ExitCode==0 && output.Contains("HOST-PASS:240:7"),"host counters and clean disconnect: "+output+errorText);
            } finally {if(!host.HasExited){host.Kill();host.WaitForExit(2000);}}
        }
        string summary="PASS "+assertions+" network/input checks: separate host/client processes, 240 merged dual-input frames, frame checksums, version/core/mod/rules/code rejection, malformed packets, listener cancellation and clean disconnect.";
        string result="单机输入传输自测通过：双进程握手、240 帧双人输入合并及异常处理通过。此结果不代表原版游戏状态已确定同步。";
        Console.WriteLine(summary);Console.WriteLine(result);
        if(savedLog!=null){savedLog.Write(summary);savedLog.Write(result);}
    }
    static int Main(string[] args) {
        Console.OutputEncoding=new UTF8Encoding(false);
        try {
            if(args.Length==3 && args[0]=="--online-self-test")return OnlineProbe.SelfTest(args[1],args[2]);
            if(args.Length==4 && args[0]=="--online-guest")return OnlineProbe.Guest(args[1],Int32.Parse(args[2]),args[3]);
            if(args.Length==5 && args[0]=="--online-guest")return OnlineProbe.Guest(args[1],Int32.Parse(args[2]),args[3],args[4]=="diverge");
            if((args.Length==3 || args.Length==4) && args[0]=="--host-test") {
                var identity=NetIdentity.Decode(Encoding.ASCII.GetString(Convert.FromBase64String(args[2])));
                using(var host=new NetSession())using(var watchdog=new Timer(s=>host.Dispose(),null,20000,Timeout.Infinite)) {
                    int port=host.Listen(IPAddress.Loopback,0);Console.WriteLine("PORT:"+port);Console.Out.Flush();
                    host.Host(identity,args[1],s=>{},args.Length==4?InputTape.Read(args[3]):InputTape.Fixture());
                    if(host.Messages!=240 || host.Rejections!=7)return 3;
                    Console.WriteLine("HOST-PASS:"+host.Messages+":"+host.Rejections);return 0;
                }
            }
            if(args.Length==1 && args[0]=="--self-test") {SelfTest(Fixture());return 0;}
            if(args.Length==2 && args[0]=="--self-test") {SelfTest(Fixture(),args[1]);return 0;}
            if(args.Length==2 && args[0]=="--inspect-input") {Console.WriteLine(InputTape.Read(args[1]).Summary());return 0;}
            if(args.Length==3 && args[0]=="--self-test") {
                savedLog=new NetworkLog(Path.GetDirectoryName(Path.GetFullPath(args[1])),"selftest");
                Console.WriteLine("日志："+savedLog.FilePath);
                var identity=NetIdentity.Load(args[1],args[2]);
                savedLog.Write("Core="+identity.Core+"; Mod="+identity.Mod+"; Rules="+identity.Rules+"; endpoint=127.0.0.1");
                SelfTest(identity);return 0;
            }
            Console.Error.WriteLine("Usage: TH18Coop.NetTest.exe --self-test [game-core-exe mod-folder]");return 2;
        } catch(Exception error) {
            string failure="FAIL ["+stage+"] "+error.GetType().Name+": "+error.Message;Console.Error.WriteLine(failure);
            if(savedLog!=null)try {savedLog.Write(failure);}catch { }return 1;
        } finally {if(savedLog!=null)try {savedLog.Dispose();}catch { }}
    }
}
