# 1.0.4 — Recovery test release

- During active gameplay, retry the original TCP peer for up to30 seconds. Journal unacknowledged packets and suppress duplicates; authenticate the original session/seat.
- A state mismatch or failed recovery now pauses the game, saves session-error.txt and exposes an explicit End session action. No automatic native exit on these faults.
- Keep strict state validation. The original intermittent online desync is unresolved; game-state/checkpoint restoration is not implemented.
- Native test windows now use the verified640x480 display encoding; preserve permanent settings and progress isolation.
- Includes portable thcrap Auto/Off/Custom selection and current native Replay/scene-loading guards.
- Protocol5 requires all players to update the complete package. See RECOVERY_VALIDATION.json for measured coverage.

# 1.0.1

- 恢复受校验 Steam 1.00a 支持；移除启动器、原生加载器和安装器对 Steam 目录的排除。
- Steam / 独立版核心映射至相同联机与录像兼容标识，仍严格检查 EXE、mod 和规则。
- 保留进程专属 APPDATA、独立联机档、私有路径校验；本轮不启动或修改本机 Steam 游戏。

# 1.0.0

- 本地P2默认键位：IJKL移动、F低速、G射击、H Bomb、U主动卡、O换卡。
- 主界面移除键位提示，主要功能按钮改为Local Co-op、Online Co-op、Replay，其余界面保留中文。
- 增加英文README，发布1.0.0便携包。

# 0.20.0 — 键位设置

- 启动器新增本地P1、本地P2、在线本人三组键位设置，保存至本机keybindings.ini；默认键位不变，设置下次启动生效。
- 在线传输仍使用标准动作位，不传物理按键；各端可独立改键。原版键盘/手柄配置可继续沿用。
- 删除玩法说明中的其他项目介绍，保留依赖许可和必要署名。

# 0.19.0 experimental — 2026-11-06

- 原版 .rpy 录像附加多人扩展：各席位输入、机体、独立卡牌/资源、赠送/幽灵状态、原生卡牌序列化值及关卡随机数状态；保留原版录像列表、选关和播放控制。
- 主机正常结束本局时自动收藏录像，原版保存录像也写入多人数据；收藏在游戏目录 coop-data/replay。旧输入诊断不能转换为录像。
- 启动器新增“录像”，以独立临时 profile 进入原版 Replay 界面，禁用普通开局；正常退出不提交进度并清理临时数据。异常停止播放且保留原录像和诊断。
- 回放创建额外机体前恢复正确机体编号，避免外观正确却误用P1射击数据；额外卡牌使用原生导入/导出入口，不保存原生指针。
- 逐帧检查各席位状态，每60帧检查敌人/弹幕/掉落；不一致立即停止，避免继续展示错误录像。
- 主界面角落新增“?”，按操作、救援/赠送、战斗/掉落、联机/数据、录像分栏说明当前规则；主界面仍保留简洁功能按钮。

# 0.18.0 experimental — 2026-11-06

- 修复混合持卡时按分类索引导致的错图，主动卡切换保留原版动画。
- 常规自机狙/改向弹、激光及 ECL 自机坐标改选最近存活玩家，平局按席位；不改变碰撞归属。
- 原版 EX Bomb 免疫和 Bomb 伤害修正检查所有席位，最后一个有效 Bomb 结束才解除。
- 死亡释放自己遗留的低速判定点；幽灵绘制不显示低速状态。
- 剧情立绘树不参与队友透明；右侧原生立绘按当前原生动画边界向左避让侧栏，绘制后恢复。
- 每次 Bomb 给存活队友60逻辑帧保护，不持续刷新，不缩短已有无敌。
- 输入缓冲默认2帧，仍可选1～12帧；所有人均需更新0.18.0。

