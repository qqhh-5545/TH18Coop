using System;
using System.IO;

namespace TH18LocalCoop.V104 {
    internal sealed class OnlineProfile {
        internal const int MaxScore=2*1024*1024;
        internal readonly string DataRoot,ProfileDirectory;
        readonly string permanent;
        readonly string baseline;
        readonly bool host;
        internal bool IsHost {get{return host;}}
        internal readonly bool ReplayOnly;
        internal static void ValidateScore(byte[] score) {
            if(score.Length==0)return;
            if(score.Length<24 || score.Length>MaxScore || score[0]!='T' || score[1]!='H' || score[2]!='8' || score[3]!='1')throw new IOException("联机存档格式或大小不支持。");
        }
        internal static byte[] Snapshot(string gameRoot) {
            string path=Path.Combine(CoopData.ProfileDirectory(Path.Combine(gameRoot,"coop-data")),"scoreth18.dat");
            if(!File.Exists(path))return new byte[0];
            if(new FileInfo(path).Length>MaxScore)throw new IOException("联机存档过大。");
            var score=File.ReadAllBytes(path);ValidateScore(score);return score;
        }
        internal OnlineProfile(string gameRoot,string session,int role,byte[] score,string localAppData,bool replayOnly=false) {
            if(session.Length!=32)throw new ArgumentException("Invalid session identifier");
            foreach(char c in session)if(!((c>='a' && c<='f')||(c>='0' && c<='9')))throw new ArgumentException("Invalid session identifier");
            ValidateScore(score);ReplayOnly=replayOnly;host=role==0 && !replayOnly;
            permanent=CoopData.ProfileDirectory(Path.Combine(gameRoot,"coop-data"));
            string current=Path.Combine(permanent,"scoreth18.dat");baseline=File.Exists(current)?NetIdentity.Hash(File.ReadAllBytes(current)):null;
            if(host && baseline!=(score.Length==0?null:NetIdentity.Hash(score)))throw new IOException("联机档在连接期间发生变化，请关闭其他游戏后重试。");
            if(role<0 || role>2)throw new IOException("ONLINE_ROSTER");
            DataRoot=replayOnly?Path.Combine(gameRoot,"coop-data","replay-sessions",session,"viewer"):Path.Combine(gameRoot,"coop-data","sessions",session,host?"host":role==1?"guest":"guest-p3");
            if(Directory.Exists(DataRoot))throw new IOException("会话目录已存在，请重新连接。");
            ProfileDirectory=CoopData.ProfileDirectory(DataRoot);Directory.CreateDirectory(ProfileDirectory);
            Directory.CreateDirectory(Path.Combine(ProfileDirectory,"replay"));Directory.CreateDirectory(Path.Combine(ProfileDirectory,"snapshot"));
            if(score.Length>0) {
                File.WriteAllBytes(Path.Combine(ProfileDirectory,"scoreth18.dat"),score);
                File.WriteAllBytes(Path.Combine(ProfileDirectory,"scoreth18bak.dat"),score);
            }
            CoopData.SyncLocalConfig(localAppData,DataRoot);
        }
        internal void CommitHost() {
            if(!host)return;
            string source=Path.Combine(ProfileDirectory,"scoreth18.dat"),target=Path.Combine(permanent,"scoreth18.dat");
            if(!File.Exists(source))return;
            byte[] result=File.ReadAllBytes(source);ValidateScore(result);
            if(result.Length==0)throw new IOException("会话成绩为空，未覆盖长期档。");
            string nativeBackup=Path.Combine(ProfileDirectory,"scoreth18bak.dat");
            byte[] backupResult=File.Exists(nativeBackup)?File.ReadAllBytes(nativeBackup):null;
            if(backupResult!=null){ValidateScore(backupResult);if(backupResult.Length==0)backupResult=null;}
            string current=File.Exists(target)?NetIdentity.Hash(File.ReadAllBytes(target)):null;
            if(current!=baseline)throw new IOException("联机长期档已被其他游戏修改，本次进度保留在会话目录，未覆盖长期档。");
            Directory.CreateDirectory(permanent);
            string backup=Path.Combine(permanent,"before-online-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8));
            Directory.CreateDirectory(backup);
            foreach(string name in new[]{"scoreth18.dat","scoreth18bak.dat"}) {
                string path=Path.Combine(permanent,name);if(File.Exists(path))File.Copy(path,Path.Combine(backup,name));
            }
            string pending=target+"."+Guid.NewGuid().ToString("N")+".tmp";
            try {
                File.WriteAllBytes(pending,result);
                if(File.Exists(target))File.Replace(pending,target,null);else File.Move(pending,target);
            } finally {if(File.Exists(pending))File.Delete(pending);}
            // Preserve the native session backup independently of the main file.
            if(backupResult!=null)File.WriteAllBytes(Path.Combine(permanent,"scoreth18bak.dat"),backupResult);
        }
        internal void UseLocalTestWindow() {
            string path=Path.Combine(ProfileDirectory,"th18.cfg");
            if(!File.Exists(path))throw new IOException("请先用游戏目录中的 custom.exe 保存一次配置，再运行本机双端测试。");
            byte[] settings=File.ReadAllBytes(path);
            if(settings.Length!=136 || BitConverter.ToUInt32(settings,0)!=0x180002)throw new IOException("本机测试配置格式不匹配，未改动原配置。");
            // custom.exe 0x4012a5: checked control 0xd8 (Window 640x480)
            // stores 3 at config+0x47. Resource-list order is not the encoding.
            settings[0x47]=3;
            // custom.exe checkbox 0xca: ask for a display mode at each startup.
            settings[0x51]=(byte)(settings[0x51]&~1);
            File.WriteAllBytes(path,settings);
        }
    }
}
