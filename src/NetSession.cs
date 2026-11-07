using System;

using System.Diagnostics;

using System.IO;

using System.Net;

using System.Net.Sockets;

using System.Security.Cryptography;

using System.Text;

using System.Threading;

namespace TH18LocalCoop {

    internal sealed class NetIdentity {

        internal const string Version="1.0.4";

        internal const string CoreHash="5aeb74b19939a29a8cf06e4fe7777aaf9139e93478507e039f43318171020e9f";

        internal string Build=Version,Core,Mod,Rules;

        internal static string Hash(byte[] bytes) {

            using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();

        }

        internal static NetIdentity Load(string engine,string root) {

            string core=GameSelection.CanonicalCore(Hash(File.ReadAllBytes(engine)));

            if(core!=CoreHash)throw new InvalidOperationException("游戏核心版本不匹配。");

            // Rendering preferences are local. Only selection/fallback gameplay

            // options participate in this connection-test compatibility check.

            string section="";var rules=new StringBuilder();

            foreach(string line in File.ReadAllLines(Path.Combine(root,"coop.ini"))) {

                string value=line.Trim();

                if(value.StartsWith("["))section=value.ToLowerInvariant();

                else if((section=="[selection]" || section=="[player2]" || section=="[player3]") && value.Contains("=") && !value.StartsWith(";"))

                    rules.AppendLine(section+value.Replace(" ","").ToLowerInvariant());

            }

            return new NetIdentity {Core=core,Mod=Hash(File.ReadAllBytes(Path.Combine(root,"th18_coop.dll"))),Rules=Hash(Encoding.UTF8.GetBytes(rules.ToString()))};

        }

        internal string Encode() {return Build+"|"+Core+"|"+Mod+"|"+Rules;}

        internal static NetIdentity Decode(string value) {

            string[] parts=value.Split('|');

            if(parts.Length!=4 || parts[0].Length>32)throw new IOException("IDENTITY");

            for(int i=1;i<4;++i) {

                if(parts[i].Length!=64)throw new IOException("IDENTITY");

                foreach(char c in parts[i])if(!Uri.IsHexDigit(c))throw new IOException("IDENTITY");

            }

            return new NetIdentity {Build=parts[0],Core=parts[1],Mod=parts[2],Rules=parts[3]};

        }

        internal string Difference(NetIdentity peer) {

            if(Build!=peer.Build)return "VERSION";

            if(Core!=peer.Core)return "CORE";

            if(Mod!=peer.Mod)return "MOD";

            if(Rules!=peer.Rules)return "RULES";

            return null;

        }

    }

    internal static class NetWire {

        internal const int Limit=512;

        internal static void Send(Stream stream,string message) {

            if(message.Length>Limit)throw new IOException("LENGTH");

            foreach(char c in message)if(c<32 || c>126)throw new IOException("FORMAT");

            byte[] bytes=Encoding.ASCII.GetBytes(message+"\n");stream.Write(bytes,0,bytes.Length);

        }

        internal static string Read(Stream stream) {

            var result=new StringBuilder();var deadline=new Stopwatch();

            while(result.Length<=Limit) {

                if(deadline.ElapsedMilliseconds>8000)throw new IOException("TIMEOUT");

                int c=stream.ReadByte();

                if(!deadline.IsRunning)deadline.Start();

                if(c<0)throw new EndOfStreamException("DISCONNECTED");

                if(c==10)return result.ToString();

                if(c<32 || c>126)throw new IOException("FORMAT");

                result.Append((char)c);

            }

            throw new IOException("LENGTH");

        }

    }

    // Connection/compatibility/heartbeat prototype only. It never reads keys,

    // starts the game, attaches a DLL, or opens score/config/replay for writing.

    internal sealed class NetSession : IDisposable {

        readonly object gate=new object();

        TcpListener listener;TcpClient client;volatile bool stopped;

        internal int Messages {get;private set;}

        internal int Rejections {get;private set;}

        internal static string NewCode() {

            byte[] bytes=new byte[4];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(bytes);

            return BitConverter.ToString(bytes).Replace("-","");

        }

        internal static bool ValidCode(string value) {

            if(value==null || value.Length!=8)return false;

            foreach(char c in value)if(!Uri.IsHexDigit(c))return false;

            return true;

        }

        internal int Listen(IPAddress address,int port) {

            lock(gate) {

                if(stopped)throw new ObjectDisposedException("NetSession");

                listener=new TcpListener(address,port);listener.Server.ExclusiveAddressUse=true;listener.Start(1);

                return ((IPEndPoint)listener.LocalEndpoint).Port;

            }

        }

        void UseClient(TcpClient value) {

            lock(gate) {

                if(stopped){value.Close();throw new ObjectDisposedException("NetSession");}

                client=value;client.NoDelay=true;client.ReceiveTimeout=5000;client.SendTimeout=3000;

            }

        }

        internal void Host(NetIdentity identity,string code,Action<string> note,InputTape tape=null) {

            if(!ValidCode(code))throw new ArgumentException("CODE");

            while(!stopped) {

                TcpClient accepted=listener.AcceptTcpClient();UseClient(accepted);

                using(accepted) {

                    var stream=accepted.GetStream();bool welcomed=false;

                    try {

                        string[] hello=NetWire.Read(stream).Split('|');

                        if(hello.Length!=7 || hello[0]!="TH18COOP" || hello[1]!="1")throw new IOException("PROTOCOL");

                        if(!String.Equals(hello[6],code,StringComparison.OrdinalIgnoreCase))throw new IOException("CODE");

                        string difference=identity.Difference(NetIdentity.Decode(String.Join("|",hello,2,4)));

                        if(difference!=null)throw new IOException(difference);

                        NetWire.Send(stream,"WELCOME|1|"+identity.Encode()+"|P2");welcomed=true;

                        note("朋友已连接，你是 P1，朋友是 P2。版本校验通过；当前仅测试连接，尚不能在线开始游戏。");

                        int expected=0;

                        while(!stopped) {

                            string line=NetWire.Read(stream);

                            if(line=="BYE") {NetWire.Send(stream,"BYE");note("朋友已正常断开。");return;}

                            string[] ping=line.Split('|');int frame,mask;

                            if(ping.Length==3 && ping[0]=="INPUT" && tape!=null) {

                                uint sequence,guest;

                                if(!UInt32.TryParse(ping[1],out sequence) || sequence!=(uint)expected || expected>=tape.Frames.Count ||

                                   !UInt32.TryParse(ping[2],out guest) || (guest&~0xcfbu)!=0)throw new IOException("SEQUENCE");

                                InputFrame source=tape.Frames[expected];

                                var merged=new InputFrame {Sequence=source.Sequence,P1=source.P1,P2=guest,P3=source.P3,Stage=source.Stage,Phase=source.Phase,Owner=source.Owner,Flags=source.Flags,ThreeSeatFormat=source.ThreeSeatFormat};

                                merged.Checksum=merged.Hash();NetWire.Send(stream,"FRAME|"+merged.Encode());

                                ++expected;++Messages;continue;

                            }

                            if(ping.Length!=3 || ping[0]!="PING" || !Int32.TryParse(ping[1],out frame) ||

                               !Int32.TryParse(ping[2],out mask) || frame!=expected || mask<0 || mask>4095)throw new IOException("SEQUENCE");

                            NetWire.Send(stream,"PONG|"+frame+"|"+mask);++expected;++Messages;

                        }

                    } catch(IOException error) {

                        if(welcomed)throw;

                        ++Rejections;

                        try {NetWire.Send(stream,"ERROR|"+Reason(error.Message));}catch { }

                        note("拒绝不匹配或无效连接（"+Reason(error.Message)+"），继续等待朋友。");

                    }

                }

            }

        }

        static string Reason(string value) {

            switch(value) {

                case "VERSION":case "CORE":case "MOD":case "RULES":case "CODE":case "PROTOCOL":case "IDENTITY":case "LENGTH":case "FORMAT":return value;

                default:return "HANDSHAKE";

            }

        }

        internal void Join(string address,int port,NetIdentity identity,string code,Action<string> note,int rounds,InputTape tape=null) {

            if(!ValidCode(code))throw new ArgumentException("CODE");

            IPAddress ip;

            if(!IPAddress.TryParse(address,out ip) || ip.AddressFamily!=AddressFamily.InterNetwork)throw new ArgumentException("请输入主机的 IPv4 地址。");

            var connection=new TcpClient(AddressFamily.InterNetwork);UseClient(connection);

            using(connection) {

                IAsyncResult pending=connection.BeginConnect(ip,port,null,null);

                using(pending.AsyncWaitHandle) {

                    if(!pending.AsyncWaitHandle.WaitOne(5000))throw new IOException("连接超时，请检查地址、端口和虚拟局域网。");

                    connection.EndConnect(pending);

                }

                var stream=connection.GetStream();NetWire.Send(stream,"TH18COOP|1|"+identity.Encode()+"|"+code.ToUpperInvariant());

                string welcome=NetWire.Read(stream);

                if(welcome.StartsWith("ERROR|"))throw new IOException(welcome.Substring(6));

                string[] parts=welcome.Split('|');

                if(parts.Length!=7 || parts[0]!="WELCOME" || parts[1]!="1" || parts[6]!="P2")throw new IOException("PROTOCOL");

                string difference=identity.Difference(NetIdentity.Decode(String.Join("|",parts,2,4)));

                if(difference!=null)throw new IOException(difference);

                note("已连接主机，你是 P2。版本校验通过；当前仅测试连接，尚不能在线开始游戏。");

                for(int frame=0;!stopped && (rounds==0 || frame<rounds);++frame) {

                    // Synthetic probe payload verifies ordered delivery, not game input sync.

                    int mask=(frame*17)&4095;var timer=Stopwatch.StartNew();

                    if(tape!=null) {

                        if(frame>=tape.Frames.Count)throw new IOException("SEQUENCE");

                        InputFrame input=tape.Frames[frame];NetWire.Send(stream,"INPUT|"+frame+"|"+input.P2);

                        string reply=NetWire.Read(stream);if(!reply.StartsWith("FRAME|"))throw new IOException("INPUT_FORMAT");

                        InputFrame merged=InputFrame.Decode(reply.Substring(6));

                        if(merged.Encode()!=input.Encode())throw new IOException("INPUT_DIVERGED");

                    } else {

                        NetWire.Send(stream,"PING|"+frame+"|"+mask);

                        if(NetWire.Read(stream)!="PONG|"+frame+"|"+mask)throw new IOException("SEQUENCE");

                    }

                    ++Messages;

                    note("连接正常 · 往返 "+timer.ElapsedMilliseconds+" ms · 已确认 "+Messages+" 组探测消息。尚未接入游戏同步。");

                    if(rounds==0)Thread.Sleep(500);

                }

                if(!stopped) {NetWire.Send(stream,"BYE");if(NetWire.Read(stream)!="BYE")throw new IOException("PROTOCOL");}

            }

        }

        internal static string Explain(Exception error) {

            if(error is ObjectDisposedException)return "测试已停止。";

            var socket=error as SocketException;

            if(socket!=null && socket.SocketErrorCode==SocketError.AddressAlreadyInUse)return "端口已被占用，请停止另一个主机或更换端口。";

            switch(error.Message) {

                case "VERSION":return "双方 mod 版本不同，请使用同一版本。";
                case "PROTOCOL":case "ONLINE_PROTOCOL":return "联机协议不兼容，请双方使用同一完整安装包。";

                case "CORE":return "双方游戏核心版本不同。";

                case "MOD":return "双方 mod 文件不同，请使用同一安装包。";

                case "RULES":return "双方玩法配置不同，请核对 coop.ini 的 Selection 和 Player2 部分。";

                case "CODE":return "连接码不正确，请向主机确认。";

                case "DISCONNECTED":return "对方已断开连接。";

                default:return error is SocketException?"网络连接中断或无法连接，请检查主机地址、端口和网络。":error.Message;

            }

        }

        public void Dispose() {

            lock(gate){stopped=true;if(client!=null)client.Close();if(listener!=null)listener.Stop();}

        }

    }

}

