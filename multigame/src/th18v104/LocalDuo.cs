using System;
using System.IO;
using System.Net;
using System.Threading.Tasks;

namespace TH18LocalCoop.V104 {
    internal sealed class LocalDuo : IDisposable {
        readonly OnlineSession[] sessions;
        readonly string entry;
        readonly bool realGames;
        internal LocalDuo(string gameEntry,string root,Func<int,IOnlineGame> factory=null,NetIdentity identity=null,string appData=null,bool sharedKeyboard=false,int players=2,bool realGameTests=false) {
            entry=gameEntry;realGames=factory==null || realGameTests;
            if(players<2 || players>3)throw new ArgumentException("ONLINE_ROSTER");
            sessions=new OnlineSession[players];
            for(int i=0;i<players;++i)sessions[i]=new OnlineSession(entry,root,factory,identity,appData,true,sharedKeyboard && players==2,players);
        }
        internal async Task Run(Action<string> note) {
            if(realGames)OnlineGame.RequireNoRunningGame(Path.GetDirectoryName(entry));
            int port=sessions[0].Listen(IPAddress.Loopback,0);string code=NetSession.NewCode();
            note(realGames?"本机测试将打开 "+sessions.Length+" 个隔离游戏并自动连接。分别聚焦各玩家窗口，用原版键位操作；测试进度不提交到长期档。":
                "后台模拟本机双端启动流程，不打开真实游戏；检查互斥、窗口会话配置和进度隔离。");
            var tasks=new Task[sessions.Length];
            tasks[0]=Task.Run(()=>sessions[0].Host(code,s=>note("[P1] "+s)));
            for(int i=1;i<sessions.Length;++i) {int index=i;tasks[i]=Task.Run(()=>sessions[index].Join("127.0.0.1",port,code,s=>note("[客机 "+index+"] "+s)));}
            Exception failure=null;bool normalEnd=false;
            try {
                Task ended=await Task.WhenAny(tasks);
                if(ended.IsFaulted)failure=ended.Exception.GetBaseException();
                else normalEnd=true;
            } catch(Exception error){failure=error;}
            if(failure==null || !PausedAfterFailure)Dispose();
            try {await Task.WhenAll(tasks);}catch {if(!normalEnd && failure==null)foreach(var task in tasks)if(task.IsFaulted){failure=task.Exception.GetBaseException();break;}}
            if(failure!=null)throw failure;
            note(realGames?"本机 "+sessions.Length+" 端测试已结束，长期进度未提交。请退出仍打开的测试游戏窗口。":"本机 "+sessions.Length+" 端模拟检查结束，未启动真实游戏。");
        }
        public void Dispose(){foreach(var session in sessions)session.Dispose();}
        internal bool PausedAfterFailure {get{foreach(var session in sessions)if(session.PausedAfterFailure)return true;return false;}}
        internal void InterruptForTest(int seat){sessions[seat].InterruptForTest(0);}
        internal void DesyncForTest(int seat,uint frame){sessions[seat].DesyncForTest(frame);}
    }
}
