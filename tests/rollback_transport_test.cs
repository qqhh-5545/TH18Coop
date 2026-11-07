using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using TH18LocalCoop;
class RollbackTransportTest {
    static void Check(bool ok,string label){if(!ok)throw new Exception(label);}
    static void Main(){try{ThreeSeats();ThreeSeats(true);RejectForgedSeat();PauseWithoutExit(0);PauseWithoutExit(1);ReconnectPauseFlag();}catch(Exception error){Console.Error.WriteLine(error);Environment.Exit(1);}}
    static void PauseWithoutExit(int failingSeat){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();
        var clients=new TcpClient[2];var hosts=new TcpClient[2];var channels=new RollbackTransport[3];
        var maps=new MemoryMappedFile[3];var views=new MemoryMappedViewAccessor[3];var runs=new Task<Exception>[3];var cancel=new CancellationTokenSource();
        try{
            for(int i=0;i<2;++i){clients[i]=new TcpClient();clients[i].Connect(IPAddress.Loopback,((IPEndPoint)listener.LocalEndpoint).Port);hosts[i]=listener.AcceptTcpClient();}
            string session=Guid.NewGuid().ToString("N");
            for(int seat=0;seat<3;++seat){int own=seat;string token=Guid.NewGuid().ToString("N");channels[seat]=new RollbackTransport(token,seat,3,session);
                maps[seat]=MemoryMappedFile.OpenExisting("Local\\TH18CoopRollback-"+token);views[seat]=maps[seat].CreateViewAccessor();views[seat].Write(4,1);
                Stream[] streams=seat==0?new Stream[]{hosts[0].GetStream(),hosts[1].GetStream()}:new Stream[]{clients[seat-1].GetStream()};
                runs[seat]=Task.Run<Exception>(()=>{try{channels[own].Run(streams,()=>false,cancel.Token,s=>{},f=>{});return null;}catch(Exception error){return error;}});
            }
            Thread.Sleep(100);views[failingSeat].Write(60,40097u);views[failingSeat].Write(80,1u);views[failingSeat].Write(48,54u);views[failingSeat].Write(4,4);
            Check(Task.WaitAll(runs,8000),"pause propagation timeout");
            for(int seat=0;seat<3;++seat){var error=runs[seat].Result as OnlinePauseException;
                Check(error!=null && error.Category=="desync" && error.Frame==40097,"all seats retain typed desync and original frame");
                Check(views[seat].ReadUInt32(52)==0 && views[seat].ReadUInt32(4)==4,"fault must pause without requesting native exit");
                channels[seat].RequestExit();Check(views[seat].ReadUInt32(52)==1,"explicit End must still exit paused game");
            }
            Console.WriteLine("PASS three-seat desync from P"+(failingSeat+1)+": broadcast frame40097, pause without automatic exit, explicit End preserved");
        }finally{cancel.Cancel();foreach(var c in clients)if(c!=null)c.Close();foreach(var c in hosts)if(c!=null)c.Close();foreach(var c in channels)if(c!=null)c.Dispose();foreach(var v in views)if(v!=null)v.Dispose();foreach(var m in maps)if(m!=null)m.Dispose();listener.Stop();cancel.Dispose();}
    }
    static void ReconnectPauseFlag(){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();var client=new TcpClient();client.Connect(IPAddress.Loopback,((IPEndPoint)listener.LocalEndpoint).Port);var host=listener.AcceptTcpClient();var cancel=new CancellationTokenSource();string token=Guid.NewGuid().ToString("N");int waiting=1;
        try{using(var channel=new RollbackTransport(token,0,2,Guid.NewGuid().ToString("N")))using(var map=MemoryMappedFile.OpenExisting("Local\\TH18CoopRollback-"+token))using(var view=map.CreateViewAccessor()){
            view.Write(4,1);var run=Task.Run(()=>channel.Run(new[]{host.GetStream()},()=>false,cancel.Token,s=>{},f=>{},()=>Thread.VolatileRead(ref waiting)!=0));
            Thread.Sleep(100);Check(view.ReadUInt32(84)==1 && view.ReadUInt32(52)==0,"reconnect should pause native without exiting");
            Interlocked.Exchange(ref waiting,0);Thread.Sleep(100);Check(view.ReadUInt32(84)==0 && view.ReadUInt32(4)==1,"reattach should release temporary pause");
            cancel.Cancel();try{run.Wait(4000);}catch(AggregateException){}
            Console.WriteLine("PASS reconnect pause flag set/cleared without faulting or exiting native game");
        }}finally{cancel.Cancel();client.Close();host.Close();listener.Stop();cancel.Dispose();}
    }
    static void ThreeSeats(bool reconnect=false){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start(2);int port=((IPEndPoint)listener.LocalEndpoint).Port;
        var clients=new TcpClient[2];var hosts=new TcpClient[2];var channels=new RollbackTransport[3];var maps=new MemoryMappedFile[3];var views=new MemoryMappedViewAccessor[3];
        var exited=new int[3];var run=new Task[3];var native=new Task[3];var cancel=new CancellationTokenSource();string session=Guid.NewGuid().ToString("N");
        var recoveries=new SessionReconnect[3];Task cut=null;
        try{
            for(int i=0;i<2;++i){clients[i]=new TcpClient();clients[i].Connect(IPAddress.Loopback,port);hosts[i]=listener.AcceptTcpClient();clients[i].NoDelay=hosts[i].NoDelay=true;}
            string[] secrets={Guid.NewGuid().ToString("N"),Guid.NewGuid().ToString("N")};
            var identity=new NetIdentity {Core=new string('a',64),Mod=new string('b',64),Rules=new string('c',64)};
            for(int seat=0;seat<3;++seat){int own=seat;string token=Guid.NewGuid().ToString("N");channels[seat]=new RollbackTransport(token,seat,3,session);maps[seat]=MemoryMappedFile.OpenExisting("Local\\TH18CoopRollback-"+token);views[seat]=maps[seat].CreateViewAccessor();views[seat].Write(4,1);
                NetworkStream[] streams=seat==0?new[]{hosts[0].GetStream(),hosts[1].GetStream()}:new[]{clients[seat-1].GetStream()};
                if(reconnect)recoveries[seat]=new SessionReconnect(streams,seat==0?listener:null,seat==0?null:IPAddress.Loopback,port,seat,session,identity,seat==0?secrets:new[]{secrets[seat-1]},s=>{});
                Stream[] wire=reconnect?recoveries[seat].Streams:streams;
                run[seat]=Task.Run(()=>channels[own].Run(wire,()=>Thread.VolatileRead(ref exited[own])!=0,cancel.Token,s=>{},f=>{},reconnect?(Func<bool>)(()=>recoveries[own].Recovering):null,reconnect?(Func<Exception>)(()=>recoveries[own].Failure):null));
            }
            if(reconnect)cut=Task.Run(()=>{while(views[1].ReadUInt32(64)<20){cancel.Token.ThrowIfCancellationRequested();Thread.Sleep(1);}recoveries[1].InterruptForTest(0);});
            for(int seat=0;seat<3;++seat){int own=seat;native[seat]=Task.Run(()=>{
                var view=views[own];uint sent=0,received=0;var seen=new bool[3,100];
                while(received<200 || sent<200){
                    if(sent<200){uint write=view.ReadUInt32(64),read=view.ReadUInt32(68);if(unchecked(write-read)<64){int to=(own+1+(int)(sent%2))%3;uint ordinal=sent/2;long at=128+(write%64)*1032;view.Write(at,(uint)to);view.Write(at+4,4u);view.WriteArray(at+8,new byte[]{0,(byte)own,(byte)to,(byte)ordinal},0,4);Thread.MemoryBarrier();view.Write(64,unchecked(write+1));++sent;}}
                    uint inputRead=view.ReadUInt32(76),inputWrite=view.ReadUInt32(72);
                    while(inputRead!=inputWrite){long at=128+64*1032+(inputRead%64)*1032;int from=(int)view.ReadUInt32(at);Check(view.ReadUInt32(at+4)==4,"relay payload length");var packet=new byte[4];view.ReadArray(at+8,packet,0,4);Check(from!=own && packet[0]==0 && packet[1]==from && packet[2]==own && packet[3]<100 && !seen[from,packet[3]],"seat ownership, exact payload and once-only delivery");seen[from,packet[3]]=true;++received;Thread.MemoryBarrier();view.Write(76,++inputRead);}
                    if(view.ReadUInt32(4)==4)throw new Exception("transport stopped during ring wrap");Thread.Sleep(1);
                }
                if(own!=1)while(view.ReadUInt32(52)==0){cancel.Token.ThrowIfCancellationRequested();Thread.Sleep(1);}
                view.Write(4,2);Interlocked.Exchange(ref exited[own],1);
            });}
            var all=new Task[6];Array.Copy(run,all,3);Array.Copy(native,0,all,3,3);
            Check(Task.WaitAll(all,15000),"three-seat transport timeout");
            Check(views[0].ReadUInt32(52)==1 && views[2].ReadUInt32(52)==1,"authenticated END reaches host and other guest");
            if(reconnect){Check(cut.Wait(1000) && recoveries[0].RecoveryCount>0 && recoveries[1].RecoveryCount>0,"original channel did not actually recover");Console.WriteLine("PASS authenticated three-seat TCP reconnect during600 routed packets: no loss/duplicates and clean END after journal flush");}
            Console.WriteLine("PASS rollback transport:3 real TCP endpoints,600 routed packets,64-slot ring wrap,seat ownership,byte-exact delivery,clean peer END");
        }finally{cancel.Cancel();foreach(var recovery in recoveries)if(recovery!=null)recovery.Dispose();foreach(var client in clients)if(client!=null)client.Close();foreach(var host in hosts)if(host!=null)host.Close();foreach(var channel in channels)if(channel!=null)channel.Dispose();foreach(var view in views)if(view!=null)view.Dispose();foreach(var map in maps)if(map!=null)map.Dispose();listener.Stop();cancel.Dispose();}
    }
    static void RejectForgedSeat(){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();var client=new TcpClient();client.Connect(IPAddress.Loopback,((IPEndPoint)listener.LocalEndpoint).Port);var host=listener.AcceptTcpClient();var cancel=new CancellationTokenSource();
        string token=Guid.NewGuid().ToString("N");
        try{using(var channel=new RollbackTransport(token,0,2,Guid.NewGuid().ToString("N"))){var run=Task.Run(()=>channel.Run(new[]{host.GetStream()},()=>false,cancel.Token,s=>{},f=>{}));
            client.GetStream().Write(new byte[]{0,0,1,0,0},0,5);bool refused=false;try{Check(run.Wait(5000),"forged-seat timeout");}catch(AggregateException error){for(Exception cause=error;cause!=null;cause=cause.InnerException)if(cause.Message=="ROLLBACK_RELAY_OWNERSHIP")refused=true;}
            Check(refused,"relay must reject a guest claiming P1");
            using(var map=MemoryMappedFile.OpenExisting("Local\\TH18CoopRollback-"+token))using(var view=map.CreateViewAccessor()){Check(view.ReadUInt32(52)==0,"no fabricated clean end after transport failure");channel.RequestExit();Check(view.ReadUInt32(52)==1,"explicit disconnect requests native cleanup even after transport failure");}
            Console.WriteLine("PASS rollback transport rejects forged sender before native delivery; explicit disconnect requests native cleanup");}}
        finally{cancel.Cancel();client.Close();host.Close();listener.Stop();cancel.Dispose();}
    }
}
