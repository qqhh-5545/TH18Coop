using System;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace TH18LocalCoop.V104 {
    internal sealed class KeyBindingsForm : Form {
        static readonly string[] Sections={"LocalP1","LocalP2","Online"};
        static readonly string[] Names={"Up","Down","Left","Right","Shoot","Focus","Bomb","Card","Cycle"};
        static readonly string[] Labels={"上","下","左","右","射击 / 确认","低速","Bomb / 返回","使用主动卡","切换主动卡"};
        static readonly int[][] Defaults={new[]{38,40,37,39,90,16,88,67,68},new[]{73,75,74,76,71,70,72,85,79},new[]{38,40,37,39,90,16,88,67,68}};
        readonly int[][] values=new int[3][];
        readonly TextBox[,] boxes=new TextBox[3,9];
        readonly CheckBox[] native=new CheckBox[3];
        readonly string file;
        internal KeyBindingsForm(string root) {
            file=Path.Combine(root,"keybindings.ini");Text="键位设置";Font=new Font("Microsoft YaHei UI",10);ClientSize=new Size(480,518);
            FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterParent;
            for(int p=0;p<3;++p)values[p]=(int[])Defaults[p].Clone();
            bool[] useNative={true,false,true};
            if(File.Exists(file)){int section=-1;foreach(string raw in File.ReadAllLines(file)) {string line=raw.Trim();if(line.StartsWith("[")){section=Array.IndexOf(Sections,line.Trim('[',']'));continue;}int split=line.IndexOf('=');if(section<0 || split<0)continue;string name=line.Substring(0,split).Trim();int v;if(!Int32.TryParse(line.Substring(split+1).Trim(),out v))continue;if(name=="UseNative")useNative[section]=v!=0;int index=Array.IndexOf(Names,name);if(index>=0 && v>=8 && v<=254 && v!=13 && v!=27)values[section][index]=v;}}
            var tabs=new TabControl();tabs.SetBounds(12,12,456,422);Controls.Add(tabs);
            string[] titles={"本地 P1","本地 P2","在线本人"};
            for(int p=0;p<3;++p){int profile=p;var page=new TabPage(titles[p]);tabs.TabPages.Add(page);
                native[p]=new CheckBox {Text=p==1?"本地 P2 使用独立键位":"沿用原版键盘 / 手柄配置",Checked=p==1 || useNative[p],Enabled=p!=1,AutoSize=true,Location=new Point(16,14)};page.Controls.Add(native[p]);
                for(int i=0;i<9;++i){int action=i;page.Controls.Add(new Label {Text=Labels[i],Location=new Point(20,53+i*32),Size=new Size(174,26)});
                    var box=new TextBox {ReadOnly=true,ShortcutsEnabled=false,Location=new Point(204,49+i*32),Size=new Size(194,27),Text=KeyName(values[p][i])};boxes[p,i]=box;page.Controls.Add(box);
                    box.PreviewKeyDown+=delegate(object sender,PreviewKeyDownEventArgs e){e.IsInputKey=true;};
                    box.KeyDown+=delegate(object sender,KeyEventArgs e){e.SuppressKeyPress=true;int code=(int)e.KeyCode;if(code<8 || code>254 || code==13 || code==27)return;values[profile][action]=code;box.Text=KeyName(code);if(profile!=1)native[profile].Checked=false;};
                }
                var reset=new Button {Text="恢复默认",Location=new Point(290,347),Size=new Size(108,30)};reset.Click+=delegate {values[profile]=(int[])Defaults[profile].Clone();for(int i=0;i<9;++i)boxes[profile,i].Text=KeyName(values[profile][i]);native[profile].Checked=true;};page.Controls.Add(reset);
            }
            Controls.Add(new Label {Text="点击键位框后按一个键。Esc、Enter 保留用于菜单。\r\n保存后下次启动游戏生效；在线各端可使用不同键位。",Location=new Point(16,440),Size=new Size(450,40),Font=new Font(Font.FontFamily,9)});
            var save=new Button {Text="保存",Location=new Point(268,484),Size=new Size(96,28)};save.Click+=delegate {Save();};
            var cancel=new Button {Text="取消",DialogResult=DialogResult.Cancel,Location=new Point(372,484),Size=new Size(96,28)};Controls.AddRange(new Control[]{save,cancel});
        }
        static string KeyName(int code){return ((Keys)code).ToString();}
        void Save() {
            for(int p=0;p<3;++p)for(int i=0;i<9;++i)for(int j=0;j<i;++j)if(values[p][i]==values[p][j]){MessageBox.Show(this,"同一组内的两个动作不能使用同一个键。","键位冲突");return;}
            for(int i=0;i<9;++i)for(int j=0;j<9;++j)if(values[0][i]==values[1][j]){MessageBox.Show(this,"本地 P1 与 P2 的键位不能重叠。","键位冲突");return;}
            var text=new StringBuilder();for(int p=0;p<3;++p){text.AppendLine("["+Sections[p]+"]");text.AppendLine("UseNative="+(p==1?0:native[p].Checked?1:0));for(int i=0;i<9;++i)text.AppendLine(Names[i]+"="+values[p][i]);text.AppendLine();}
            try {string pending=file+".tmp";File.WriteAllText(pending,text.ToString(),new UTF8Encoding(false));if(File.Exists(file))File.Replace(pending,file,null);else File.Move(pending,file);DialogResult=DialogResult.OK;Close();}
            catch(Exception error){MessageBox.Show(this,error.Message,"无法保存键位");}
        }
    }
}
