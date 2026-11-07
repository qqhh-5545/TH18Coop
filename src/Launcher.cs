using System;

using System.Diagnostics;

using System.Drawing;

using System.IO;

using System.Linq;

using System.Runtime.InteropServices;

using System.Security.Cryptography;

using System.Text;

using System.Threading.Tasks;

using System.Windows.Forms;

namespace TH18LocalCoop {

    internal static class Launcher {

        internal static bool DeveloperTools;

        [STAThread] static void Main(string[] args) {

            DeveloperTools=args.Contains("--developer-tools");

            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);

            var form=new MainForm();

            if(args.Length==2 && args[0]=="--render-network") {

                using(form)using(var network=form.CreateNetworkWindow())using(var bitmap=new Bitmap(network.ClientSize.Width,network.ClientSize.Height)) {

                    using(var graphics=Graphics.FromImage(bitmap))graphics.Clear(network.BackColor);

                    RenderChildren(network,bitmap,Point.Empty);bitmap.Save(args[1]);

                }

                return;

            }

            if(args.Length==2 && args[0]=="--render-ui") {

                using(form) using(var bitmap=new Bitmap(form.ClientSize.Width,form.ClientSize.Height)) {

                    using(var graphics=Graphics.FromImage(bitmap)) graphics.Clear(form.BackColor);

                    RenderChildren(form,bitmap,Point.Empty);bitmap.Save(args[1]);

                }

                return;

            }

            if(args.Length==2 && args[0]=="--ui-shot") {

                form.Shown+=async delegate {await Task.Delay(300);using(var bitmap=new Bitmap(form.Width,form.Height)) {

                    form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,form.Size));bitmap.Save(args[1]);}form.Close();};

            }

            if(args.Length==2 && args[0]=="--smoke-start") {

                form.Shown+=async delegate {await form.Run();File.WriteAllText(args[1],form.StatusText,new UTF8Encoding(false));form.Close();};

            }

            Application.Run(form);

        }

        static void RenderChildren(Control parent,Bitmap bitmap,Point offset) {

            foreach(Control control in parent.Controls) {

                var position=new Point(offset.X+control.Left,offset.Y+control.Top);

                IntPtr handle=control.Handle;

                control.DrawToBitmap(bitmap,new Rectangle(position,control.Size));

                if(control is ComboBox) using(var graphics=Graphics.FromImage(bitmap))

                    graphics.DrawString(control.Text,control.Font,Brushes.Black,new PointF(position.X+5,position.Y+5));

                RenderChildren(control,bitmap,position);

            }

        }

    }

    internal sealed class MainForm : Form {

        const string SupportedHash="5aeb74b19939a29a8cf06e4fe7777aaf9139e93478507e039f43318171020e9f";

        const string ChineseHash="8c1dc7ebf7aab0c8962b2a71cb848d59ef180e1f0613a3c410ca043767044c5b";

        string GameDirectory {get{return path.Text.Trim().Trim('"');}}

        string SelectedEntry {get {

            string directory=GameDirectory;if(String.IsNullOrWhiteSpace(directory))return "";

            bool chinese=entryKind.SelectedIndex==1 || (entryKind.SelectedIndex==0 && File.Exists(Path.Combine(directory,"th18c.exe")));

            return Path.Combine(directory,chinese?"th18c.exe":"th18.exe");

        }}

        string DataRoot {get{return Path.Combine(GameDirectory,"coop-data");}}

        readonly string root=AppDomain.CurrentDomain.BaseDirectory;

        readonly TextBox path=new TextBox();

        readonly Label status=new Label();

        readonly ToolTip tips=new ToolTip();

        readonly ComboBox entryKind=new ComboBox();

        readonly Button start=new Button(),browse=new Button(),network=new Button(),replays=new Button(),help=new Button();

        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,int pid);

        [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr size,out UIntPtr count);

        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);

        public MainForm() {

            Text="虹龙洞联机 1.0.4（测试版）";

            Font=new Font("Microsoft YaHei UI",10);ClientSize=new Size(640,230);

            FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;

            BackColor=Color.FromArgb(244,246,249);

            var fileLabel=new Label {Text="游戏目录",AutoSize=true,Location=new Point(20,15)};

            path.Name="GameDirectory";path.SetBounds(20,41,508,30);browse.SetBounds(538,40,82,32);browse.Text="浏览…";

            var entryLabel=new Label {Text="版本",AutoSize=true,Location=new Point(20,91)};

            entryKind.DropDownStyle=ComboBoxStyle.DropDownList;entryKind.Items.AddRange(new object[]{"自动","汉化版","日文版"});entryKind.SelectedIndex=0;entryKind.SetBounds(73,86,131,30);

            browse.Click+=delegate {using(var dialog=new FolderBrowserDialog {Description="虹龙洞游戏目录",ShowNewFolderButton=false}) {

                if(Directory.Exists(GameDirectory))dialog.SelectedPath=GameDirectory;

                if(dialog.ShowDialog()==DialogResult.OK){path.Text=dialog.SelectedPath;entryKind.SelectedIndex=0;}}};

            start.Text="Local Co-op";start.SetBounds(20,132,192,40);

            start.Click+=async delegate {await Run();};

            network.Text="Online Co-op";network.SetBounds(224,132,192,40);

            network.Click+=delegate {try{ValidateSelection();using(var window=CreateNetworkWindow())window.ShowDialog(this);}catch(Exception error){Note(error.Message);}};
            replays.Text="Replay";replays.SetBounds(428,132,192,40);
            replays.Click+=async delegate {
                try {
                    string exe=ValidateSelection();start.Enabled=network.Enabled=replays.Enabled=browse.Enabled=entryKind.Enabled=path.Enabled=false;
                    await Task.Run(()=>ReplayViewer.Run(exe,root,text=>{if(!IsDisposed)BeginInvoke((Action)(()=>Note(text)));}));
                } catch(Exception error){Note(error.Message);}
                finally {start.Enabled=network.Enabled=replays.Enabled=browse.Enabled=entryKind.Enabled=path.Enabled=true;}
            };
            help.Text="?";help.SetBounds(588,86,32,30);tips.SetToolTip(help,"联机玩法与数据位置");
            help.Click+=delegate {using(var window=new HelpForm(GameDirectory))window.ShowDialog(this);};

            var patchSettings=new Button {Text="thcrap",Location=new Point(402,86),Size=new Size(84,30)};patchSettings.Click+=delegate {using(var window=new ThcrapForm(root))window.ShowDialog(this);};Controls.Add(patchSettings);
            var keySettings=new Button {Text="键位",Location=new Point(498,86),Size=new Size(78,30)};keySettings.Click+=delegate {using(var window=new KeyBindingsForm(root))window.ShowDialog(this);};Controls.Add(keySettings);
            status.SetBounds(20,188,600,33);status.AutoEllipsis=true;
            Controls.AddRange(new Control[]{fileLabel,path,browse,entryLabel,entryKind,start,network,replays,help,status});

            var settings=Path.Combine(root,"game-path.txt");

            try {

                if(File.Exists(settings)) {

                    string saved=File.ReadAllText(settings).Trim();string directory=Path.GetDirectoryName(saved);

                    if(Directory.Exists(directory)) {

                        path.Text=directory;

                        entryKind.SelectedIndex=String.Equals(Path.GetFileName(saved),"th18c.exe",StringComparison.OrdinalIgnoreCase)?1:2;

                    }

                }

            } catch { }

        }

        void Note(string text) {status.Text=text;tips.SetToolTip(status,text);}

        string ValidateSelection(){

            string exe=GameSelection.Validate(SelectedEntry);

            foreach(string file in new[]{"TH18LocalCoop.Console.exe","th18_coop.dll","coop.ini"})

                if(!File.Exists(Path.Combine(root,file)))throw new IOException("联机文件不完整，请重新解压压缩包。");

            path.Text=Path.GetDirectoryName(exe);

            File.WriteAllText(Path.Combine(root,"game-path.txt"),exe,new UTF8Encoding(false));return exe;

        }

        internal NetworkForm CreateNetworkWindow() {return new NetworkForm(SelectedEntry,root,Launcher.DeveloperTools);}

        static bool Ready(int pid) {

            IntPtr handle=OpenProcess(0x410,false,pid);if(handle==IntPtr.Zero)return false;

            try {var bytes=new byte[3];UIntPtr n;

                return ReadProcessMemory(handle,new IntPtr(0x45af20),bytes,new UIntPtr(3),out n) && n.ToUInt32()==3 &&

                       bytes[0]==0x55 && bytes[1]==0x8b && bytes[2]==0xec;

            } finally {CloseHandle(handle);}

        }

        bool IsIsolated(int pid) {

            IntPtr handle=OpenProcess(0x410,false,pid);if(handle==IntPtr.Zero)return false;

            try {var bytes=new byte[4096];UIntPtr count;

                return ReadProcessMemory(handle,new IntPtr(0x568c61),bytes,new UIntPtr(4096),out count) &&

                       count.ToUInt32()==4096 && CoopData.MatchesNativePath(bytes,DataRoot);

            } finally {CloseHandle(handle);}

        }

        Process FindGame() {

            foreach(var process in Process.GetProcessesByName("th18")) {

                try {if(!process.HasExited && String.Equals(process.MainModule.FileName,Path.GetFullPath(Path.Combine(GameDirectory,"th18.exe")),StringComparison.OrdinalIgnoreCase) && Ready(process.Id))return process;}

                catch { }

                process.Dispose();

            }

            return null;

        }

        async Task<string> Native(string arguments) {

            var info=new ProcessStartInfo(Path.Combine(root,"TH18LocalCoop.Console.exe"),arguments) {

                UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=root};

            using(var process=Process.Start(info)) {

                var output=process.StandardOutput.ReadToEndAsync();var error=process.StandardError.ReadToEndAsync();

                while(!process.HasExited)await Task.Delay(50);

                string text=(await output)+(await error);

                if(process.ExitCode!=0)throw new InvalidOperationException(text.Trim());return text.Trim();

            }

        }

        internal string StatusText {get {return status.Text;}}

        internal async Task Run() {

            start.Enabled=browse.Enabled=network.Enabled=replays.Enabled=entryKind.Enabled=path.Enabled=false;

            NetworkLog launchLog=null;
            try {

                string exe=ValidateSelection();launchLog=new NetworkLog(GameDirectory,"local-launch");launchLog.Write("ENTRY="+exe+" profile="+CoopData.ProfileDirectory(DataRoot));

                Process game=FindGame();

                if(game!=null && !IsIsolated(game.Id)) {game.Dispose();throw new InvalidOperationException("请先退出当前普通游戏，再由本启动器启动联机，确保从启动起就隔离存档。");}

                if(game==null) {

                    CoopData.SyncLocalConfig(Environment.GetEnvironmentVariable("APPDATA"),DataRoot);

                    Directory.CreateDirectory(CoopData.ProfileDirectory(DataRoot));

                    Note("正在启动游戏…");

                    var info=CoopData.StartInfo(exe,DataRoot,root);info.EnvironmentVariables["TH18_COOP_REPLAY_IDENTITY"]=NetIdentity.Load(Path.Combine(GameDirectory,"th18.exe"),root).Encode();
                    launchLog.Write("START executable="+info.FileName+" args="+info.Arguments+" cwd="+info.WorkingDirectory+" child_APPDATA="+info.EnvironmentVariables["APPDATA"]);
                    using(var initial=Process.Start(info)) {launchLog.Write("ENTRY_PID="+initial.Id);}

                    for(int i=0;i<160;++i) {await Task.Delay(250);game=FindGame();if(game!=null && Ready(game.Id) && IsIsolated(game.Id))break;if(game!=null){game.Dispose();game=null;}}

                }

                if(game==null)throw new InvalidOperationException("未找到使用独立联机档的游戏；Steam 或外部补丁可能重新启动了普通游戏。详情见："+launchLog.FilePath);

                int pid;using(game)pid=game.Id;

                Note("正在加载联机…");

                await Native("--attach "+pid);

                bool activated=false;string last="";

                for(int i=0;i<40;++i) {await Task.Delay(100);try {await Native("--activate "+pid);activated=true;break;}catch(Exception e){last=e.Message;}}

                if(!activated)throw new InvalidOperationException("扩展未就绪，请查看 coop.log 后重启游戏。\r\n"+last);

                launchLog.Write("COOP_ACTIVATED pid="+pid);Note("已启动。");

            } catch(Exception ex) {if(launchLog!=null)launchLog.Write("FAIL "+ex);Note(ex.Message);}

            finally {if(launchLog!=null)launchLog.Dispose();start.Enabled=browse.Enabled=network.Enabled=replays.Enabled=entryKind.Enabled=path.Enabled=true;}

        }

    }

}

