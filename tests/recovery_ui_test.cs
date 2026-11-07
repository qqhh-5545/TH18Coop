using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;
using TH18LocalCoop;

class RecoveryUiProbe {
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static void Check(bool ok,string text){if(!ok)throw new Exception(text);}
    [STAThread] static int Main(){
        try{
            string root=Path.GetFullPath("research/recovery-ui"),token=Guid.NewGuid().ToString("N");
            var identity=new NetIdentity{Core=new string('a',64),Mod=new string('b',64),Rules=new string('c',64)};
            using(var channel=new RollbackTransport(token,0,2,Guid.NewGuid().ToString("N")))
            using(var map=MemoryMappedFile.OpenExisting("Local\\TH18CoopRollback-"+token))
            using(var view=map.CreateViewAccessor())
            using(var session=new OnlineSession(Path.Combine(root,"th18.exe"),root,role=>null,identity))
            using(var form=new NetworkForm(Path.Combine(root,"th18.exe"),root)){
                typeof(OnlineSession).GetField("pausedAfterFailure",Private).SetValue(session,true);
                typeof(OnlineSession).GetField("activeRollback",Private).SetValue(session,channel);
                typeof(NetworkForm).GetField("session",Private).SetValue(form,session);
                typeof(NetworkForm).GetMethod("FinishConnection",Private).Invoke(form,new object[]{session});
                var stop=(Button)typeof(NetworkForm).GetField("stop",Private).GetValue(form);
                var host=(Button)typeof(NetworkForm).GetField("host",Private).GetValue(form);
                var join=(Button)typeof(NetworkForm).GetField("join",Private).GetValue(form);
                var cancellation=(CancellationTokenSource)typeof(OnlineSession).GetField("stopped",Private).GetValue(session);
                Check(session.PausedAfterFailure && !cancellation.IsCancellationRequested && view.ReadUInt32(52)==0,"UI automatically ended faulted native game");
                Check(stop.Enabled && stop.Text=="结束本局" && !host.Enabled && !join.Enabled,"fault action or duplicate-start gating missing");
                typeof(Button).GetMethod("OnClick",Private).Invoke(stop,new object[]{EventArgs.Empty});
                Check(view.ReadUInt32(52)==1 && cancellation.IsCancellationRequested,"explicit End did not request native exit");
                Check(host.Enabled && join.Enabled && !stop.Enabled && typeof(NetworkForm).GetField("session",Private).GetValue(form)==null,"UI did not release completed paused session");
            }
            Console.WriteLine("PASS WinForms fault retention, End action, duplicate-start gating and explicit native cleanup; no game launched");return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
