using System;
using System.Drawing;
using System.IO;

namespace TH18LocalCoop {
    internal sealed class GameDefinition {
        internal readonly string Id,Name,CoreHash,ChineseHash,Module,Loader;
        internal GameDefinition(string id,string name,string core,string chinese,string module,string loader) {
            Id=id;Name=name;CoreHash=core;ChineseHash=chinese;Module=module;Loader=loader;
        }
        internal string Engine {get{return Id+".exe";}}
        internal string ChineseEntry {get{return Id+"c.exe";}}
        internal Icon LoadIcon() {
            using(var stream=typeof(GameDefinition).Assembly.GetManifestResourceStream("GameIcons."+Id+".ico")) {
                if(stream==null)return null;
                using(var icon=new Icon(stream))return (Icon)icon.Clone();
            }
        }
        internal string PathFile {get{return Id=="th18"?"game-path.txt":"game-"+Id+"-path.txt";}}
        internal string Validate(string entry) {
            if(Id=="th18")return GameSelection.Validate(entry);
            if(String.IsNullOrWhiteSpace(entry))throw new IOException("请先选择"+Name+"游戏目录。");
            entry=Path.GetFullPath(entry);string dir=Path.GetDirectoryName(entry),name=Path.GetFileName(entry);
            if(!String.Equals(name,Engine,StringComparison.OrdinalIgnoreCase) && !String.Equals(name,ChineseEntry,StringComparison.OrdinalIgnoreCase))throw new IOException("请选择 "+Engine+" 或 "+ChineseEntry+"。");
            if(GameSelection.IsSteam(dir))throw new IOException("请选择独立安装的受支持版本。");
            if(entry.Length>=170)throw new IOException("游戏路径过长。");
            if(!File.Exists(Path.Combine(dir,Engine)) || GameSelection.Digest(Path.Combine(dir,Engine))!=CoreHash)throw new IOException(Name+"核心版本不匹配。");
            if(!File.Exists(entry) || GameSelection.Digest(entry)!=(String.Equals(name,ChineseEntry,StringComparison.OrdinalIgnoreCase)?ChineseHash:CoreHash))throw new IOException(Name+"启动入口版本不匹配。");
            return entry;
        }
        public override string ToString(){return Name;}
    }
    internal static class GameCatalog {
        internal static readonly GameDefinition TH18=new GameDefinition("th18","东方虹龙洞",GameSelection.CoreHash,GameSelection.ChineseHash,"th18_coop.dll","TH18LocalCoop.Console.exe");
        internal static readonly GameDefinition TH14=new GameDefinition("th14","东方辉针城","f08e885e28d2e0fc8c579e88034dd7de8f2a648d8f08ce781cf08eadc9ffe353","ec04fc884f658eee5096a3cff97688634216e23a52e29e77e0c272d5c069b2af","th14_coop.dll","TH14Coop.Console.exe");
        internal static readonly GameDefinition[] All={TH18,TH14};
    }
}
