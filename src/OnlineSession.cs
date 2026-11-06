using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Threading;

namespace TH18LocalCoop {
    internal sealed class OnlineSession : IDisposable {
        readonly object gate=new object();
        readonly string gameRoot,modRoot,entry,localAppData;
        readonly NetIdentity identity;
        readonly Func<int,IOnlineGame> gameFactory;
        readonly bool localTest,testSharedKeyboard,useBuffered;
        readonly int playerCount,inputDelay;
        readonly CancellationTokenSource stopped=new CancellationTokenSource();
        TcpListener listener;TcpClient connection;OnlineBridge bridge;RollbackTransport activeRollback;
        readonly List<TcpClient> clients=new List<TcpClient>();
        internal uint Completed {get;private set;}
        internal OnlineSession(string gameEntry,string root,Func<int,IOnlineGame> factory=null,NetIdentity testIdentity=null,string testAppData=null,bool localTest=false,bool sharedKeyboard=false,int players=2,int delayFrames=2) {
            if(players<2 || players>3 || (players==3 && localTest && sharedKeyboard))throw new ArgumentException("ONLINE_ROSTER");
            if(delayFrames<1 || delayFrames>12)throw new ArgumentException("ONLINE_DELAY");
            playerCount=players;inputDelay=delayFrames;
            entry=gameEntry;modRoot=root;gameRoot=Path.GetDirectoryName(entry);
            gameFactory=factory??(role=>new OnlineGame(entry,modRoot));
            identity=testIdentity??NetIdentity.Load(Path.Combine(gameRoot,"th18.exe"),modRoot);
            localAppData=testAppData??Environment.GetEnvironmentVariable("APPDATA");
            this.localTest=localTest;testSharedKeyboard=sharedKeyboard;
            useBuffered=factory==null || (localTest && Environment.GetEnvironmentVariable("TH18_COOP_ROLLBACK_TEST")=="1");
        }
        internal int Listen(IPAddress address,int port) {
            lock(gate) {
                stopped.Token.ThrowIfCancellationRequested();listener=new TcpListener(address,port);
                listener.Server.ExclusiveAddressUse=true;listener.Start(2);return ((IPEndPoint)listener.LocalEndpoint).Port;
            }
        }
        void Own(TcpClient client) {
            lock(gate) {
                if(stopped.IsCancellationRequested){client.Close();throw new OperationCanceledException();}
                connection=client;client.NoDelay=true;client.ReceiveTimeout=180000;client.SendTimeout=15000;
                clients.Add(client);
            }
        }
        internal void Host(string code,Action<string> note) {
            if(!NetSession.ValidCode(code))throw new IOException("CODE");
            var peers=new List<NetworkStream>();
            while(peers.Count<playerCount-1 && !stopped.IsCancellationRequested) {
                var client=listener.AcceptTcpClient();Own(client);
                    var stream=client.GetStream();
                    try {
                        string[] hello=NetWire.Read(stream).Split('|');
                        if(hello.Length!=7 || hello[0]!="ONLINE" || hello[1]!=(useBuffered?"4":"2"))throw new IOException("ONLINE_PROTOCOL");
                        if(!String.Equals(code,hello[6],StringComparison.OrdinalIgnoreCase))throw new IOException("CODE");
                        string mismatch=identity.Difference(NetIdentity.Decode(String.Join("|",hello,2,4)));
                        if(mismatch!=null)throw new IOException(mismatch);
                    } catch(IOException error) {
                        string reason=error.Message;
                        if(reason!="CODE" && reason!="VERSION" && reason!="CORE" && reason!="MOD" && reason!="RULES")reason="PROTOCOL";
                        try {NetWire.Send(stream,"ERROR|"+reason);}catch { }
                        client.Close();note("拒绝不匹配的在线连接，继续等待。");continue;
                    }
                    peers.Add(stream);NetWire.Send(stream,"LOBBY|"+peers.Count+"|"+playerCount);
                    note("P"+(peers.Count+1)+" 已加入；已连接 "+(peers.Count+1)+"/"+playerCount+" 人。");
            }
            stopped.Token.ThrowIfCancellationRequested();
            string session=Guid.NewGuid().ToString("N");uint seed;
            using(var rng=RandomNumberGenerator.Create()){byte[] random=new byte[4];rng.GetBytes(random);seed=BitConverter.ToUInt32(random,0);}
            byte[] score=OnlineProfile.Snapshot(gameRoot);
            for(int i=0;i<peers.Count;++i) {
                NetWire.Send(peers[i],"ONLINE_OK|"+identity.Encode()+"|"+session+"|"+seed+(useBuffered?"|"+inputDelay:""));
                WriteScore(peers[i],score);
                if(NetWire.Read(peers[i])!="SCORE_OK|"+NetIdentity.Hash(score))throw new IOException("ONLINE_SCORE");
            }
            RunConnected(peers.ToArray(),0,playerCount,session,seed,score,note,inputDelay);
        }
        internal void Join(string address,int port,string code,Action<string> note) {
            if(!NetSession.ValidCode(code))throw new IOException("CODE");
            IPAddress ip=ResolveHost(address,stopped.Token);
            var client=new TcpClient(AddressFamily.InterNetwork);Own(client);
            using(client) {
                var pending=client.BeginConnect(ip,port,null,null);
                using(pending.AsyncWaitHandle) {
                    if(!pending.AsyncWaitHandle.WaitOne(5000))throw new IOException("连接主机超时。");client.EndConnect(pending);
                }
                var stream=client.GetStream();NetWire.Send(stream,"ONLINE|"+(useBuffered?"4":"2")+"|"+identity.Encode()+"|"+code.ToUpperInvariant());
                string lobby=NetWire.Read(stream);if(lobby.StartsWith("ERROR|"))throw new IOException(lobby.Substring(6));
                string[] allocation=lobby.Split('|');int role,count;
                if(allocation.Length!=3 || allocation[0]!="LOBBY" || !Int32.TryParse(allocation[1],out role) || !Int32.TryParse(allocation[2],out count) || count<2 || count>3 || role<1 || role>=count)throw new IOException("ONLINE_ROSTER");
                note("已分配 P"+(role+1)+"，等待 "+count+" 人到齐后自动启动。");
                string greeting=NetWire.Read(stream);if(greeting.StartsWith("ERROR|"))throw new IOException(greeting.Substring(6));
                string[] hello=greeting.Split('|');uint seed;
                if(hello.Length!=(useBuffered?8:7) || hello[0]!="ONLINE_OK" || !UInt32.TryParse(hello[6],out seed))throw new IOException("ONLINE_PROTOCOL");
                string difference=identity.Difference(NetIdentity.Decode(String.Join("|",hello,1,4)));
                if(difference!=null)throw new IOException(difference);
                byte[] score=ReadScore(stream);NetWire.Send(stream,"SCORE_OK|"+NetIdentity.Hash(score));
                int delay=inputDelay;
                if(useBuffered && (!Int32.TryParse(hello[7],out delay) || delay<1 || delay>12))throw new IOException("ONLINE_DELAY");
                RunConnected(new[]{stream},role,count,hello[5],seed,score,note,delay);
            }
        }
        internal static IPAddress ResolveHost(string address,CancellationToken cancel) {
            cancel.ThrowIfCancellationRequested();
            address=(address??"").Trim();IPAddress ip;
            if(IPAddress.TryParse(address,out ip)) {
                if(ip.AddressFamily!=AddressFamily.InterNetwork)throw new IOException("当前只支持 IPv4，请使用节点的 IPv4 地址或域名。");
                return ip;
            }
            if(Uri.CheckHostName(address)!=UriHostNameType.Dns || address.Length>253)
                throw new IOException("请输入主机 IPv4 或节点域名，端口填写在右侧端口框。");
            cancel.ThrowIfCancellationRequested();
            try {
                var pending=Dns.BeginGetHostAddresses(address,null,null);
                using(pending.AsyncWaitHandle) {
                    int ready=WaitHandle.WaitAny(new[]{pending.AsyncWaitHandle,cancel.WaitHandle},5000);
                    if(ready==1)throw new OperationCanceledException(cancel);
                    if(ready==WaitHandle.WaitTimeout)throw new IOException("节点域名解析超时。");
                    foreach(var value in Dns.EndGetHostAddresses(pending))if(value.AddressFamily==AddressFamily.InterNetwork)return value;
                }
            } catch(SocketException error){throw new IOException("无法解析节点域名："+error.Message,error);}
            throw new IOException("节点没有可用的 IPv4 地址。");
        }
        static void WriteScore(Stream stream,byte[] score) {
            OnlineProfile.ValidateScore(score);byte[] length=BitConverter.GetBytes(score.Length);
            stream.Write(length,0,4);byte[] digest=System.Text.Encoding.ASCII.GetBytes(NetIdentity.Hash(score));stream.Write(digest,0,64);
            stream.Write(score,0,score.Length);stream.Flush();
        }
        static byte[] Exact(Stream stream,int size) {
            byte[] bytes=new byte[size];int offset=0;
            while(offset<size){int n=stream.Read(bytes,offset,size-offset);if(n==0)throw new EndOfStreamException("DISCONNECTED");offset+=n;}
            return bytes;
        }
        static byte[] ReadScore(Stream stream) {
            int size=BitConverter.ToInt32(Exact(stream,4),0);if(size<0 || size>OnlineProfile.MaxScore)throw new IOException("ONLINE_SCORE_SIZE");
            string digest=System.Text.Encoding.ASCII.GetString(Exact(stream,64));byte[] score=Exact(stream,size);
            if(digest!=NetIdentity.Hash(score))throw new IOException("ONLINE_SCORE_HASH");OnlineProfile.ValidateScore(score);return score;
        }
        void RunConnected(NetworkStream[] streams,int role,int count,string session,uint seed,byte[] score,Action<string> note,int delay) {
            var profile=new OnlineProfile(gameRoot,session,role,score,localAppData);
            // A forwarded remote connection can originate from 127.0.0.1.
            // Only the explicit one-click test opts into shared-keyboard mode.
            bool localDuo=localTest;
            if(localDuo)profile.UseLocalTestWindow();
            IOnlineGame game=gameFactory(role);bool saveAllowed=false;
            using(var ownBridge=new OnlineBridge(role,seed,localDuo,testSharedKeyboard,count)) {
            using(var rollback=useBuffered?new RollbackTransport(ownBridge.Token,role,count,session,localTest,delay):null) {
                if(rollback!=null)ownBridge.EnableDelay();
                lock(gate){stopped.Token.ThrowIfCancellationRequested();bridge=ownBridge;activeRollback=rollback;}
                try {
                    note(localDuo && testSharedKeyboard?"共享键盘测试：P1 方向键/Z/X/Shift/C/D；P2 IJKL/U/P/O/H/N。聚焦任一测试游戏窗口即可操作双方。":
                        "已连接，正在启动本次会话游戏。你是 P"+(role+1)+"；使用本机方向键、Z/X/Shift/C/D 操作。");
                    note("本次会话目录："+profile.ProfileDirectory);
                    if(role>0) {
                        string started=NetWire.Read(streams[0]);uint peer;
                        if(!started.StartsWith("START|") || !UInt32.TryParse(started.Substring(6),out peer))throw new IOException("ONLINE_START_ORDER");
                        ownBridge.SetPeerPid(peer);
                    }
                    game.Start(profile,ownBridge,stopped.Token);
                    ownBridge.WaitBootReady(()=>game.Exited,stopped.Token);
                    if(role==0) {
                        var pids=new uint[count];pids[0]=ownBridge.GamePid;
                        for(int i=0;i<streams.Length;++i) {
                            NetWire.Send(streams[i],"START|"+ownBridge.GamePid);
                            string started=NetWire.Read(streams[i]);
                            if(!started.StartsWith("STARTED|") || !UInt32.TryParse(started.Substring(8),out pids[i+1]))throw new IOException("ONLINE_START_ORDER");
                        }
                        ownBridge.SetRoster(pids);Broadcast(streams,"ROSTER|"+String.Join("|",pids));
                    } else {
                        NetWire.Send(streams[0],"STARTED|"+ownBridge.GamePid);
                        string[] roster=NetWire.Read(streams[0]).Split('|');var pids=new uint[count];
                        if(roster.Length!=count+1 || roster[0]!="ROSTER")throw new IOException("ONLINE_ROSTER");
                        for(int i=0;i<count;++i)if(!UInt32.TryParse(roster[i+1],out pids[i]))throw new IOException("ONLINE_ROSTER");
                        ownBridge.SetRoster(pids);
                    }
                    uint expected=0,kind=1;
                    uint paceStep=0;var pace=Stopwatch.StartNew();
                    while(!stopped.IsCancellationRequested) {
                        OnlineRequest local=ownBridge.WaitRequest(()=>game.Exited,stopped.Token);
                        if(local==null){saveAllowed=true;Broadcast(streams,"END");note("本机游戏已退出，结束在线会话。");break;}
                        if(local.Step!=expected || local.Kind!=kind)throw new IOException("ONLINE_SEQUENCE");
                        OnlineReply reply;
                        if(role==0) {
                            var requests=new OnlineRequest[count];requests[0]=local;bool ended=false;
                            for(int i=0;i<streams.Length;++i) {
                                string message=NetWire.Read(streams[i]);
                                if(message=="END"){ended=true;break;}
                                if(message.StartsWith("ABORT|"))throw new IOException(message.Substring(6));
                                if(!message.StartsWith("REQUEST|"))throw new IOException("ONLINE_PROTOCOL");
                                requests[i+1]=OnlineRequest.Decode(message.Substring(8));
                            }
                            if(ended){saveAllowed=true;Broadcast(streams,"END");note("有玩家退出，同步会话结束；请正常退出游戏。");break;}
                            reply=OnlineReply.Merge(requests);Broadcast(streams,"ADVANCE|"+reply.Encode());
                        } else {
                            NetWire.Send(streams[0],"REQUEST|"+local.Encode());
                            string message=NetWire.Read(streams[0]);if(message=="END"){saveAllowed=true;note("同步会话结束；请退出本次会话游戏。");break;}
                            if(message.StartsWith("ABORT|"))throw new IOException(message.Substring(6));
                            if(!message.StartsWith("ADVANCE|"))throw new IOException("ONLINE_PROTOCOL");
                            reply=OnlineReply.Decode(message.Substring(8));
                            if(reply.Step!=expected || reply.Kind!=kind)throw new IOException("ONLINE_SEQUENCE");
                            if(kind==1 && (role==1?reply.P2:reply.P3)!=OnlineReply.Normalize(local.Input))throw new IOException("ONLINE_INPUT_OWNERSHIP");
                        }
                        ownBridge.Respond(reply,()=>game.Exited,stopped.Token);
                        if(kind==2){++expected;Completed=expected;kind=1;}else kind=2;
                        if(expected==1 && kind==1 && rollback!=null){rollback.Run(streams,()=>game.Exited,stopped.Token,note,value=>Completed=value);saveAllowed=true;break;}
                        if(expected==1 && kind==1) {
                            note(count+" 人已进入同步。P1 操作主菜单，选人时依席位顺序操作，之后各自控制角色。");
                            pace.Restart();paceStep=expected;
                        }
                        if(expected>0 && expected%120==0 && kind==1) {
                            double rate=(expected-paceStep)/Math.Max(pace.Elapsed.TotalSeconds,0.000001);
                            note("在线同步步数 "+expected+"；状态校验一致；同步推进 "+rate.ToString("F1")+" 帧/秒（含等待）。");
                            pace.Restart();paceStep=expected;
                        }
                    }
                } catch(Exception error) {
                    if(rollback!=null && !localTest)rollback.RequestExit();
                    ownBridge.Fail(10);
                    if(rollback==null)Broadcast(streams,"ABORT|"+(error.Message.StartsWith("ONLINE_")?error.Message:"ONLINE_CONNECTION"));
                    note("在线会话中止："+error.Message+(rollback!=null && !localTest?"。本次游戏正在退出；会话文件已保留。":"。游戏保持暂停，请退出后重连；会话文件已保留。"));throw;
                } finally {
                    lock(gate){if(rollback!=null && !saveAllowed && !localTest)rollback.RequestExit();ownBridge.Fail(9);bridge=null;activeRollback=null;}
                    game.Finish(!localTest && saveAllowed && Completed>0,profile,note);
                }
            }
            }
        }
        static void Broadcast(NetworkStream[] streams,string message) {
            bool ending=message=="END" || message.StartsWith("ABORT|");
            foreach(var stream in streams)try {NetWire.Send(stream,message);}catch {if(!ending)throw;}
        }
        public void Dispose() {
            lock(gate) {
                if(activeRollback!=null)activeRollback.RequestExit();stopped.Cancel();if(bridge!=null)bridge.Fail(9);
                foreach(var client in clients)client.Close();if(listener!=null)listener.Stop();
            }
        }
    }
}
