using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net.Sockets;
using System.Net;
using System.Threading;

namespace TH18LocalCoop.V104 {
    internal sealed class OnlinePauseException : IOException {
        internal readonly string Category;internal readonly uint Frame,Code;
        internal OnlinePauseException(string category,string message,uint frame=0,uint code=0,Exception inner=null):base(message,inner){Category=category;Frame=frame;Code=code;}
    }
    // One reliable, bounded message journal per original TCP peer. A new socket
    // resumes this journal; it never creates a new game, seat or input history.
    internal sealed class ReconnectStream : Stream {
        const uint Magic=0x35524854;
        const int MaxPacket=1028,MaxJournal=4096,MaxQueuedBytes=4*1024*1024;
        readonly object gate=new object();
        readonly AutoResetEvent changed=new AutoResetEvent(false);
        readonly SortedDictionary<ulong,byte[]> journal=new SortedDictionary<ulong,byte[]>();
        readonly Queue<byte[]> received=new Queue<byte[]>();
        readonly Thread reader,writer;readonly int recoveryMs;
        Stream socket;long generation;bool closed;Exception fatal;
        ulong sent,acknowledged,accepted,publishedAck;int queuedBytes;byte[] current;int currentOffset;
        long missingSince;int recoveries;
        internal ReconnectStream(Stream initial,int recoveryMilliseconds=30000) {
            if(initial==null)throw new ArgumentNullException("initial");
            recoveryMs=recoveryMilliseconds;socket=initial;
            reader=new Thread(ReadLoop){IsBackground=true};writer=new Thread(WriteLoop){IsBackground=true};
            reader.Start();writer.Start();
        }
        internal bool Recovering {get{lock(gate)return !closed && fatal==null && (socket==null || missingSince!=0);}}
        internal bool NeedsConnection {get{lock(gate)return !closed && fatal==null && socket==null;}}
        internal int Recoveries {get{lock(gate)return recoveries;}}
        internal Exception Failure {get{lock(gate){CheckDeadline();return fatal;}}}
        internal bool Attach(Stream replacement) {
            lock(gate) {
                CheckDeadline();
                if(closed || fatal!=null || socket!=null){replacement.Dispose();return false;}
                socket=replacement;++generation;++recoveries;publishedAck=0;
                Monitor.PulseAll(gate);changed.Set();return true;
            }
        }
        // Test seam: cuts only this owned link and leaves both journals intact.
        internal void Interrupt(){Stream old;lock(gate)old=socket;if(old!=null)Lost(old);}
        void CheckDeadline() {
            if(!closed && fatal==null && missingSince!=0 && (Stopwatch.GetTimestamp()-missingSince)*1000/Stopwatch.Frequency>=recoveryMs)
                fatal=new IOException("RECONNECT_TIMEOUT：原席位未在重连期限内恢复。");
        }
        void Lost(Stream old) {
            bool close=false;
            lock(gate){if(ReferenceEquals(socket,old)){socket=null;if(missingSince==0)missingSince=Stopwatch.GetTimestamp();++generation;close=true;Monitor.PulseAll(gate);changed.Set();}}
            if(close)try{old.Dispose();}catch(IOException){}
        }
        void Fail(Exception failure){lock(gate){if(!closed && fatal==null)fatal=failure;Monitor.PulseAll(gate);changed.Set();}}
        static byte[] Exact(Stream source,int size){var bytes=new byte[size];for(int at=0;at<size;){int n=source.Read(bytes,at,size-at);if(n==0)throw new EndOfStreamException();at+=n;}return bytes;}
        void ReadLoop() {
            while(true) {
                Stream source;
                lock(gate){CheckDeadline();if(closed || fatal!=null)return;source=socket;if(source==null){Monitor.Wait(gate,50);continue;}}
                try {
                    byte[] header=Exact(source,25);
                    uint magic=BitConverter.ToUInt32(header,0);byte kind=header[4];
                    ulong sequence=BitConverter.ToUInt64(header,5),ack=BitConverter.ToUInt64(header,13);
                    uint length=BitConverter.ToUInt32(header,21);
                    if(magic!=Magic || (kind!=1 && kind!=2) || length>MaxPacket || (kind==1?(sequence==0 || length==0):(sequence!=0 || length!=0)))
                        throw new InvalidDataException("RECONNECT_PROTOCOL");
                    byte[] payload=Exact(source,(int)length);
                    lock(gate) {
                        if(!ReferenceEquals(source,socket))continue;
                        if(ack>sent)throw new InvalidDataException("RECONNECT_ACK");
                        // A successful TCP handshake alone is insufficient. Only
                        // a valid original peer record completes recovery.
                        missingSince=0;
                        if(ack>acknowledged){acknowledged=ack;var remove=new List<ulong>();foreach(var entry in journal){if(entry.Key>ack)break;remove.Add(entry.Key);}foreach(ulong key in remove)journal.Remove(key);}
                        if(kind==1 && sequence>accepted) {
                            if(sequence!=accepted+1)throw new InvalidDataException("RECONNECT_SEQUENCE");
                            if(queuedBytes+payload.Length>MaxQueuedBytes)throw new InvalidDataException("RECONNECT_INBOX_LIMIT");
                            received.Enqueue(payload);queuedBytes+=payload.Length;accepted=sequence;
                            Monitor.PulseAll(gate);
                        }
                        // A duplicate is acknowledged, but never delivered twice.
                        if(kind==1)changed.Set();
                    }
                } catch(InvalidDataException failure){Fail(failure);return;}
                  catch(Exception failure){if(!(failure is IOException) && !(failure is ObjectDisposedException) && !(failure is SocketException)){Fail(failure);return;}Lost(source);}
            }
        }
        void WriteLoop() {
            long epoch=-1;ulong cursor=1;long heartbeat=0;
            while(true) {
                Stream target;byte[] payload=null;ulong sequence=0,ack,ackSent;
                lock(gate) {
                    CheckDeadline();if(closed || fatal!=null)return;target=socket;
                    if(target!=null) {
                        if(epoch!=generation){epoch=generation;cursor=acknowledged+1;heartbeat=0;}
                        if(cursor<=acknowledged)cursor=acknowledged+1;
                        if(journal.TryGetValue(cursor,out payload))sequence=cursor;
                    }
                    ack=accepted;ackSent=publishedAck;
                }
                if(target==null){changed.WaitOne(50);continue;}
                // Data wakes immediately. Heartbeats detect silent broken links
                // even while a loader or menu produces no gameplay packets.
                if(payload==null && ack<=ackSent && Stopwatch.GetTimestamp()-heartbeat<Stopwatch.Frequency/2){changed.WaitOne(10);continue;}
                try {
                    int size=payload==null?0:payload.Length;byte[] record=new byte[25+size];
                    Array.Copy(BitConverter.GetBytes(Magic),0,record,0,4);record[4]=(byte)(payload==null?2:1);
                    Array.Copy(BitConverter.GetBytes(sequence),0,record,5,8);Array.Copy(BitConverter.GetBytes(ack),0,record,13,8);
                    Array.Copy(BitConverter.GetBytes(size),0,record,21,4);if(size>0)Array.Copy(payload,0,record,25,size);
                    target.Write(record,0,record.Length);heartbeat=Stopwatch.GetTimestamp();if(payload!=null)++cursor;
                    lock(gate){if(ReferenceEquals(target,socket) && epoch==generation)publishedAck=ack;Monitor.PulseAll(gate);}
                } catch(Exception failure){if(!(failure is IOException) && !(failure is ObjectDisposedException) && !(failure is SocketException)){Fail(failure);return;}Lost(target);}
            }
        }
        public override int Read(byte[] buffer,int offset,int count) {
            if(buffer==null || offset<0 || count<0 || offset>buffer.Length-count)throw new ArgumentException();if(count==0)return 0;
            lock(gate) {
                while(current==null) {
                    CheckDeadline();if(fatal!=null)throw new IOException(fatal.Message,fatal);if(closed)throw new ObjectDisposedException("ReconnectStream");
                    if(received.Count>0){current=received.Dequeue();currentOffset=0;break;}Monitor.Wait(gate,50);
                }
                int n=Math.Min(count,current.Length-currentOffset);Array.Copy(current,currentOffset,buffer,offset,n);currentOffset+=n;queuedBytes-=n;
                if(currentOffset==current.Length)current=null;return n;
            }
        }
        public override void Write(byte[] buffer,int offset,int count) {
            if(buffer==null || offset<0 || count<1 || count>MaxPacket || offset>buffer.Length-count)throw new ArgumentException("RECONNECT_WRITE");
            lock(gate){CheckDeadline();if(fatal!=null)throw new IOException(fatal.Message,fatal);if(closed)throw new ObjectDisposedException("ReconnectStream");
                if(journal.Count>=MaxJournal)throw new IOException("RECONNECT_JOURNAL_LIMIT");var copy=new byte[count];Array.Copy(buffer,offset,copy,0,count);journal.Add(++sent,copy);changed.Set();}
        }
        protected override void Dispose(bool disposing) {
            Stream old;lock(gate){if(closed)return;
                // The application may close immediately after reading END.
                // Publish its wire acknowledgment before closing the socket.
                var drain=Stopwatch.StartNew();
                while(socket!=null && fatal==null && accepted>publishedAck && writer.IsAlive && drain.ElapsedMilliseconds<1000){changed.Set();Monitor.Wait(gate,20);}
                closed=true;old=socket;socket=null;Monitor.PulseAll(gate);changed.Set();}
            if(old!=null)try{old.Dispose();}catch(IOException){}
            if(Thread.CurrentThread!=reader)reader.Join(3500);if(Thread.CurrentThread!=writer)writer.Join(3500);
            // Threads may still be unwinding OS I/O. Keep their event alive.
            if(!reader.IsAlive && !writer.IsAlive)changed.Dispose();
            base.Dispose(disposing);
        }
        public override bool CanRead {get{return !closed;}}public override bool CanWrite {get{return !closed;}}public override bool CanSeek {get{return false;}}
        public override void Flush(){var wait=Stopwatch.StartNew();lock(gate){while(journal.Count>0 && socket!=null && fatal==null && !closed && wait.ElapsedMilliseconds<3000)Monitor.Wait(gate,20);if(journal.Count>0)throw new IOException("RECONNECT_FLUSH_UNCONFIRMED");}}
        public override long Length{get{throw new NotSupportedException();}}
        public override long Position{get{throw new NotSupportedException();}set{throw new NotSupportedException();}}
        public override long Seek(long offset,SeekOrigin origin){throw new NotSupportedException();}public override void SetLength(long value){throw new NotSupportedException();}
    }

    internal sealed class SessionReconnect : IDisposable {
        readonly ReconnectStream[] links;readonly TcpListener listener;
        readonly string session,identity;readonly string[] secrets;readonly int role,port;
        readonly IPAddress address;readonly Action<string> note;readonly Thread connector;
        volatile bool disposed;
        internal Stream[] Streams {get{return Array.ConvertAll(links,link=>(Stream)link);}}
        internal bool Recovering {get{foreach(var link in links)if(link.Recovering)return true;return false;}}
        internal Exception Failure {get{foreach(var link in links){Exception error=link.Failure;if(error!=null)return error;}return null;}}
        internal int RecoveryCount {get{int count=0;foreach(var link in links)count+=link.Recoveries;return count;}}
        internal SessionReconnect(NetworkStream[] original,TcpListener hostListener,IPAddress guestAddress,int guestPort,
            int seat,string sessionId,NetIdentity fingerprint,string[] keys,Action<string> report) {
            listener=hostListener;address=guestAddress;port=guestPort;role=seat;session=sessionId;identity=fingerprint.Encode();secrets=keys;note=report;
            links=new ReconnectStream[original.Length];
            for(int i=0;i<links.Length;++i){Configure(original[i]);links[i]=new ReconnectStream(original[i]);}
            connector=new Thread(role==0?(ThreadStart)AcceptLoop:ConnectLoop){IsBackground=true};connector.Start();
        }
        static void Configure(NetworkStream stream){stream.ReadTimeout=3000;stream.WriteTimeout=2000;}
        static bool SameSecret(string a,string b){if(a==null || b==null || a.Length!=b.Length)return false;int diff=0;for(int i=0;i<a.Length;++i)diff|=a[i]^b[i];return diff==0;}
        void AcceptLoop() {
            while(!disposed) {
                TcpClient client=null;
                try {
                    if(!listener.Pending()){Thread.Sleep(50);continue;}
                    client=listener.AcceptTcpClient();client.NoDelay=true;client.ReceiveTimeout=2000;client.SendTimeout=2000;
                    NetworkStream stream=client.GetStream();string[] request=NetWire.Read(stream).Split('|');int seat;
                    if(request.Length!=9 || request[0]!="RESUME5" || request[1]!=session || !Int32.TryParse(request[2],out seat) ||
                        seat<1 || seat>links.Length || !SameSecret(request[3],secrets[seat-1]) ||
                        String.Join("|",request,4,4)!=identity || request[8]!="END" || !links[seat-1].NeedsConnection)
                        throw new IOException("RECONNECT_AUTH");
                    NetWire.Send(stream,"RESUME_OK|"+session+"|"+seat);Configure(stream);
                    if(links[seat-1].Attach(stream)){client=null;note("P"+(seat+1)+" 已重新接入原会话。");}
                } catch(SocketException){if(disposed)return;}
                  catch(IOException){/* Invalid/late connections never create or replace a seat. */}
                  catch(ObjectDisposedException){return;}
                finally{if(client!=null)client.Close();}
            }
        }
        void ConnectLoop() {
            while(!disposed) {
                if(!links[0].NeedsConnection){Thread.Sleep(50);continue;}
                TcpClient client=null;
                try {
                    client=new TcpClient(AddressFamily.InterNetwork){NoDelay=true,ReceiveTimeout=2000,SendTimeout=2000};
                    var pending=client.BeginConnect(address,port,null,null);
                    using(pending.AsyncWaitHandle){if(!pending.AsyncWaitHandle.WaitOne(1000))throw new IOException("RECONNECT_CONNECT_TIMEOUT");client.EndConnect(pending);}
                    NetworkStream stream=client.GetStream();NetWire.Send(stream,"RESUME5|"+session+"|"+role+"|"+secrets[0]+"|"+identity+"|END");
                    if(NetWire.Read(stream)!="RESUME_OK|"+session+"|"+role)throw new IOException("RECONNECT_AUTH");
                    Configure(stream);if(links[0].Attach(stream)){client=null;note("已重新接入主机，正在校验并补齐输入。");}
                } catch(SocketException){}catch(IOException){}catch(ObjectDisposedException){return;}
                finally{if(client!=null)client.Close();}
                Thread.Sleep(250);
            }
        }
        // Diagnostic only; no production UI or automatic fault injection.
        internal void InterruptForTest(int socket){links[socket].Interrupt();}
        public void Dispose(){disposed=true;foreach(var link in links)link.Dispose();connector.Join(3500);}
    }
}
