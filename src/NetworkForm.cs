using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace TH18LocalCoop {
    internal sealed class NetworkForm : Form {
        readonly string engine,entry,root;
        readonly bool diagnostics;
        readonly TextBox address=new TextBox(),code=new TextBox(),log=new TextBox();
        readonly NumericUpDown port=new NumericUpDown(),delay=new NumericUpDown();
        readonly Button host=new Button(),join=new Button(),stop=new Button(),test=new Button(),inspect=new Button(),duoButton=new Button();
        readonly CheckBox sharedTestKeys=new CheckBox();
        readonly ComboBox partySize=new ComboBox();
        OnlineSession session;bool running,closing;Process probe;
        LocalDuo duo;
        NetworkLog savedLog;
        internal NetworkForm(string enginePath,string modRoot,bool developerTools=false) {
            entry=enginePath;root=modRoot;diagnostics=developerTools;
            string gameDirectory=String.IsNullOrEmpty(entry)?null:Path.GetDirectoryName(entry);
            engine=Path.Combine(String.IsNullOrEmpty(gameDirectory)?root:gameDirectory,"th18.exe");
            Text="在线联机";Font=new Font("Microsoft YaHei UI",10);
            ClientSize=new Size(640,332);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;
            StartPosition=FormStartPosition.CenterParent;BackColor=Color.FromArgb(244,246,249);
            var ipLabel=new Label {Text="主机地址",AutoSize=true,Location=new Point(20,16)};
            address.Text="";address.SetBounds(20,41,318,30);
            var portLabel=new Label {Text="端口",AutoSize=true,Location=new Point(354,16)};
            port.Minimum=1024;port.Maximum=65535;port.Value=28180;port.SetBounds(354,41,100,30);
            var codeLabel=new Label {Text="连接码",AutoSize=true,Location=new Point(470,16)};
            code.CharacterCasing=CharacterCasing.Upper;code.MaxLength=8;code.SetBounds(470,41,150,30);
            var partyLabel=new Label {Text="人数",AutoSize=true,Location=new Point(20,91)};
            partySize.DropDownStyle=ComboBoxStyle.DropDownList;partySize.Items.AddRange(new object[]{"2 人","3 人"});partySize.SelectedIndex=0;partySize.SetBounds(70,86,100,30);
            partySize.SelectedIndexChanged+=delegate {if(partySize.SelectedIndex==1)sharedTestKeys.Checked=false;sharedTestKeys.Enabled=!running && partySize.SelectedIndex==0;};
            var delayLabel=new Label{Text="缓冲（帧）",AutoSize=true,Location=new Point(195,91)};
            delay.Minimum=1;delay.Maximum=12;delay.Value=2;delay.SetBounds(282,86,73,30);
            var localIp=new Label{Text="本机 IP："+LocalAddresses(),AutoEllipsis=true,Location=new Point(382,91),Size=new Size(238,24)};
            host.Text="创建联机";host.SetBounds(20,134,190,38);
            join.Text="加入联机";join.SetBounds(224,134,190,38);
            stop.Text="断开";stop.SetBounds(428,134,192,38);stop.Enabled=false;
            log.SetBounds(20,188,600,100);log.Multiline=true;log.ReadOnly=true;log.ScrollBars=ScrollBars.Vertical;log.BackColor=Color.White;
            var keys=new Label{Text="方向键移动 · Z 射击 · X Bomb · Shift 低速 · C 主动卡 · D 换卡",Location=new Point(20,300),Size=new Size(600,22),Font=new Font(Font.FontFamily,9)};
            Controls.Add(keys);
            Controls.AddRange(new Control[]{ipLabel,address,portLabel,port,codeLabel,code,partyLabel,partySize,delayLabel,delay,localIp,host,join,stop,log});
            if(developerTools) {
                ClientSize=new Size(640,432);
                test.Text="后台自测";test.SetBounds(20,332,180,32);
                inspect.Text="输入记录";inspect.SetBounds(220,332,180,32);
                duoButton.Text="本机多端测试";duoButton.SetBounds(420,332,200,32);
                sharedTestKeys.Text="共享键盘（本机双人）";sharedTestKeys.SetBounds(20,380,600,28);
                Controls.AddRange(new Control[]{test,inspect,duoButton,sharedTestKeys});
            }
            host.Click+=async delegate {await Connect(true);};join.Click+=async delegate {await Connect(false);};
            test.Click+=async delegate {await SelfTest();};stop.Click+=delegate {
                if(session!=null){bool paused=session.PausedAfterFailure;session.Dispose();if(paused){session=null;Busy(false);Note("本局已结束，请等游戏关闭后重新创建或加入联机。");}}
                if(duo!=null)duo.Dispose();
            };
            duoButton.Click+=async delegate {await TestLocalDuo();};
            inspect.Click+=async delegate {
                using(var dialog=new OpenFileDialog {Title="检查多人原始输入记录",Filter="输入记录|*.th18input",InitialDirectory=Path.Combine(Path.GetDirectoryName(engine),"coop-data","logs")})
                    if(dialog.ShowDialog()==DialogResult.OK)await RunProbe("--inspect-input \""+dialog.FileName+"\"","正在检查输入记录的序号和完整性……");
            };
            Shown+=delegate {
                try {
                    savedLog=new NetworkLog(Path.GetDirectoryName(engine),"window");
                    savedLog.Write(log.Text.Trim());Note("日志已保存到："+savedLog.FilePath);
                } catch(Exception error) {Note("无法保存日志："+error.Message);}
            };
            FormClosing+=delegate {
                if(session!=null && diagnostics)Note("会话统计：同步步数="+session.Completed);
                Note("关闭联机窗口。");closing=true;
                if(session!=null)session.Dispose();if(probe!=null)try {if(!probe.HasExited)probe.Kill();}catch { }
                if(duo!=null)duo.Dispose();
                if(savedLog!=null){savedLog.Dispose();savedLog=null;}
            };
        }
        static string LocalAddresses() {
            try {return String.Join("、",Dns.GetHostAddresses(Dns.GetHostName()).Where(ip=>ip.AddressFamily==AddressFamily.InterNetwork && !IPAddress.IsLoopback(ip)).Select(ip=>ip.ToString()));}
            catch {return "—";}
        }
        void Note(string text) {
            if(closing || IsDisposed)return;
            if(InvokeRequired) {try {BeginInvoke((Action)(()=>Note(text)));}catch(InvalidOperationException) { }return;}
            if(savedLog!=null)try {savedLog.Write(text);}catch(Exception error) {
                try {savedLog.Dispose();}catch { }savedLog=null;
                log.AppendText("日志写入失败："+error.Message+Environment.NewLine);
            }
            if(!diagnostics) {
                if(text.StartsWith("校验指纹：") || text.Contains("在线同步步数 ") || text.Contains("本次会话目录：") ||
                   text.StartsWith("日志已保存到：") || text.StartsWith("本次会话文件保留在："))return;
                if(text.Contains("正在启动本次会话游戏")) {
                    int seat=text.IndexOf("你是 P",StringComparison.Ordinal);text="正在启动游戏"+(seat>=0?" · "+text.Substring(seat+3,2):"")+"…";
                } else if(text.Contains("人延迟同步已连接"))text="已连接。";
                else if(text.StartsWith("本次主机进度已保存。"))text="进度已保存。";
            }
            if(log.TextLength>14000)log.Clear();
            log.AppendText(DateTime.Now.ToString("HH:mm:ss")+"  "+text+Environment.NewLine);
        }
        void Busy(bool value) {
            stop.Text="断开";
            running=value;host.Enabled=join.Enabled=test.Enabled=inspect.Enabled=duoButton.Enabled=sharedTestKeys.Enabled=!value;stop.Enabled=value;
            address.Enabled=port.Enabled=delay.Enabled=!value;code.ReadOnly=value;
            partySize.Enabled=!value;sharedTestKeys.Enabled=!value && partySize.SelectedIndex==0;
        }
        async Task Connect(bool asHost) {
            if(running)return;
            OnlineSession current=null;
            try {
                var identity=NetIdentity.Load(engine,root);
                Note("校验指纹：core="+identity.Core+"; mod="+identity.Mod+"; rules="+identity.Rules);
                if(asHost)code.Text=NetSession.NewCode();
                string token=code.Text.Trim(),ip=address.Text.Trim();int number=(int)port.Value;
                if(!NetSession.ValidCode(token))throw new InvalidOperationException("请填写主机提供的 8 位连接码。");
                int players=partySize.SelectedIndex+2;
                current=new OnlineSession(entry,root,players:players,delayFrames:(int)delay.Value);session=current;Busy(true);
                if(asHost) {
                    current.Listen(IPAddress.Any,number);
                    Note("等待玩家 · 1/"+players);
                    await Task.Run(()=>current.Host(token,Note));
                } else {
                    Note("正在连接 "+ip+":"+number+"……");
                    await Task.Run(()=>current.Join(ip,number,token,Note));
                }
            } catch(Exception error) {if(!closing)Note("连接结束："+NetSession.Explain(error));}
            finally {FinishConnection(current);}
        }
        void FinishConnection(OnlineSession current) {
                bool paused=current!=null && current.PausedAfterFailure && !closing;
                if(!paused){if(current!=null)current.Dispose();session=null;}
                if(!closing){
                    if(current!=null && diagnostics)Note("会话统计：同步步数="+current.Completed);
                    if(paused){Busy(true);stop.Text="结束本局";Note("游戏已暂停。"+(String.IsNullOrEmpty(current.FailureReportPath)?"本次会话已保留。":"诊断文件："+current.FailureReportPath));}
                    else{Busy(false);Note("已断开。");}
                }
        }
        async Task SelfTest() {
            await RunProbe("--online-self-test \""+Path.Combine(root,"TH18Coop.NativeTest.exe")+"\" \""+Path.GetDirectoryName(engine)+"\"","正在用两个网络进程和两个原生模拟进程检查在线开局链路，不打开游戏……");
        }
        async Task TestLocalDuo() {
            if(running)return;Busy(true);
            LocalDuo current=null;
            try {
                current=new LocalDuo(entry,root,sharedKeyboard:sharedTestKeys.Checked,players:partySize.SelectedIndex+2);duo=current;
                await current.Run(Note);
            } catch(Exception error){if(!closing)Note("本机双端测试结束："+NetSession.Explain(error));}
            finally {if(current!=null)current.Dispose();duo=null;if(!closing)Busy(false);}
        }
        async Task RunProbe(string arguments,string message) {
            if(running)return;Busy(true);stop.Enabled=false;
            try {
                // Validate actual installed files; no game process is created.
                NetIdentity.Load(engine,root);
                var info=new ProcessStartInfo(Path.Combine(root,"TH18Coop.NetTest.exe"),arguments) {
                    UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,
                    StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8,WorkingDirectory=root};
                Note(message);
                using(var child=Process.Start(info)) {
                    probe=child;var output=child.StandardOutput.ReadToEndAsync();var errors=child.StandardError.ReadToEndAsync();
                    while(!child.HasExited)await Task.Delay(50);
                    string report=await output;string failure=await errors;
                    if(!closing)Note(report.Trim()+(failure.Length>0?Environment.NewLine+failure.Trim():""));
                    if(child.ExitCode!=0 && !closing)Note("单机自测未通过，以上为检查结果。");
                }
            } catch(Exception error) {if(!closing)Note("自测未完成："+NetSession.Explain(error));}
            finally {probe=null;if(!closing)Busy(false);}
        }
    }
}
