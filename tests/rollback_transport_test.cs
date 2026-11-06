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
    static void Main(){try{ThreeSeats();RejectForgedSeat();}catch(Exception error){Console.Error.WriteLine(error);Environment.Exit(1);}}
    static void ThreeSeats(){
        var listener=new TcpListener(IPAddress.Loopback,0);listener.Start(2);int port=((IPEndPoint)listener.LocalEndpoint).Port;
        var clients=new TcpClient[2];var hosts=new TcpClient[2];var channels=new RollbackTransport[3];var maps=new MemoryMappedFile[3];var views=new MemoryMappedViewAccessor[3];
        var exited=new int[3];var run=new Task[3];var native=new Task[3];var cancel=new CancellationTokenSource();string session=Guid.NewGuid().ToString("N");
        try{
            for(int i=0;i<2;++i){clients[i]=new TcpClient();clients[i].Connect(IPAddress.Loopback,port);hosts[i]=listener.AcceptTcpClient();clients[i].NoDelay=hosts[i].NoDelay=true;}
            for(int seat=0;seat<3;++seat){int own=seat;string token=Guid.NewGuid().ToString("N");channels[seat]=new RollbackTransport(token,seat,3,session);maps[seat]=MemoryMappedFile.OpenExisting("Local\\TH18CoopRollback-"+token);views[seat]=maps[seat].CreateViewAccessor();views[seat].Write(4,1);
                NetworkStream[] streams=seat==0?new[]{hosts[0].GetStream(),hosts[1].GetStream()}:new[]{clients[seat-1].GetStream()};
                run[seat]=Task.Run(()=>channels[own].Run(streams,()=>Thread.VolatileRead(ref exited[own])!=0,cancel.Token,s=>{},f=>{}));
            }
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
            Console.WriteLine("PASS rollback transport:3 real TCP endpoints,600 routed packets,64-slot ring wrap,seat ownership,byte-exact delivery,clean peer END");
        }finally{cancel.Cancel();foreach(var client in clients)if(client!=null)client.Close();foreach(var host in hosts)if(host!=null)host.Close();foreach(var channel in channels)if(channel!=null)channel.Dispose();foreach(var view in views)if(view!=null)view.Dispose();foreach(var map in maps)if(map!=null)map.Dispose();listener.Stop();cancel.Dispose();}
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
