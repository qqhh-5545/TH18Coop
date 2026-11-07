using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using TH18LocalCoop;
class DataTest {
    static void Check(bool condition,string message) {
        if(!condition)throw new Exception(message);
    }
    static void TestSettings() {
        string fixture=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"TH18-settings-"+Guid.NewGuid().ToString("N"));
        string local=Path.Combine(fixture,"本地配置"),coop=Path.Combine(fixture,"联机 [settings]");
        string original=CoopData.ProfileDirectory(local),profile=CoopData.ProfileDirectory(coop);
        string source=Path.Combine(original,"th18.cfg"),target=Path.Combine(profile,"th18.cfg");
        string step="missing-source";
        try {
            CoopData.SyncLocalConfig(null,coop);
            CoopData.SyncLocalConfig(local,coop);
            Check(!Directory.Exists(coop),"Missing original config must not create a profile");
            step="fixture-directory";Directory.CreateDirectory(original);
            step="initial-source";
            File.WriteAllText(source,"settings-1");
            File.WriteAllText(Path.Combine(original,"scoreth18.dat"),"single-player-progress");
            Directory.CreateDirectory(Path.Combine(original,"replay"));
            File.WriteAllText(Path.Combine(original,"replay","single.rpy"),"single-replay");
            step="initial-sync";CoopData.SyncLocalConfig(local,coop);
            Check(File.ReadAllText(target)=="settings-1","Initial config was not imported");
            Check(Directory.GetFiles(profile,"*",SearchOption.AllDirectories).Length==1,"Imported files other than config");
            CoopData.SyncLocalConfig(local,coop);
            Check(!File.Exists(target+".before-local-sync"),"Identical settings must not be replaced");
            File.WriteAllText(Path.Combine(profile,"scoreth18.dat"),"coop-progress");
            File.WriteAllText(Path.Combine(profile,"scoreth18bak.dat"),"coop-progress-backup");
            Directory.CreateDirectory(Path.Combine(profile,"replay"));
            File.WriteAllText(Path.Combine(profile,"replay","coop.rpy"),"coop-replay");
            step="changed-sync";File.WriteAllText(source,"settings-2");
            CoopData.SyncLocalConfig(local,coop);
            Check(File.ReadAllText(target)=="settings-2","Changed custom settings must apply on next sync");
            Check(File.ReadAllText(target+".before-local-sync")=="settings-1","Old private config not backed up");
            File.WriteAllText(source,"settings-3");
            CoopData.SyncLocalConfig(local,coop);
            Check(File.ReadAllText(target)=="settings-3","Repeated setting updates must apply");
            Check(File.ReadAllText(target+".before-local-sync")=="settings-1","First private config backup overwritten");
            Check(File.ReadAllText(source)=="settings-3","Original custom config modified");
            Check(File.ReadAllText(Path.Combine(original,"scoreth18.dat"))=="single-player-progress","Single-player progress modified");
            Check(File.ReadAllText(Path.Combine(profile,"scoreth18.dat"))=="coop-progress","Coop progress modified");
            Check(File.ReadAllText(Path.Combine(profile,"scoreth18bak.dat"))=="coop-progress-backup","Coop backup modified");
            Check(File.ReadAllText(Path.Combine(profile,"replay","coop.rpy"))=="coop-replay","Coop replay modified");
            File.Delete(source);
            CoopData.SyncLocalConfig(local,coop);
            Check(File.ReadAllText(target)=="settings-3","Missing source overwrote private config");
            CoopData.SyncLocalConfig(coop,coop);
            Check(Directory.GetFiles(profile,"*.tmp").Length==0,"Temporary settings file left behind");
        } catch(Exception ex) {Console.Error.WriteLine("Settings test step: "+step+" ("+ex.GetType().Name+")");throw;}
        finally {if(Directory.Exists(fixture))Directory.Delete(fixture,true);}
        Console.WriteLine("PASS custom config initial/repeated sync, missing/identical source, first backup, score/replay isolation");
    }
    [DllImport("kernel32.dll",EntryPoint="GetEnvironmentVariableA",CharSet=CharSet.Ansi)]
    static extern uint NativeEnv(string name,byte[] bytes,uint size);
    static int Main(string[] args) {
        if(args.Length==2 && args[0]=="--check-entry") {try{Console.WriteLine(GameSelection.Validate(args[1]));return 0;}catch(Exception e){Console.WriteLine(e.Message);return 2;}}
        if(args.Length==2 && args[0]=="--check-launch") {var info=CoopData.StartInfo(args[1],Path.Combine(Path.GetTempPath(),"TH18-readonly-plan"));Console.WriteLine(info.FileName);Console.WriteLine(info.Arguments);Console.WriteLine(info.EnvironmentVariables["APPDATA"]);return 0;}
        try {return Run(args);}
        catch(Exception ex) {Console.Error.WriteLine(ex.GetType().Name+": "+ex.Message);return 10;}
    }
    static int Run(string[] args) {
        if(args.Length==1 && args[0]=="--settings-only") {TestSettings();return 0;}
        if(args.Length==1 && args[0]=="--child") {
            var bytes=new byte[4096];uint n=NativeEnv("APPDATA",bytes,4096);
            if(n==0 || n>=4096)return 2;
            Console.WriteLine(Convert.ToBase64String(bytes,0,(int)n));return 0;
        }
        Check(GameSelection.CanonicalCore(GameSelection.SteamCoreHash)==GameSelection.CoreHash,"Steam and standalone canonical core identity");
        Check(!GameSelection.SupportedCore(new string('0',64)),"unknown core still rejected");
        bool rejected=false;try {GameSelection.CanonicalCore(new string('0',64));}catch(IOException){rejected=true;}Check(rejected,"unknown core cannot use canonical identity");
        string parent=Environment.GetEnvironmentVariable("APPDATA");
        string root=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"新联机档 [fresh]","coop-data");
        string entry=Assembly.GetExecutingAssembly().Location;
        var info=CoopData.StartInfo(entry,root);
        if(info.UseShellExecute || info.WorkingDirectory!=Path.GetDirectoryName(entry) ||
           info.EnvironmentVariables["APPDATA"]!=root || Environment.GetEnvironmentVariable("APPDATA")!=parent)return 3;
        info.Arguments="--child";info.CreateNoWindow=true;info.RedirectStandardOutput=true;
        using(var child=Process.Start(info)) {
            string result=child.StandardOutput.ReadToEnd().Trim();child.WaitForExit();if(child.ExitCode!=0)return 4;
            string nativeRoot=Encoding.Default.GetString(Convert.FromBase64String(result));
            string observed=Path.Combine(nativeRoot,"ShanghaiAlice","th18")+"\\";
            if(!CoopData.MatchesNativePath(Encoding.Default.GetBytes(observed+"\0"),root))return 5;
        }
        if(CoopData.MatchesNativePath(Encoding.Default.GetBytes(parent+"\\ShanghaiAlice\\th18\0"),root))return 6;
        if(CoopData.MatchesNativePath(Encoding.Default.GetBytes(CoopData.ProfileDirectory(root)+"-other\0"),root))return 7;
        if(CoopData.MatchesNativePath(Encoding.Default.GetBytes(CoopData.ProfileDirectory(root)),root))return 8;
        Check(CoopData.MatchesNativePath(Encoding.UTF8.GetBytes(CoopData.ProfileDirectory(root)+"\0"),root),"thcrap UTF8 profile with Chinese path");
        if(Directory.Exists(root))return 9; // No save seeding or migration during environment preparation.
        TestSettings();
        string[] roundtrip={@"C:\path with spaces\",@"quote""and\tail",@"D:\普通汉化\file.js"};
        string[] decoded=CoopData.WindowsArgs("test "+String.Join(" ",Array.ConvertAll(roundtrip,CoopData.QuoteArg)));
        for(int i=0;i<roundtrip.Length;++i)Check(decoded[i+1]==roundtrip[i],"Windows argument quoting roundtrip");
        string fixture=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"steam-launch-"+Guid.NewGuid().ToString("N"));
        try {
            Directory.CreateDirectory(Path.Combine(fixture,"config"));
            string loader=Path.Combine(fixture,"thcrap_loader.exe"),config=Path.Combine(fixture,"config","中文 test.js");
            File.WriteAllText(loader,"");File.WriteAllText(config,"{}");
            string option=CoopData.QuoteArg(loader)+" "+CoopData.QuoteArg("中文 test.js")+" %command%";
            var plan=CoopData.ThcrapInfo(@"C:\fake game\th18.exe",root,option);
            string[] argv=CoopData.WindowsArgs("test "+plan.Arguments);
            Check(plan.FileName==loader && plan.WorkingDirectory==fixture && argv.Length==3 && argv[1]==config && argv[2]==@"C:\fake game\th18.exe","thcrap direct plan, config-directory lookup and exact game");
            Check(plan.EnvironmentVariables["APPDATA"]==root && Environment.GetEnvironmentVariable("APPDATA")==parent,"child-only thcrap profile environment");
            string steam=Path.Combine(fixture,"Steam"),steamapps=Path.Combine(steam,"steamapps"),fakeGame=Path.Combine(steamapps,"common","th18");
            Directory.CreateDirectory(fakeGame);string user=Path.Combine(steam,"userdata","42","config");Directory.CreateDirectory(user);
            File.WriteAllText(Path.Combine(steamapps,"appmanifest_1566410.acf"),"\"AppState\" { \"LastOwner\" \"76561197960265770\" }");
            string escaped=option.Replace("\\","\\\\").Replace("\"","\\\"");
            File.WriteAllText(Path.Combine(user,"localconfig.vdf"),"// comment\n\"UserLocalConfigStore\" { \"Software\" { \"Valve\" { \"Steam\" { \"apps\" { \"1566410\" { \"LaunchOptions\" \""+escaped+"\" } } } } } }");
            Check(CoopData.SteamOptions(Path.Combine(fakeGame,"th18.exe"))==option,"scoped Steam app/owner VDF parsing and escaped paths");
            bool bad=false;try {CoopData.ThcrapInfo(@"C:\fake game\th18.exe",root,option+" --extra");}catch(IOException){bad=true;}Check(bad,"unrecognized extra launch options rejected");
            var settings=new ThcrapSettings {Mode="Custom",Loader=loader,Config=config};settings.Save(fixture);
            var reloaded=ThcrapSettings.Load(fixture);Check(reloaded.Mode=="Custom" && reloaded.Loader==loader && reloaded.Config==config,"machine-local thcrap settings roundtrip");
            var manual=CoopData.StartInfo(@"C:\fake game\th18.exe",root,fixture);Check(manual.FileName==loader && manual.EnvironmentVariables["APPDATA"]==root,"manual thcrap works without Steam");
            settings.Mode="Off";settings.Save(fixture);Check(CoopData.StartInfo(@"C:\fake game\th18.exe",root,fixture).FileName==@"C:\fake game\th18.exe","per-launcher thcrap opt-out");
            Console.WriteLine("PASS thcrap direct launch plan, Windows quoting, inherited profile and unsupported option rejection; no original game launched");
        } finally {Directory.Delete(fixture,true);}
        string chinese=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"汉化目录 [launch]","th18c.exe");
        var chineseInfo=CoopData.StartInfo(chinese,root);
        Check(chineseInfo.Arguments=="th18.exe" && chineseInfo.WorkingDirectory==Path.GetDirectoryName(chinese),"Chinese direct-launch argument and exact game working directory");
        Console.WriteLine("PASS Chinese entry uses its direct launch argument without starting a process");
        Console.WriteLine("PASS child-only APPDATA, native ANSI observation with Chinese/spaces, local profile matching, no parent changes or save migration");
        return 0;
    }
}
