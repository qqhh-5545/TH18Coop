using System;using System.Drawing;using System.IO;using System.Windows.Forms;
namespace TH18LocalCoop.V104 {
 internal sealed class ThcrapForm:Form {
  internal ThcrapForm(string root) {
   Text="thcrap 设置";Font=new Font("Microsoft YaHei UI",10);ClientSize=new Size(580,262);
   FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterParent;
   var mode=new ComboBox {DropDownStyle=ComboBoxStyle.DropDownList,Location=new Point(16,16),Size=new Size(548,30)};
   mode.Items.AddRange(new object[]{"自动使用 Steam 启动选项","不使用 thcrap","手动指定 thcrap"});
   var loader=new TextBox {Location=new Point(16,78),Size=new Size(448,28)};
   var config=new TextBox {Location=new Point(16,140),Size=new Size(448,28)};
   var browseLoader=new Button {Text="浏览…",Location=new Point(476,76),Size=new Size(88,30)};
   var browseConfig=new Button {Text="浏览…",Location=new Point(476,138),Size=new Size(88,30)};
   var settings=ThcrapSettings.Load(root);mode.SelectedIndex=settings.Mode=="Off"?1:settings.Mode=="Custom"?2:0;loader.Text=settings.Loader;config.Text=settings.Config;
   Action enabled=()=>{loader.Enabled=config.Enabled=browseLoader.Enabled=browseConfig.Enabled=mode.SelectedIndex==2;};mode.SelectedIndexChanged+=delegate {enabled();};enabled();
   browseLoader.Click+=delegate {using(var dialog=new OpenFileDialog {Filter="thcrap loader|thcrap_loader.exe",CheckFileExists=true})if(dialog.ShowDialog(this)==DialogResult.OK)loader.Text=dialog.FileName;};
   browseConfig.Click+=delegate {using(var dialog=new OpenFileDialog {Filter="thcrap 配置|*.js;*.json|所有文件|*.*",CheckFileExists=true})if(dialog.ShowDialog(this)==DialogResult.OK)config.Text=dialog.FileName;};
   var save=new Button {Text="保存",Location=new Point(368,218),Size=new Size(92,30)};
   save.Click+=delegate {try {
    settings.Mode=mode.SelectedIndex==1?"Off":mode.SelectedIndex==2?"Custom":"Auto";settings.Loader=loader.Text.Trim();settings.Config=config.Text.Trim();
    if(settings.Mode=="Custom")CoopData.ThcrapInfo(Path.Combine(root,"th18.exe"),Path.Combine(root,"coop-data"),CoopData.QuoteArg(settings.Loader)+" "+CoopData.QuoteArg(settings.Config)+" %command%");
    settings.Save(root);DialogResult=DialogResult.OK;Close();
   }catch(Exception e){MessageBox.Show(this,e.Message,"无法保存 thcrap 设置");}};
   var cancel=new Button {Text="取消",DialogResult=DialogResult.Cancel,Location=new Point(472,218),Size=new Size(92,30)};
   Controls.AddRange(new Control[]{mode,loader,config,browseLoader,browseConfig,save,cancel,
    new Label {Text="thcrap_loader.exe",AutoSize=true,Location=new Point(16,54)},new Label {Text="汉化配置（例如 config\\zh-hans.js）",AutoSize=true,Location=new Point(16,116)},
    new Label {Text="仅保存到本机。下次启动游戏生效；不修改 Steam 启动选项。",Location=new Point(16,180),Size=new Size(548,28)}});
  }
 }
}
