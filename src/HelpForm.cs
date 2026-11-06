using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace TH18LocalCoop {
    internal sealed class HelpForm : Form {
        internal HelpForm(string gameDirectory) {
            Text="联机玩法说明";Font=new Font("Microsoft YaHei UI",10);
            ClientSize=new Size(700,530);MinimumSize=new Size(600,420);
            StartPosition=FormStartPosition.CenterParent;MinimizeBox=false;
            var tabs=new TabControl {Dock=DockStyle.Fill};Controls.Add(tabs);
            Page(tabs,"操作与选人",
                "本地双人\r\n默认键位（可在启动器“键位”修改）\r\nP1：方向键移动，Z 射击，X Bomb，Shift 低速，C 使用主动卡，D 换卡。\r\nP2：IJKL 移动，U 射击，P Bomb，O 低速，H 使用主动卡，N 换卡。\r\n\r\n在线两至三人\r\n每台电脑使用自己设置的在线键位控制自己的角色，未修改时沿用原版配置。P1 操作共用主菜单和推进剧情；机体、初始卡牌和关底购买按 P1→P2→P3 轮流选择。\r\n\r\n每人可以选择不同机体与初始卡牌。初始卡槽和已解锁卡牌以本局使用的联机存档为准，不临时全解锁。角色、残机、Bomb、灵力、金钱、卡牌和主动卡冷却分别结算。在线左下显示观看席位自己的主动卡。");
            Page(tabs,"死亡与赠送",
                "耗尽残机\r\n零备用残机仍能继续使用当前机体。最后一命耗尽后成为半透明、自动漂浮的幽灵，不能自行移动、射击或拾取。\r\n\r\n分命 / 救援\r\n靠近队友约20个游戏像素，停止射击，快速按低速键5次；每次间隔不超过24个逻辑帧（60帧时约0.4秒）。长按不累计，开火、离开、超时或更换目标会重置。赠送者至少要有1个备用残机，每次消耗1命。优先救近身幽灵，再选备用残机更少者；同值按席位顺序。给活人则生成仅归该队友的追踪道具。\r\n\r\n分火力\r\n靠近未满火力的队友，连续点射5次，每次间隔不超过24个逻辑帧。每次转移1.00火力，赠送者必须持有足够火力；优先给近身火力最低者。只扣实际成功生成的赠送份额，接收道具不向全队共享。\r\n\r\n复活\r\n救援复活恢复半满火力、自己的初始Bomb，并有240帧保护（约4秒）。过关或进入关底商店时，幽灵免费复活。共享残机补给让幽灵重新获得生命时，也会复活。\r\n\r\n全员成为幽灵约3秒后进入结束界面。联机不允许续关，可回标题重新开始。");
            Page(tabs,"战斗与掉落",
                "Boss伤害缩放\r\n仍可参战者两人时，总伤害×3/4；三人时×2/3；只剩一人时不缩放。普通掉命等待重生仍计入，成为幽灵后才退出计数。只影响原版登记Boss，不改变杂鱼耐久。\r\n\r\n决死与Bomb\r\n各机体原版决死窗口翻倍。每局的基础初始Bomb比原版少1，最低保留1；卡牌额外加成独立计算。每次Bomb给其他存活队友60帧保护（约1秒），不缩短已有更长无敌，也不持续刷新。EX的原版Bomb免疫判定计入所有玩家的有效Bomb。\r\n\r\n瞄准与透明\r\n自机狙按攻击来源选择最近的存活机体，排除死亡等待重生与幽灵；同距离按席位顺序。自己的机体正常显示，队友、其射击和部分装备按距离淡化。剧情立绘不参与队友透明。\r\n\r\n全队共享的掉落\r\n残机、Bomb、两类碎片、完整灵力补给，以及消耗型补给卡，任意玩家拾取后由各席位按自身上限与碎片阈值结算。拾取者已满，不会阻止未满队友获得。\r\n\r\n仍各自结算的掉落\r\n普通小P、大P与常规金钱不共享。普通P掉落数按人数增加，优先分配给未满火力的可操作玩家；全部满时沿用原版处理。赠送道具只归指定队友。初始选卡、商店购买与常驻能力卡各自独立。");
            string data=String.IsNullOrWhiteSpace(gameDirectory)?"所选游戏目录\\coop-data":Path.Combine(gameDirectory,"coop-data");
            Page(tabs,"联机与数据",
                "连接\r\n局域网和内网穿透均使用TCP。隧道映射到主机启动器设置的本地端口，默认28180。所有人需要相同联机版本、游戏核心和玩法配置。不能中途加入已开始的对局。\r\n\r\n输入缓冲\r\n主机设置1～12帧，默认2帧；客机自动采用主机值。网络抖动超过缓冲时会等待，不使用预测回滚。\r\n\r\n数据根目录\r\n"+data+"\r\n\r\n长期联机档：ShanghaiAlice\\th18\r\n在线会话：sessions\\会话编号\\host、guest、guest-p3\\ShanghaiAlice\\th18\r\n日志与旧输入诊断：logs\r\n\r\n在线使用主机联机档快照。只有正常结束的主机提交长期进度，并保留提交前备份；客机不覆盖自己的长期档。异常会话数据保留。原版单人进度不导入；custom.exe配置仅复制到联机配置，原件不写。\r\n\r\n启动器记住的游戏路径保存在启动器旁game-path.txt。移动游戏目录后重新选择即可。\r\n本机键位保存在启动器旁keybindings.ini，可分别设置本地P1、本地P2和在线本人。修改后下次启动生效，不随网络同步。");
            Page(tabs,"多人录像",
                "从启动器的“录像”进入原版录像菜单。观看使用独立临时数据目录，只能选择和观看录像，不能开始普通游戏，也不保存解锁、成绩或游戏进度。\r\n\r\n主机结束本局后会自动收藏录像；也可使用原版保存录像界面。多人数据随原版.rpy录像保存，包含额外玩家输入、机体、资源和卡牌状态；请通过对应版本联机启动器的“录像”入口观看；可把.rpy复制到朋友游戏目录的coop-data\\replay后观看。过去只有单人输入、没有多人扩展数据的旧录像不能补回队友数据。\r\n\r\n录像收藏保存在游戏目录的coop-data\\replay。临时观看数据在coop-data\\replay-sessions，正常退出后自动清理；异常时保留。\r\n\r\n原版菜单负责选录像、选关与播放控制。退出录像菜单即结束临时观看实例。");
        }
        static void Page(TabControl tabs,string title,string content) {
            var page=new TabPage(title);var text=new TextBox {Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Vertical,
                Dock=DockStyle.Fill,BorderStyle=BorderStyle.None,BackColor=SystemColors.Window,Text=content,WordWrap=true};
            page.Padding=new Padding(14);page.Controls.Add(text);tabs.TabPages.Add(page);
        }
    }
}
