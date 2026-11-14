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

[assembly: System.Reflection.AssemblyVersion("1.0.5.0")]
[assembly: System.Reflection.AssemblyFileVersion("1.0.5.0")]

namespace TH18LocalCoop {

    internal static class Launcher {

        internal static bool DeveloperTools;

        [STAThread] static void Main(string[] args) {

            DeveloperTools=args.Contains("--developer-tools");

            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);

            var form=new MainForm();
            if(args.Contains("--th14"))form.SelectTh14();

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

                if(control is ComboBox) using(var graphics=Graphics.FromImage(bitmap)) {
                    var combo=(ComboBox)control;var definition=combo.SelectedItem as GameDefinition;
                    if(definition!=null){graphics.FillRectangle(SystemBrushes.Window,position.X+2,position.Y+2,control.Width-22,control.Height-4);MainForm.DrawGameChoice(graphics,new Rectangle(position,control.Size),definition,control.Font,control.ForeColor);}
                    else graphics.DrawString(control.Text,control.Font,Brushes.Black,new PointF(position.X+5,position.Y+5));
                }

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

            bool chinese=entryKind.SelectedIndex==1 || (entryKind.SelectedIndex==0 && File.Exists(Path.Combine(directory,SelectedGame.ChineseEntry)));

            return Path.Combine(directory,chinese?SelectedGame.ChineseEntry:SelectedGame.Engine);

        }}

        string DataRoot {get{return Path.Combine(GameDirectory,"coop-data");}}

        readonly string root=AppDomain.CurrentDomain.BaseDirectory;

        readonly TextBox path=new TextBox();

        readonly Label status=new Label();

        readonly ToolTip tips=new ToolTip();

        readonly ComboBox entryKind=new ComboBox();
        readonly ComboBox games=new ComboBox();
        V104.MainForm publishedView;
        GameDefinition SelectedGame {get{return games.SelectedItem as GameDefinition??GameCatalog.TH18;}}
        string GamePathFile {get{return Path.Combine(root,SelectedGame.PathFile);}}

        readonly Button start=new Button(),browse=new Button(),network=new Button(),replays=new Button(),help=new Button(),patchSettings=new Button();

        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,int pid);

        [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr size,out UIntPtr count);

        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);

        public MainForm() {

            Text="Touhou Co-op 开发版";

            Font=new Font("Microsoft YaHei UI",10);ClientSize=new Size(640,274);

            FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;

            BackColor=Color.FromArgb(244,246,249);

            var fileLabel=new Label {Text="游戏目录",AutoSize=true,Location=new Point(20,59)};

            path.Name="GameDirectory";path.SetBounds(20,85,508,30);browse.SetBounds(538,84,82,32);browse.Text="浏览…";

            var entryLabel=new Label {Text="版本",AutoSize=true,Location=new Point(20,135)};

            entryKind.DropDownStyle=ComboBoxStyle.DropDownList;entryKind.Items.AddRange(new object[]{"自动","汉化版","日文版"});entryKind.SelectedIndex=0;entryKind.SetBounds(73,130,131,30);

            browse.Click+=delegate {using(var dialog=new FolderBrowserDialog {Description="选择游戏目录",ShowNewFolderButton=false}) {

                if(Directory.Exists(GameDirectory))dialog.SelectedPath=GameDirectory;

                if(dialog.ShowDialog()==DialogResult.OK){path.Text=dialog.SelectedPath;entryKind.SelectedIndex=0;}}};

            start.Text="Local Co-op";start.SetBounds(20,176,192,40);

            start.Click+=async delegate {await Run();};

            network.Text="Online Co-op";network.SetBounds(224,176,192,40);

            network.Click+=delegate {try{ValidateSelection();using(var window=CreateNetworkWindow())window.ShowDialog(this);}catch(Exception error){Note(error.Message);}};
            replays.Text="Replay";replays.SetBounds(428,176,192,40);
            replays.Click+=async delegate {
                try {
                    string exe=ValidateSelection();games.Enabled=start.Enabled=network.Enabled=replays.Enabled=browse.Enabled=entryKind.Enabled=path.Enabled=false;
                    await Task.Run(()=>ReplayViewer.Run(exe,root,text=>{if(!IsDisposed)BeginInvoke((Action)(()=>Note(text)));}));
                } catch(Exception error){Note(error.Message);}
                finally {games.Enabled=start.Enabled=network.Enabled=browse.Enabled=entryKind.Enabled=path.Enabled=true;replays.Enabled=SelectedGame.Id=="th18";}
            };
            help.Text="?";help.SetBounds(588,130,32,30);tips.SetToolTip(help,"联机玩法与数据位置");
            help.Click+=delegate {using(var window=new HelpForm(GameDirectory,SelectedGame.Id))window.ShowDialog(this);};

            var keySettings=new Button {Text="键位",Location=new Point(498,130),Size=new Size(78,30)};keySettings.Click+=delegate {using(var window=new KeyBindingsForm(root))window.ShowDialog(this);};Controls.Add(keySettings);
            patchSettings.Text="thcrap";patchSettings.SetBounds(402,130,84,30);
            patchSettings.Click+=delegate {try {using(var window=new Th14ThcrapForm(root,SelectedEntry))window.ShowDialog(this);}catch(Exception error){Note(error.Message);}};Controls.Add(patchSettings);
            status.SetBounds(20,232,600,33);status.AutoEllipsis=true;
            Controls.AddRange(new Control[]{fileLabel,path,browse,entryLabel,entryKind,start,network,replays,help,status});

            games.DropDownStyle=ComboBoxStyle.DropDownList;games.DrawMode=DrawMode.OwnerDrawFixed;games.ItemHeight=26;games.SetBounds(20,12,270,34);
            games.Items.AddRange(GameCatalog.All);games.SelectedIndex=0;
            games.DrawItem+=delegate(object sender,DrawItemEventArgs e){if(e.Index<0)return;e.DrawBackground();DrawGameChoice(e.Graphics,e.Bounds,(GameDefinition)games.Items[e.Index],Font,e.ForeColor);e.DrawFocusRectangle();};
            games.SelectedIndexChanged+=delegate {path.Text="";entryKind.SelectedIndex=0;try{if(File.Exists(GamePathFile)){string saved=File.ReadAllText(GamePathFile).Trim();if(Directory.Exists(Path.GetDirectoryName(saved))){path.Text=Path.GetDirectoryName(saved);entryKind.SelectedIndex=Path.GetFileName(saved).EndsWith("c.exe",StringComparison.OrdinalIgnoreCase)?1:2;}}File.WriteAllText(Path.Combine(root,"selected-game.txt"),SelectedGame.Id);}catch{}replays.Enabled=SelectedGame.Id=="th18";Note("");ApplyGameView();};Controls.Add(games);
            try{if(File.Exists(Path.Combine(root,"selected-game.txt")) && File.ReadAllText(Path.Combine(root,"selected-game.txt")).Trim()=="th14")games.SelectedIndex=1;}catch{}
            var settings=GamePathFile;

            try {

                if(File.Exists(settings)) {

                    string saved=File.ReadAllText(settings).Trim();string directory=Path.GetDirectoryName(saved);

                    if(Directory.Exists(directory) && !GameSelection.IsSteam(directory)) {

                        path.Text=directory;

                        entryKind.SelectedIndex=String.Equals(Path.GetFileName(saved),SelectedGame.ChineseEntry,StringComparison.OrdinalIgnoreCase)?1:2;

                    }

                }

            } catch { }
            ApplyGameView();

        }

        void ApplyGameView() {
            if(SelectedGame.Id=="th18") {
                if(publishedView==null){V104.Launcher.DeveloperTools=Launcher.DeveloperTools;publishedView=new V104.MainForm {TopLevel=false,FormBorderStyle=FormBorderStyle.None,Location=new Point(0,44)};Controls.Add(publishedView);
                    foreach(Control c in publishedView.Controls)if(c is Button && c.Text=="Local Co-op")c.EnabledChanged+=delegate(object sender,EventArgs e){games.Enabled=((Control)sender).Enabled;};}
                publishedView.Show();publishedView.BringToFront();games.BringToFront();
            }else if(publishedView!=null)publishedView.Hide();
            Text=SelectedGame.Id=="th18"?"Touhou Co-op 1.0.5 · TH18":"Touhou Co-op 1.0.5 · TH14";
        }
        internal static void DrawGameChoice(Graphics graphics,Rectangle bounds,GameDefinition definition,Font font,Color color) {
            using(var icon=definition.LoadIcon())if(icon!=null)graphics.DrawIcon(icon,new Rectangle(bounds.X+4,bounds.Y+3,22,22));
            TextRenderer.DrawText(graphics,definition.Name,font,new Point(bounds.X+34,bounds.Y+4),color);
        }
        internal void SelectTh14(){games.SelectedItem=GameCatalog.TH14;}
        void Note(string text) {status.Text=text;tips.SetToolTip(status,text);}

        string ValidateSelection(){

            string exe=SelectedGame.Validate(SelectedEntry);

            foreach(string file in new[]{SelectedGame.Loader,SelectedGame.Module,"coop.ini"})

                if(!File.Exists(Path.Combine(root,file)))throw new IOException("联机文件不完整，请重新解压压缩包。");

            path.Text=Path.GetDirectoryName(exe);

            File.WriteAllText(GamePathFile,exe,new UTF8Encoding(false));games.Invalidate();return exe;

        }

        internal Form CreateNetworkWindow() {if(SelectedGame.Id=="th18"){ApplyGameView();return publishedView.CreateNetworkWindow();}return new NetworkForm(SelectedEntry,root,Launcher.DeveloperTools);}

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

        internal string StatusText {get {return SelectedGame.Id=="th18" && publishedView!=null?publishedView.StatusText:status.Text;}}

        internal async Task Run() {
            if(SelectedGame.Id=="th18"){ApplyGameView();await publishedView.Run();return;}

            games.Enabled=start.Enabled=browse.Enabled=network.Enabled=replays.Enabled=entryKind.Enabled=path.Enabled=patchSettings.Enabled=false;

            try {

                string exe=ValidateSelection();
                if(SelectedGame.Id=="th14"){await Th14Launcher.Run(exe,root,Note);return;}

                Process game=FindGame();

                if(game!=null && !IsIsolated(game.Id)) {game.Dispose();throw new InvalidOperationException("请先退出当前普通游戏，再由本启动器启动联机，确保从启动起就隔离存档。");}

                if(game==null) {

                    CoopData.SyncLocalConfig(Environment.GetEnvironmentVariable("APPDATA"),DataRoot);

                    Directory.CreateDirectory(CoopData.ProfileDirectory(DataRoot));

                    Note("正在启动游戏…");

                    var info=CoopData.StartInfo(exe,DataRoot);info.EnvironmentVariables["TH18_COOP_REPLAY_IDENTITY"]=NetIdentity.Load(Path.Combine(GameDirectory,"th18.exe"),root).Encode();
                    using(var initial=Process.Start(info)) { }

                    for(int i=0;i<160;++i) {await Task.Delay(250);game=FindGame();if(game!=null && Ready(game.Id) && IsIsolated(game.Id))break;if(game!=null){game.Dispose();game=null;}}

                }

                if(game==null)throw new InvalidOperationException("尚未找到使用独立数据目录的游戏，请使用本启动器启动联机。");

                int pid;using(game)pid=game.Id;

                Note("正在加载联机…");

                await Native("--attach "+pid);

                bool activated=false;string last="";

                for(int i=0;i<40;++i) {await Task.Delay(100);try {await Native("--activate "+pid);activated=true;break;}catch(Exception e){last=e.Message;}}

                if(!activated)throw new InvalidOperationException("扩展未就绪，请查看 coop.log 后重启游戏。\r\n"+last);

                Note("已启动。");

            } catch(Exception ex) {Note(ex.Message);}

            finally {games.Enabled=start.Enabled=browse.Enabled=network.Enabled=entryKind.Enabled=path.Enabled=patchSettings.Enabled=true;replays.Enabled=SelectedGame.Id=="th18";}

        }

    }

}

