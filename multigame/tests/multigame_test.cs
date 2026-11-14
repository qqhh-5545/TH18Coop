using System;
using System.IO;
using System.Text;
using TH18LocalCoop;
class MultiGameTest {
    static void Check(bool value,string text){if(!value)throw new Exception(text);}
    static void Reject(Action call,string text){try{call();}catch(IOException){return;}throw new Exception(text);}
    static byte[] Score(string game,byte marker){byte[] b=new byte[24];Encoding.ASCII.GetBytes(game=="th14"?"TH41":"TH81").CopyTo(b,0);b[23]=marker;return b;}
    static int Main() {
        string root=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"multigame-"+Guid.NewGuid().ToString("N"));
        try {
            string local=Path.Combine(root,"local"),game=Path.Combine(root,"game"),privateRoot=Path.Combine(root,"private");
            string original=Th14Launcher.Profile(local);Directory.CreateDirectory(original);
            byte[] cfg=new byte[92];BitConverter.GetBytes(0x140002).CopyTo(cfg,0);cfg[0x1f]=7;cfg[0x20]=1;
            File.WriteAllBytes(Path.Combine(original,"th14.cfg"),cfg);File.WriteAllBytes(Path.Combine(original,"scoreth14.dat"),Score("th14",99));
            Th14Launcher.SyncConfig(local,privateRoot,true);
            byte[] windowed=File.ReadAllBytes(Path.Combine(Th14Launcher.Profile(privateRoot),"th14.cfg"));
            Check(windowed[0x1f]==3 && windowed[0x20]==0,"Private windowed config");
            Check(NetIdentity.Hash(File.ReadAllBytes(Path.Combine(original,"th14.cfg")))==NetIdentity.Hash(cfg),"Original config changed");
            Check(Directory.GetFiles(Th14Launcher.Profile(privateRoot)).Length==1,"Single-player progress imported");
            string previousTestFlag=Environment.GetEnvironmentVariable("TH14_COOP_TEST");
            try {
                Environment.SetEnvironmentVariable("TH14_COOP_TEST",null);
                Th14Launcher.SyncLaunchConfig(local,privateRoot);
                Check(NetIdentity.Hash(File.ReadAllBytes(Path.Combine(Th14Launcher.Profile(privateRoot),"th14.cfg")))==NetIdentity.Hash(cfg),"Normal local launch overwrote custom.exe display/startup settings");
                cfg[0x1f]=1;cfg[0x20]=0;cfg[0x18]=42;File.WriteAllBytes(Path.Combine(original,"th14.cfg"),cfg);
                Th14Launcher.SyncLaunchConfig(local,privateRoot);
                Check(NetIdentity.Hash(File.ReadAllBytes(Path.Combine(Th14Launcher.Profile(privateRoot),"th14.cfg")))==NetIdentity.Hash(cfg),"Updated custom.exe settings did not apply on next local launch");
                Environment.SetEnvironmentVariable("TH14_COOP_TEST","1");
                Th14Launcher.SyncLaunchConfig(local,privateRoot);
                byte[] testConfig=File.ReadAllBytes(Path.Combine(Th14Launcher.Profile(privateRoot),"th14.cfg"));
                Check(testConfig[0x1f]==3 && testConfig[0x20]==0 && testConfig[0x18]==42,"Automated test must override only private display settings");
                Check(NetIdentity.Hash(File.ReadAllBytes(Path.Combine(original,"th14.cfg")))==NetIdentity.Hash(cfg),"Test launch changed original custom.exe settings");
            } finally {Environment.SetEnvironmentVariable("TH14_COOP_TEST",previousTestFlag);}
            Check(GameCatalog.TH14.PathFile!=GameCatalog.TH18.PathFile,"Remembered paths collide");
            var start=Th14Launcher.StartInfo(Path.Combine(game,"th14c.exe"),privateRoot);
            Check(start.Arguments=="th14.exe" && start.WorkingDirectory==game && start.EnvironmentVariables["APPDATA"]==privateRoot,"Chinese direct launch/private environment");
            string patchRoot=Path.Combine(root,"patch space 中文"),modRoot=Path.Combine(root,"launcher");Directory.CreateDirectory(Path.Combine(patchRoot,"config"));Directory.CreateDirectory(modRoot);
            string loader=Path.Combine(patchRoot,"thcrap_loader.exe"),patchConfig=Path.Combine(patchRoot,"config","zh-hans.js");File.WriteAllBytes(loader,new byte[0]);File.WriteAllText(patchConfig,"{}");
            var patch=new Th14Thcrap{Enabled=true,Loader=loader,Config="zh-hans.js"};patch.Save(modRoot);
            var patched=Th14Launcher.StartInfo(Path.Combine(game,"th14c.exe"),privateRoot,modRoot);
            Check(patched.FileName==loader && patched.WorkingDirectory==patchRoot && patched.EnvironmentVariables["APPDATA"]==privateRoot,"thcrap private launch environment");
            Check(patched.Arguments==Th14Thcrap.Quote(patchConfig)+" "+Th14Thcrap.Quote(Path.Combine(game,"th14.exe")),"thcrap must patch the Japanese core with correctly quoted paths");
            Check(!File.Exists(Path.Combine(modRoot,"thcrap.ini")),"TH14 modified TH18 patch settings");
            patch.Enabled=false;patch.Save(modRoot);Check(Th14Launcher.StartInfo(Path.Combine(game,"th14c.exe"),privateRoot,modRoot).Arguments=="th14.exe","Disabling thcrap lost CN direct entry");
            patch.Enabled=true;patch.Loader=Path.Combine(patchRoot,"missing.exe");Reject(()=>patch.StartInfo(Path.Combine(game,"th14.exe"),privateRoot),"Missing patch loader accepted");
            patch.Loader=loader;patch.Config="missing.js";Reject(()=>patch.StartInfo(Path.Combine(game,"th14.exe"),privateRoot),"Missing patch config accepted");
            string unicodeProfile=Path.Combine(root,"独立联机", "ShanghaiAlice", "th14");
            byte[] pathBytes=new byte[4096];Encoding.UTF8.GetBytes(unicodeProfile+"\0").CopyTo(pathBytes,0);
            Check(Th14Launcher.MatchesProfile(pathBytes,Path.Combine(root,"独立联机")),"thcrap UTF8 private profile not recognized");
            Check(!Th14Launcher.MatchesProfile(pathBytes,privateRoot),"Unrelated patch instance accepted");
            Reject(()=>OnlineProfile.ValidateScore(Score("th18",1),"th14"),"TH18 score accepted as TH14");
            Reject(()=>OnlineProfile.ValidateScore(Score("th14",1),"th18"),"TH14 score accepted as TH18");
            Reject(()=>OnlineProfile.ValidateScore(new byte[0],"invalid"),"Unknown profile accepted");
            var host=new OnlineProfile(game,Guid.NewGuid().ToString("N"),0,new byte[0],local,gameId:"th14");
            Check(NetIdentity.Hash(File.ReadAllBytes(Path.Combine(host.ProfileDirectory,"th14.cfg")))==NetIdentity.Hash(cfg),"Normal online profile overwrote custom.exe settings");
            Check(!File.Exists(Path.Combine(host.ProfileDirectory,"scoreth14.dat")),"Fresh online score was imported");
            string unrelated=Path.Combine(game,"coop-data","ShanghaiAlice","th18","scoreth18.dat");Directory.CreateDirectory(Path.GetDirectoryName(unrelated));File.WriteAllBytes(unrelated,Score("th18",77));
            File.WriteAllBytes(Path.Combine(host.ProfileDirectory,"scoreth14.dat"),Score("th14",1));host.CommitHost();
            Check(OnlineProfile.Snapshot(game,"th14")[23]==1,"Host progress was not committed");
            Check(File.ReadAllBytes(unrelated)[23]==77,"TH18 progress was changed");
            var guest=new OnlineProfile(game,Guid.NewGuid().ToString("N"),1,Score("th14",1),local,gameId:"th14");
            File.WriteAllBytes(Path.Combine(guest.ProfileDirectory,"scoreth14.dat"),Score("th14",2));guest.CommitHost();
            Check(OnlineProfile.Snapshot(game,"th14")[23]==1,"Guest overwrote long-term progress");
            var conflict=new OnlineProfile(game,Guid.NewGuid().ToString("N"),0,Score("th14",1),local,gameId:"th14");
            File.WriteAllBytes(Path.Combine(conflict.ProfileDirectory,"scoreth14.dat"),Score("th14",3));
            File.WriteAllBytes(Path.Combine(game,"coop-data","ShanghaiAlice","th14","scoreth14.dat"),Score("th14",4));
            Reject(()=>conflict.CommitHost(),"Concurrent save modification was overwritten");
            Check(File.ReadAllBytes(Path.Combine(original,"scoreth14.dat"))[23]==99,"Original single-player score changed");
            var a=new NetIdentity{Build="th14-0.1-dev",Core=GameCatalog.TH14.CoreHash,Mod=new string('a',64),Rules=new string('b',64)};
            var b=new NetIdentity{Build="1.0.0",Core=GameCatalog.TH18.CoreHash,Mod=a.Mod,Rules=a.Rules};
            Check(a.Difference(b)!=null && a.Difference(NetIdentity.Decode(a.Encode()))==null,"Cross-game identity check");
            Console.WriteLine("PASS multi-game: separate config/score/path, CN direct launch, fresh TH14 profile, host commit, guest isolation, conflict preservation and cross-game rejection");return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
        finally {if(Directory.Exists(root))Directory.Delete(root,true);}
    }
}
