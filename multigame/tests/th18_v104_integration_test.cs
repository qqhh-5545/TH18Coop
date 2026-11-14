using System;using System.IO;using System.Reflection;using System.Windows.Forms;
class V104IntegrationTest {
 static void Check(bool value,string message){if(!value)throw new Exception(message);}
 static object Field(object target,string name){return target.GetType().GetField(name,BindingFlags.Instance|BindingFlags.NonPublic).GetValue(target);}
 [STAThread]static int Main(string[] args){Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  string settings=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"selected-game.txt");byte[] previous=File.Exists(settings)?File.ReadAllBytes(settings):null;
  try{var assembly=Assembly.LoadFrom(args[0]);Check((string)assembly.GetType("TH18LocalCoop.V104.NetIdentity").GetField("Version",BindingFlags.NonPublic|BindingFlags.Static).GetRawConstantValue()=="1.0.4","Wrong TH18 identity");
   Check(assembly.GetType("TH18LocalCoop.V104.SessionReconnect")!=null,"Reconnect support absent");
   var catalog=assembly.GetType("TH18LocalCoop.GameCatalog");var all=(Array)catalog.GetField("All",BindingFlags.Static|BindingFlags.NonPublic).GetValue(null);
   string previousIcon=null;
   foreach(var definition in all)using(var icon=(System.Drawing.Icon)definition.GetType().GetMethod("LoadIcon",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(definition,null)){
    Check(icon!=null && icon.Width>0 && icon.Height>0,"Game icon unavailable without a game directory");
    using(var bitmap=icon.ToBitmap())using(var bytes=new MemoryStream()){bitmap.Save(bytes,System.Drawing.Imaging.ImageFormat.Png);string value=Convert.ToBase64String(bytes.ToArray());Check(previousIcon!=value,"Different games received identical icons");previousIcon=value;}
   }
   using(var form=(Form)Activator.CreateInstance(assembly.GetType("TH18LocalCoop.MainForm"),true)){
    var games=(ComboBox)Field(form,"games");games.SelectedIndex=0;var published=(Form)Field(form,"publishedView");Check(published!=null && !published.TopLevel && published.Parent==form,"TH18 view not embedded");Check(published.Bounds.Bottom<=form.ClientSize.Height,"Embedded view out of bounds");
    bool patch=false;foreach(Control c in published.Controls)if(c.Text=="thcrap")patch=true;Check(patch,"Official thcrap controls absent");
    var create=form.GetType().GetMethod("CreateNetworkWindow",BindingFlags.Instance|BindingFlags.NonPublic);
    using(var network=(Form)create.Invoke(form,null))Check(network.GetType().Namespace=="TH18LocalCoop.V104","TH18 used the legacy network stack");
    games.SelectedIndex=1;using(var network=(Form)create.Invoke(form,null))Check(network.GetType().Namespace=="TH18LocalCoop","TH14 network changed");
    Check(((Button)Field(form,"patchSettings")).Text=="thcrap","TH14 thcrap settings absent");Check(!((Button)Field(form,"replays")).Enabled,"TH14 Replay unexpectedly enabled");games.SelectedIndex=0;Check(ReferenceEquals(published,Field(form,"publishedView")),"Switching recreated TH18 session controls");
   }Console.WriteLine("PASS directory-independent distinct game icons, embedded TH18 1.0.4, reconnect/thcrap, 18/14 routing, bounds and repeated switching; no games launched");return 0;
  }catch(Exception e){Console.Error.WriteLine(e);return 1;}finally{if(previous==null){if(File.Exists(settings))File.Delete(settings);}else File.WriteAllBytes(settings,previous);}
 }
}
