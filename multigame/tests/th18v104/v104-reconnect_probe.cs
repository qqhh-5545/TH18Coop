using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using TH18LocalCoop.V104;

class ReconnectProbe {
    static int checks;
    static void Check(bool ok,string message){++checks;if(!ok)throw new Exception(message);}
    static void Wait(Func<bool> ready,string message,int ms=8000){var end=DateTime.UtcNow.AddMilliseconds(ms);while(DateTime.UtcNow<end){if(ready())return;Thread.Sleep(10);}throw new Exception(message);}
    static void Pair(TcpListener listener,out NetworkStream host,out NetworkStream guest){var c=new TcpClient();c.Connect(IPAddress.Loopback,((IPEndPoint)listener.LocalEndpoint).Port);var h=listener.AcceptTcpClient();c.NoDelay=h.NoDelay=true;host=h.GetStream();guest=c.GetStream();host.ReadTimeout=guest.ReadTimeout=2000;host.WriteTimeout=guest.WriteTimeout=2000;}
    static byte[] Read(Stream stream,int count){var bytes=new byte[count];for(int at=0;at<count;){int n=stream.Read(bytes,at,count-at);if(n==0)throw new EndOfStreamException();at+=n;}return bytes;}
    static void Put(Stream stream,int number){byte[] bytes=BitConverter.GetBytes(number);stream.Write(bytes,0,bytes.Length);}
    sealed class PartialWrite : Stream {
        readonly Stream inner;internal PartialWrite(Stream source){inner=source;}
        public override void Write(byte[] b,int o,int n){inner.Write(b,o,Math.Min(n,7));throw new IOException("injected partial wire write");}
        public override int Read(byte[] b,int o,int n){return inner.Read(b,o,n);}
        protected override void Dispose(bool disposing){inner.Dispose();base.Dispose(disposing);}
        public override bool CanRead{get{return true;}}public override bool CanWrite{get{return true;}}public override bool CanSeek{get{return false;}}
        public override void Flush(){}public override long Length{get{throw new NotSupportedException();}}public override long Position{get{throw new NotSupportedException();}set{throw new NotSupportedException();}}
        public override long Seek(long o,SeekOrigin k){throw new NotSupportedException();}public override void SetLength(long n){throw new NotSupportedException();}
    }
    static void JournalTest(){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();
        try{NetworkStream h,g;Pair(listener,out h,out g);
            using(var host=new ReconnectStream(new PartialWrite(h)))using(var guest=new ReconnectStream(g)){
                // Queue input while the first wire record is cut midway through
                // its header. Recovery must resend the complete journal once.
                for(int n=0;n<100;++n)Put(host,n);
                Wait(()=>host.Recovering && guest.Recovering,"partial write did not drop both links");
                Pair(listener,out h,out g);Check(host.Attach(h)&&guest.Attach(g),"reattach failed");
                var read=Task.Run(()=>{for(int n=0;n<100;++n)Check(BitConverter.ToInt32(Read(guest,4),0)==n,"duplicate, gap or reordered input");});
                Check(read.Wait(8000),"replayed journal stalled");read.GetAwaiter().GetResult();
                Put(host,100);Check(BitConverter.ToInt32(Read(guest,4),0)==100,"journal delivered a duplicate after recovery");
                for(int cycle=0;cycle<3;++cycle){host.Interrupt();guest.Interrupt();Pair(listener,out h,out g);Check(host.Attach(h)&&guest.Attach(g),"repeat attach failed");Put(guest,200+cycle);Check(BitConverter.ToInt32(Read(host,4),0)==200+cycle,"reverse replay failed");}
                Check(host.Recoveries==4 && guest.Recoveries==4,"recovery accounting");
            }
        }finally{listener.Stop();}
        Console.WriteLine("PASS partial packet replay, duplicate suppression, ordering, bidirectional repeated recovery");
    }
    static void AuthTest(){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();
        try{NetworkStream h,g;Pair(listener,out h,out g);int port=((IPEndPoint)listener.LocalEndpoint).Port;
            var identity=new NetIdentity{Core=new string('a',64),Mod=new string('b',64),Rules=new string('c',64)};
            string session=Guid.NewGuid().ToString("N"),secret=Guid.NewGuid().ToString("N");
            using(var host=new SessionReconnect(new[]{h},listener,null,0,0,session,identity,new[]{secret},s=>{}))
            using(var guest=new SessionReconnect(new[]{g},null,IPAddress.Loopback,port,1,session,identity,new[]{secret},s=>{})){
                Put(host.Streams[0],1);Check(BitConverter.ToInt32(Read(guest.Streams[0],4),0)==1,"initial channel failed");
                guest.InterruptForTest(0);
                // Input queued across a genuine automatic dial/auth/reattach.
                Put(guest.Streams[0],42);var result=Task.Run(()=>BitConverter.ToInt32(Read(host.Streams[0],4),0));
                Check(result.Wait(10000)&&result.Result==42,"authenticated automatic reconnect failed");
                Check(host.Failure==null && guest.Failure==null,"unexpected recovery failure");
            }
            // A caller with another token cannot reclaim even a disconnected seat.
            Pair(listener,out h,out g);
            using(var host=new SessionReconnect(new[]{h},listener,null,0,0,session,identity,new[]{secret},s=>{})){
                host.InterruptForTest(0);g.Dispose();
                using(var bad=new TcpClient()){bad.Connect(IPAddress.Loopback,port);bad.ReceiveTimeout=3000;var stream=bad.GetStream();
                    NetWire.Send(stream,"RESUME5|"+session+"|1|"+new string('0',32)+"|"+identity.Encode()+"|END");
                    bool rejected=false;try{rejected=stream.ReadByte()<0;}catch(IOException){rejected=true;}
                    Check(rejected && host.Recovering,"wrong token reclaimed an original seat");
                }
            }
        }finally{listener.Stop();}
        Console.WriteLine("PASS original session/seat authentication, automatic TCP redial, wrong-token rejection");
    }
    static void TimeoutTest(){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();
        try{NetworkStream h,g;Pair(listener,out h,out g);var originalGuest=g;using(originalGuest)using(var host=new ReconnectStream(h,200)){
            host.Interrupt();Wait(()=>host.Failure!=null,"recovery deadline did not stop");
            Check(host.Failure.Message.Contains("RECONNECT_TIMEOUT"),"timeout classification");
            bool rejected=false;try{Put(host,3);}catch(IOException){rejected=true;}Check(rejected,"expired session accepted input");
            Pair(listener,out h,out g);Check(!host.Attach(h),"expired session resumed anyway");g.Dispose();
        }}finally{listener.Stop();}
        Console.WriteLine("PASS bounded reconnect timeout and refusal to resume an expired session");
    }
    static void SilentReattachTest(){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();
        try{NetworkStream h,g;Pair(listener,out h,out g);var originalGuest=g;
            using(originalGuest)using(var host=new ReconnectStream(h,200)){
                host.Interrupt();Pair(listener,out h,out g);
                using(g){Check(host.Attach(h) && host.Recovering,"TCP attach alone incorrectly resumed the game");
                    Wait(()=>host.Failure!=null,"silent reattach reset the recovery deadline");
                    Check(host.Failure.Message.Contains("RECONNECT_TIMEOUT"),"silent reattach timeout classification");}
            }
        }finally{listener.Stop();}
        Console.WriteLine("PASS a silent TCP reattach neither resumes gameplay nor renews the recovery deadline");
    }
    static int Main(){try{JournalTest();AuthTest();TimeoutTest();SilentReattachTest();Console.WriteLine("PASS reconnect assertions="+checks);return 0;}catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
