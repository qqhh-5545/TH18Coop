using System;
using System.Diagnostics;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Threading;

namespace TH18LocalCoop.V104 {
    internal sealed class OnlineRequest {
        internal uint Step,Kind,Input,Digest,Result,Options;
        internal readonly uint[] Rng=new uint[4];
        internal readonly uint[] Detail=new uint[9];
        internal string Encode() {return Step+"|"+Kind+"|"+Input+"|"+Digest+"|"+Result+"|"+String.Join("|",Rng)+"|"+Options+"|"+String.Join("|",Detail);}
        internal static OnlineRequest Decode(string value) {
            string[] fields=value.Split('|');var words=new uint[19];
            if(fields.Length!=19)throw new IOException("ONLINE_PROTOCOL");
            for(int i=0;i<19;++i)if(!UInt32.TryParse(fields[i],out words[i]))throw new IOException("ONLINE_PROTOCOL");
            if(words[1]<1 || words[1]>2 || words[4]>8 || (words[9]&~0x200u)!=0)throw new IOException("ONLINE_PROTOCOL");
            var result=new OnlineRequest {Step=words[0],Kind=words[1],Input=words[2],Digest=words[3],Result=words[4],Options=words[9]};
            Array.Copy(words,5,result.Rng,0,4);Array.Copy(words,10,result.Detail,0,9);return result;
        }
    }
    internal sealed class OnlineReply {
        internal uint Step,Kind,P1,P2,P3,Result,Options;
        internal readonly uint[] Rng=new uint[4];
        internal string Encode() {return Step+"|"+Kind+"|"+P1+"|"+P2+"|"+Result+"|"+String.Join("|",Rng)+"|"+Options+"|"+P3;}
        internal static OnlineReply Decode(string value) {
            string[] fields=value.Split('|');var words=new uint[11];
            if(fields.Length!=11)throw new IOException("ONLINE_PROTOCOL");
            for(int i=0;i<11;++i)if(!UInt32.TryParse(fields[i],out words[i]))throw new IOException("ONLINE_PROTOCOL");
            if(words[1]<1 || words[1]>2 || (words[3]&~0xcfbu)!=0 || (words[10]&~0xcfbu)!=0 || words[4]>8 || (words[9]&~0x200u)!=0)throw new IOException("ONLINE_PROTOCOL");
            var result=new OnlineReply {Step=words[0],Kind=words[1],P1=words[2],P2=words[3],P3=words[10],Result=words[4],Options=words[9]};
            Array.Copy(words,5,result.Rng,0,4);return result;
        }
        internal static OnlineReply Merge(params OnlineRequest[] seats) {
            if(seats==null || seats.Length<2 || seats.Length>3)throw new IOException("ONLINE_ROSTER");
            OnlineRequest host=seats[0];
            for(int seat=1;seat<seats.Length;++seat) {
            OnlineRequest guest=seats[seat];
            if(host.Step!=guest.Step || host.Kind!=guest.Kind)throw new IOException("ONLINE_SEQUENCE");
            if(host.Kind==1 && host.Digest!=guest.Digest) {
                string[] names={"stage","scene","next","globals_rng","menu","p1","p2","loader_busy","loader_done"};
                string field="digest";for(int i=0;i<9;++i)if(host.Detail[i]!=guest.Detail[i]){field=names[i];break;}
                throw new IOException("ONLINE_DESYNC step="+host.Step+" host="+host.Digest+" guest="+guest.Digest+" field="+field+
                    " host_detail="+String.Join(",",host.Detail)+" guest_detail="+String.Join(",",guest.Detail));
            }
            if(host.Kind==2 && host.Result!=guest.Result)throw new IOException("ONLINE_TICK_RESULT step="+host.Step);
            }
            uint shared=0;for(int seat=1;seat<seats.Length;++seat)shared|=seats[seat].Input&0x100;
            var reply=new OnlineReply {Step=host.Step,Kind=host.Kind,P1=host.Input|shared,
                P2=Normalize(seats[1].Input),P3=seats.Length==3?Normalize(seats[2].Input):0,Result=host.Result,Options=host.Options};
            Array.Copy(host.Rng,reply.Rng,4);return reply;
        }
        internal static uint Normalize(uint input) {return (input&0xcfb)|((input&0x80000)!=0?1u:0u);}
    }
    internal sealed class OnlineBridge : IDisposable {
        internal readonly string Token=Guid.NewGuid().ToString("N");
        readonly MemoryMappedFile mapping;
        readonly MemoryMappedViewAccessor memory;
        readonly EventWaitHandle request,reply;
        volatile bool disposed;
        internal OnlineBridge(int role,uint seed,bool localDuo=false,bool sharedKeyboard=true,int players=2) {
            if(players<2 || players>3 || role<0 || role>=players || (players==3 && sharedKeyboard && localDuo))throw new ArgumentException("ONLINE_ROSTER");
            string name="Local\\TH18CoopOnline-"+Token;
            mapping=MemoryMappedFile.CreateNew(name,160);memory=mapping.CreateViewAccessor(0,160);
            request=new EventWaitHandle(false,EventResetMode.AutoReset,name+"-request");
            reply=new EventWaitHandle(false,EventResetMode.AutoReset,name+"-reply");
            memory.Write(0,0x54484e32u);memory.Write(4,2u);memory.Write(12,(uint)role);memory.Write(64,seed);memory.Write(128,(uint)players);
            memory.Write(84,localDuo?(sharedKeyboard?1u:3u):0u);
        }
        internal uint GamePid {get{return memory.ReadUInt32(72);}}
        internal bool SharedKeyboard {get{return (memory.ReadUInt32(84)&3)==1;}}
        internal void EnableRollback(){memory.Write(84,memory.ReadUInt32(84)|16u);}
        internal void EnableDelay(){memory.Write(84,memory.ReadUInt32(84)|32u);}
        internal void SetPeerPid(uint pid){memory.Write(88,pid);}
        internal void SetRoster(uint[] pids){for(int i=0;i<pids.Length && i<3;++i)memory.Write(136+i*4,pids[i]);if(pids.Length>1)SetPeerPid(pids[0]==GamePid?pids[1]:pids[0]);}
        internal void SetTestInput(uint keys) {
            if((memory.ReadUInt32(84)&1)==0)throw new InvalidOperationException("Not an isolated local test");
            memory.Write(148,keys&0x80dfbu);Thread.MemoryBarrier();memory.Write(152,0x54455354u);
        }
        internal void SetTestCapture(uint serial) {
            if((memory.ReadUInt32(84)&1)==0 || serial>999)throw new InvalidOperationException("Not an isolated local test");
            memory.Write(156,serial);
        }
        internal void WaitBootReady(Func<bool> exited,CancellationToken cancel) {
            var timer=Stopwatch.StartNew();
            while(memory.ReadUInt32(80)==0) {
                cancel.ThrowIfCancellationRequested();
                if(memory.ReadInt32(8)==4)throw new IOException("在线实例初始化失败，原生错误码="+memory.ReadUInt32(68));
                if(exited())throw new IOException("游戏在在线初始化前退出，可能仍有普通游戏实例占用单实例锁。");
                if(timer.ElapsedMilliseconds>30000)throw new IOException("在线实例初始化超时，请查看 coop.log。");
                Thread.Sleep(10);
            }
        }
        internal OnlineRequest WaitRequest(Func<bool> exited,CancellationToken cancel) {
            var timer=Stopwatch.StartNew();
            while(!disposed) {
                cancel.ThrowIfCancellationRequested();
                int state=memory.ReadInt32(8);
                if(state==4 || state==5)throw new IOException("ONLINE_NATIVE_STOP "+memory.ReadUInt32(68));
                if(state==2) {
                    Thread.MemoryBarrier();
                    var result=new OnlineRequest {Step=memory.ReadUInt32(16),Kind=memory.ReadUInt32(20),Input=memory.ReadUInt32(24),
                        Digest=memory.ReadUInt32(28),Result=memory.ReadUInt32(32),Options=memory.ReadUInt32(60)};
                    for(int i=0;i<4;++i)result.Rng[i]=memory.ReadUInt32(44+i*4);
                    for(int i=0;i<9;++i)result.Detail[i]=memory.ReadUInt32(92+i*4);
                    return result;
                }
                if(exited())return null;
                if(timer.ElapsedMilliseconds>120000)throw new IOException("游戏就绪等待超时，请查看 coop.log。");
                request.WaitOne(20);
            }
            throw new OperationCanceledException();
        }
        internal void Respond(OnlineReply value,Func<bool> exited,CancellationToken cancel) {
            if(disposed)throw new OperationCanceledException();
            if(memory.ReadInt32(8)!=2 || memory.ReadUInt32(16)!=value.Step || memory.ReadUInt32(20)!=value.Kind)throw new IOException("ONLINE_IPC_SEQUENCE");
            memory.Write(32,value.Result);memory.Write(36,value.P1);memory.Write(40,value.P2);
            memory.Write(132,value.P3);
            for(int i=0;i<4;++i)memory.Write(44+i*4,value.Rng[i]);
            memory.Write(60,value.Options);Thread.MemoryBarrier();memory.Write(8,3);reply.Set();
            // Wait for native acknowledgement so a fast loop cannot read the
            // same request twice. Native signals request on ACK and may already
            // publish its next request; map status is the condition, not events.
            var deadline=Stopwatch.StartNew();
            while(!disposed && memory.ReadInt32(8)==3) {
                cancel.ThrowIfCancellationRequested();if(exited())return;
                if(deadline.ElapsedMilliseconds>90000)throw new IOException("ONLINE_IPC_ACK_TIMEOUT");
                request.WaitOne(20);
            }
        }
        internal void Fail(uint code) {
            if(disposed)return;
            memory.Write(68,code);Thread.MemoryBarrier();memory.Write(8,4);reply.Set();request.Set();
        }
        public void Dispose() {
            if(disposed)return;Fail(9);disposed=true;
            memory.Dispose();mapping.Dispose();request.Dispose();reply.Dispose();
        }
    }
}
