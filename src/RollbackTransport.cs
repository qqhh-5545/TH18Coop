using System;

using System.Collections.Generic;

using System.Diagnostics;

using System.IO;

using System.IO.MemoryMappedFiles;

using System.Net.Sockets;

using System.Threading;

using System.Threading.Tasks;
using System.Text;



namespace TH18LocalCoop {

    // Bytes are the unmodified upstream input codec, framed only for the TCP

    // star relay. One reader per socket and serialized writes avoid head-of-

    // line dependencies on the native game's current simulation frame.

    internal sealed class RollbackTransport : IDisposable {

        const int Slots=64,SlotSize=1032,Start=128,Size=Start+2*Slots*SlotSize;

        readonly MemoryMappedFile map;readonly MemoryMappedViewAccessor memory;

        readonly object inbound=new object();readonly int role,players,inputDelay;readonly bool diagnostic;

        volatile bool disposed,remoteEnded,remoteFault;Exception error;

        sealed class Delayed {internal int From;internal byte[] Data;internal long Due;}

        readonly List<Delayed> delayed=new List<Delayed>();readonly int testDelay,testJitter;int packetOrdinal;

        internal RollbackTransport(string token,int seat,int count,string session,bool isolated=false,int delayFrames=0) {

            if(delayFrames<0 || delayFrames>12)throw new ArgumentException("ONLINE_DELAY");

            inputDelay=delayFrames;role=seat;players=count;diagnostic=isolated;map=MemoryMappedFile.CreateNew("Local\\TH18CoopRollback-"+token,Size);

            memory=map.CreateViewAccessor(0,Size);memory.Write(0,0x54485233u);memory.Write(8,(uint)role);memory.Write(12,(uint)players);

            memory.Write(16,Convert.ToUInt64(session.Substring(0,16),16));memory.Write(56,(uint)inputDelay);

            if(isolated && Environment.GetEnvironmentVariable("TH18_COOP_ROLLBACK_TEST")=="1") {int delay;if(Int32.TryParse(Environment.GetEnvironmentVariable("TH18_COOP_RB_TEST_LATENCY_MS"),out delay) && delay>=0 && delay<=150)testDelay=delay;if(Int32.TryParse(Environment.GetEnvironmentVariable("TH18_COOP_RB_TEST_JITTER_MS"),out delay) && delay>=0 && delay<=60)testJitter=delay;if(Environment.GetEnvironmentVariable("TH18_COOP_RB_SCENARIO")=="shop-stage2")memory.Write(24,1u);if(Environment.GetEnvironmentVariable("TH18_COOP_SUPPLY_TEST")=="1")memory.Write(24,2u);if(Environment.GetEnvironmentVariable("TH18_COOP_GAMEPLAY_TEST")=="1")memory.Write(24,3u);}

        }

        static byte[] Exact(Stream stream,int size) {var data=new byte[size];for(int at=0;at<size;){int n=stream.Read(data,at,size-at);if(n==0)throw new EndOfStreamException("ROLLBACK_DISCONNECTED");at+=n;}return data;}

        static void Send(Stream stream,object gate,int from,int to,byte[] data) {

            if(data.Length<1 || data.Length>1024)throw new IOException("ROLLBACK_PACKET_SIZE");

            var bytes=new byte[data.Length+4];bytes[0]=(byte)from;bytes[1]=(byte)to;bytes[2]=(byte)data.Length;bytes[3]=(byte)(data.Length>>8);Array.Copy(data,0,bytes,4,data.Length);

            lock(gate)stream.Write(bytes,0,bytes.Length);

        }

        void Deliver(int from,byte[] bytes,Func<bool> exited,CancellationToken cancel) {

            lock(inbound) {

                var deadline=Stopwatch.StartNew();

                while(!disposed){cancel.ThrowIfCancellationRequested();uint write=memory.ReadUInt32(72),read=memory.ReadUInt32(76);

                    if(unchecked(write-read)<Slots){long offset=Start+Slots*SlotSize+(write%Slots)*SlotSize;memory.Write(offset,(uint)from);memory.Write(offset+4,(uint)bytes.Length);memory.WriteArray(offset+8,bytes,0,bytes.Length);Thread.MemoryBarrier();memory.Write(72,unchecked(write+1));return;}

                    if(exited())return;if(deadline.ElapsedMilliseconds>10000)throw new IOException("ROLLBACK_INBOX_STALLED");Thread.Sleep(1);

                }

            }

        }

        internal void Run(Stream[] streams,Func<bool> exited,CancellationToken cancel,Action<string> note,Action<uint> progress,Func<bool> recovering=null,Func<Exception> recoveryFailure=null) {

            var locks=new object[streams.Length];for(int i=0;i<locks.Length;++i)locks[i]=new object();

            var readers=new List<Task>();

            for(int i=0;i<streams.Length;++i){int socket=i;readers.Add(Task.Run(()=> {

                try {while(!disposed && !cancel.IsCancellationRequested) {

                    byte[] header=Exact(streams[socket],4);int from=header[0],to=header[1],length=header[2]|header[3]<<8;

                    if(length<1 || length>1024 || from>=players || to>=players || from==to || (role==0?from!=socket+1:to!=role))throw new IOException("ROLLBACK_RELAY_OWNERSHIP");

                    byte[] packet=Exact(streams[socket],length);

                    if(to==role && length>=10 && packet[0]==254){
                        remoteFault=true;uint frame=BitConverter.ToUInt32(packet,2),code=BitConverter.ToUInt32(packet,6);
                        string category=packet[1]==1?"desync":"peer";
                        Interlocked.CompareExchange(ref error,new OnlinePauseException(category,"队友会话已暂停："+Encoding.UTF8.GetString(packet,10,packet.Length-10),frame,code),null);
                        memory.Write(4,4);continue;
                    }

                    if(to==role && length==1 && packet[0]==255){remoteEnded=true;memory.Write(52,1u);continue;}

                    if(to==role){if(testDelay>0 && packet[0]==0){int ordinal=Interlocked.Increment(ref packetOrdinal),delay=Math.Max(0,testDelay+((ordinal%5)-2)*testJitter/2);lock(delayed)delayed.Add(new Delayed {From=from,Data=packet,Due=Stopwatch.GetTimestamp()+delay*Stopwatch.Frequency/1000});}else Deliver(from,packet,exited,cancel);}else if(role==0)Send(streams[to-1],locks[to-1],from,to,packet);else throw new IOException("ROLLBACK_ROUTE");

                }}catch(Exception failure){if(!disposed && !remoteEnded && !cancel.IsCancellationRequested){Interlocked.CompareExchange(ref error,failure,null);try{memory.Write(4,4);}catch(ObjectDisposedException){}}}

            }));}

            var report=Stopwatch.StartNew();note(players+(inputDelay>0?" 人延迟同步已连接（缓冲 "+inputDelay+" 帧）；":" 人测试传输已连接；")+"P1 操作主菜单，选人和初始卡牌依席位顺序选择。使用本机正常键位控制自己的角色。");

            bool waiting=false;
            try {while(!disposed && !cancel.IsCancellationRequested) {

                if(error!=null)throw error is OnlinePauseException?error:new OnlinePauseException("transport","网络会话无法继续："+error.Message,0,0,error);
                Exception recoveryError=recoveryFailure==null?null:recoveryFailure();
                if(recoveryError!=null)throw new OnlinePauseException("transport",recoveryError.Message);
                bool reconnecting=recovering!=null && recovering();
                memory.Write(84,reconnecting?1u:0u);
                if(reconnecting!=waiting){waiting=reconnecting;note(waiting?"网络中断，游戏已暂停；正在等待原玩家重连（最多30秒）……":"连接已恢复，继续原会话。");}

                for(;;){Delayed next=null;lock(delayed){long now=Stopwatch.GetTimestamp();int earliest=-1;for(int i=0;i<delayed.Count;++i)if(delayed[i].Due<=now && (earliest<0 || delayed[i].Due<delayed[earliest].Due))earliest=i;if(earliest>=0){next=delayed[earliest];delayed.RemoveAt(earliest);}}if(next==null)break;Deliver(next.From,next.Data,exited,cancel);}

                if(memory.ReadUInt32(4)==4){uint code=memory.ReadUInt32(48);if(memory.ReadUInt32(80)==1)throw new OnlinePauseException("desync","同步状态校验不一致，首次差异帧 "+memory.ReadUInt32(60)+"。游戏已暂停，详细分组见 coop.log。",memory.ReadUInt32(60),code);throw new OnlinePauseException("native","游戏同步已暂停，代码 "+code,memory.ReadUInt32(28),code);}

                if(exited()){

                    if(memory.ReadUInt32(4)!=2)throw new IOException("ROLLBACK_UNCONFIRMED_EXIT");

                    if(!remoteEnded || role==0)for(int seat=0;seat<players;++seat)if(seat!=role)try{int socket=role==0?seat-1:0;Send(streams[socket],locks[socket],role,seat,new byte[]{255});streams[socket].Flush();}catch(IOException){if(!remoteEnded)throw;}catch(ObjectDisposedException){if(!remoteEnded)throw;}

                    return;

                }

                uint read=memory.ReadUInt32(68),write=memory.ReadUInt32(64);if(unchecked(write-read)>Slots)throw new IOException("ROLLBACK_OUTBOX_CORRUPT");

                while(read!=write){long offset=Start+(read%Slots)*SlotSize;uint to=memory.ReadUInt32(offset),length=memory.ReadUInt32(offset+4);

                    if(to>=players || to==role || length<1 || length>1024)throw new IOException("ROLLBACK_OUTBOX_PACKET");var bytes=new byte[length];memory.ReadArray(offset+8,bytes,0,bytes.Length);

                    int socket=role==0?(int)to-1:0;Send(streams[socket],locks[socket],role,(int)to,bytes);Thread.MemoryBarrier();memory.Write(68,++read);

                }

                progress(memory.ReadUInt32(28));

                if(diagnostic && report.ElapsedMilliseconds>=2000){note((inputDelay>0?"延迟同步帧 ":"测试帧 ")+memory.ReadUInt32(28)+"；确认 "+memory.ReadUInt32(32)+"；等待 "+memory.ReadUInt32(44));report.Restart();}

                Thread.Sleep(1);

            }cancel.ThrowIfCancellationRequested();}

            catch(Exception failure){
                // Freeze before notifying peers. Never request native process
                // exit here: only the user's explicit End action may do that.
                try{memory.Write(4,4);}catch(ObjectDisposedException){}
                if((!remoteFault || role==0) && !(failure is OperationCanceledException)){
                    var paused=failure as OnlinePauseException;byte[] reason=Encoding.UTF8.GetBytes(failure.Message.Length>200?failure.Message.Substring(0,200):failure.Message);
                    byte[] packet=new byte[10+reason.Length];packet[0]=254;packet[1]=(byte)(paused!=null && paused.Category=="desync"?1:2);
                    Array.Copy(BitConverter.GetBytes(paused==null?0:paused.Frame),0,packet,2,4);Array.Copy(BitConverter.GetBytes(paused==null?0:paused.Code),0,packet,6,4);Array.Copy(reason,0,packet,10,reason.Length);
                    for(int seat=0;seat<players;++seat)if(seat!=role)try{int socket=role==0?seat-1:0;Send(streams[socket],locks[socket],role,seat,packet);streams[socket].Flush();}catch(IOException){}catch(ObjectDisposedException){}
                }
                throw;
            }

            finally {disposed=true;foreach(var stream in streams)try{stream.Close();}catch{ }try{Task.WaitAll(readers.ToArray(),3000);}catch{ }}

        }

        internal void RequestExit(){try{memory.Write(52,1u);}catch(ObjectDisposedException){}}
        internal void DesyncForTest(uint frame){
            if(!diagnostic || Environment.GetEnvironmentVariable("TH18_COOP_ROLLBACK_TEST")!="1")throw new InvalidOperationException("Not an isolated diagnostic");
            memory.Write(60,frame);memory.Write(80,1u);memory.Write(48,54u);memory.Write(4,4);
        }

        public void Dispose(){disposed=true;memory.Dispose();map.Dispose();}

    }

}

