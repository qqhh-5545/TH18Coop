using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace TH18LocalCoop {
    // Kept separate from the pinned TH18 adapter and its thcrap.ini.
    internal sealed class Th14Thcrap {
        internal bool Enabled;
        internal string Loader="",Config="";
        internal static Th14Thcrap Load(string root) {
            var settings=new Th14Thcrap();
            if(String.IsNullOrEmpty(root))return settings;
            string path=Path.Combine(root,"thcrap-th14.ini");if(!File.Exists(path))return settings;
            foreach(string line in File.ReadAllLines(path)) {
                int split=line.IndexOf('=');if(split<0)continue;
                string key=line.Substring(0,split).Trim(),value=line.Substring(split+1).Trim();
                if(key=="Enabled") {if(value!="0" && value!="1")throw new IOException("辉针城 thcrap 设置无效。");settings.Enabled=value=="1";}
                else if(key=="Loader")settings.Loader=value;
                else if(key=="Config")settings.Config=value;
            }
            return settings;
        }
        internal static string Quote(string value) {
            var result=new StringBuilder("\"");int slashes=0;
            foreach(char c in value) {
                if(c=='\\'){++slashes;continue;}
                if(c=='"')result.Append('\\',slashes*2+1).Append(c);
                else result.Append('\\',slashes).Append(c);
                slashes=0;
            }
            return result.Append('\\',slashes*2).Append('"').ToString();
        }
        internal ProcessStartInfo StartInfo(string entry,string dataRoot) {
            if(String.IsNullOrWhiteSpace(Loader) || !Path.IsPathRooted(Loader) ||
                !String.Equals(Path.GetFileName(Loader),"thcrap_loader.exe",StringComparison.OrdinalIgnoreCase) || !File.Exists(Loader))
                throw new IOException("请选择现有的 thcrap_loader.exe。");
            string loader=Path.GetFullPath(Loader),directory=Path.GetDirectoryName(loader);
            if(String.IsNullOrWhiteSpace(Config))throw new IOException("请选择 thcrap 的汉化配置（.js / .json）。");
            string config=Path.IsPathRooted(Config)?Config:Path.Combine(directory,Config);
            if(!File.Exists(config) && !Path.IsPathRooted(Config))config=Path.Combine(directory,"config",Config);
            string extension=Path.GetExtension(config);
            if(!File.Exists(config) || (!String.Equals(extension,".js",StringComparison.OrdinalIgnoreCase) && !String.Equals(extension,".json",StringComparison.OrdinalIgnoreCase)))
                throw new IOException("找不到有效的 thcrap 配置文件。");
            // Always patch the Japanese core, never stack thcrap on th14c.exe.
            string engine=Path.Combine(Path.GetDirectoryName(Path.GetFullPath(entry)),"th14.exe");
            var info=new ProcessStartInfo(loader) {UseShellExecute=false,WorkingDirectory=directory,
                Arguments=Quote(Path.GetFullPath(config))+" "+Quote(engine)};
            info.EnvironmentVariables["APPDATA"]=Path.GetFullPath(dataRoot);
            return info;
        }
        internal void Save(string root) {
            foreach(string value in new[]{Loader,Config})if(value.IndexOfAny(new[]{'\r','\n'})>=0)throw new IOException("路径不能包含换行。");
            string path=Path.Combine(root,"thcrap-th14.ini"),pending=path+".tmp";
            File.WriteAllText(pending,"[Thcrap]\nEnabled="+(Enabled?"1":"0")+"\nLoader="+Loader+"\nConfig="+Config+"\n",new UTF8Encoding(false));
            try {File.Copy(pending,path,true);}finally {if(File.Exists(pending))File.Delete(pending);}
        }
    }
    internal sealed class Th14ThcrapForm:Form {
        internal Th14ThcrapForm(string root,string entry) {
            Text="辉针城 thcrap 设置";Font=new Font("Microsoft YaHei UI",10);ClientSize=new Size(580,286);
            FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterParent;
            var settings=Th14Thcrap.Load(root);
            var enabled=new CheckBox {Text="使用 thcrap 启动辉针城（本地与在线）",Checked=settings.Enabled,Location=new Point(16,14),Size=new Size(548,26)};
            var loader=new TextBox {Text=settings.Loader,Location=new Point(16,74),Size=new Size(448,28)};
            var config=new TextBox {Text=settings.Config,Location=new Point(16,136),Size=new Size(448,28)};
            var pickLoader=new Button {Text="浏览…",Location=new Point(476,72),Size=new Size(88,30)};
            var pickConfig=new Button {Text="浏览…",Location=new Point(476,134),Size=new Size(88,30)};
            Action toggle=()=>{loader.Enabled=config.Enabled=pickLoader.Enabled=pickConfig.Enabled=enabled.Checked;};enabled.CheckedChanged+=delegate {toggle();};toggle();
            pickLoader.Click+=delegate {using(var dialog=new OpenFileDialog {Filter="thcrap loader|thcrap_loader.exe",CheckFileExists=true})if(dialog.ShowDialog(this)==DialogResult.OK)loader.Text=dialog.FileName;};
            pickConfig.Click+=delegate {using(var dialog=new OpenFileDialog {Filter="thcrap 配置|*.js;*.json",CheckFileExists=true})if(dialog.ShowDialog(this)==DialogResult.OK)config.Text=dialog.FileName;};
            var save=new Button {Text="保存",Location=new Point(368,242),Size=new Size(92,30)};
            save.Click+=delegate {try {
                settings.Enabled=enabled.Checked;settings.Loader=loader.Text.Trim();settings.Config=config.Text.Trim();
                if(settings.Enabled)settings.StartInfo(String.IsNullOrWhiteSpace(entry)?Path.Combine(root,"th14.exe"):entry,Path.Combine(root,"coop-data"));
                settings.Save(root);DialogResult=DialogResult.OK;Close();
            }catch(Exception error){MessageBox.Show(this,error.Message,"无法保存 thcrap 设置");}};
            var cancel=new Button {Text="取消",DialogResult=DialogResult.Cancel,Location=new Point(472,242),Size=new Size(92,30)};
            Controls.AddRange(new Control[]{enabled,loader,config,pickLoader,pickConfig,save,cancel,
                new Label {Text="thcrap_loader.exe",AutoSize=true,Location=new Point(16,50)},
                new Label {Text="汉化配置（例如 config\\zh-hans.js）",AutoSize=true,Location=new Point(16,112)},
                new Label {Text="启用后使用日文核心与所选补丁，保留独立联机存档。\n关闭后沿用“版本”选择；仅保存在本机，下次启动生效。",Location=new Point(16,178),Size=new Size(548,54)}});
        }
    }
}
