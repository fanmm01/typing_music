# tymp → MusicXML 转换器实现方案(C 语言)

> 本文档给出将「newfileMaker 生成、严格依照 **rule_0_3(v0.3.1)** 规范书写」的 .tymp 乐谱文档转换为 MusicXML 4.0 (partwise) 的完整实现方案,精确到每个文件与每个函数,并附测试用例与预期输出。
>
> **修订记录**
>
> | 版本 | 依据 | 主要变化 |
> |---|---|---|
> | v0.2(初版) | rule_0_2.txt | 全文初稿 |
> | **v0.3(本次)** | rule_0_3.txt + part-IV.txt 实践 + 作者确认 | ①纳入 rule_0_3 的 12 条改动(含首版遗漏的「和声/主旋律行数不限」与 `*l""` 歌词分隔);②纳入 part-IV 实践中 21 项此前未总结的写法(见 §2.6);③**修正列/单位模型**(一列恒 = 16 分音符,推翻首版「固定 accu=4」假设,§1.3/§3.2);④机动行方向语义定案(`\|` 服务下方行、`\` 服务上方行,§2.5);⑤约束 C1/C2 改写,新增 direction / harmony / unpitched / work 输出(§1.2/§4);⑥新增 rule.md 作为唯一权威规则文本;⑦函数表全量更新(§6),新增行复制、参数化表记、管道实例、动态行分类等 |
| **v0.3.1(本次)** | rule_0_3.txt「v0.3.1 版本调整」11 条(§2.6.4) | ①新增 **.tymp 补全副本预处理阶段**(§7.0、`make_tymp_copy`);②`@lN` **不复制歌词**、新增 `@lN^M` 平行和声(§2.4/§7.9);③新增 ⑥ 类 `X&g"xxxxxx"` 吉他把位表注(§8、`parse_voicing_g`);④`H_6` 和弦替换、`_3/_5/_7` 原位标注、`maj`=增三和弦(§7.3);⑤歌词改**逐字句一一对应**(§7.2.4);⑥多作品 → 多个 .musicxml(§1.1/§7.0/main);⑦机动行可向上堆叠(§2.5) |
| **v0.3.1+1(本次)** | 作者指示 | ①**行角色判定不再通过表头**(仅节拍行例外,§7.2.2 重写、V12);②修改 `newfileMaker_full0_1.c`:伴奏行**每一行都写出行头**(首组完整形态 `(Accomponiment#N:ID<EN; …*4` ×3 行、续组短式 `(-ID-` ×3 行);新旧两种输出形态都须接受(§2.1) |
| **v0.3.1+5(本次)** | 作者指示 | ①**`%%` 组首时值边界修复**:组成员不得成为组首事件的下一事件边界,组首保持完整 8 单位时值;②修复 `@lN` 行复制重复应用(此前同一 `IT_COPY` 被调用多次,造成 Harmony 4 等复制声部时值叠加);③重建后如梦人生文件各声部/各小节时值全部守恒,第 16-17 小节及第 49-50、70 小节不再出现总时值错误 |
| **v0.3.1+6(本次)** | 全曲回归发现 | ①**多标记自动重复周期锚点**:每次 `emit_unit` 生成的音符必须独立归属其时间单元,空白单元只延长本定义调用内最近音;否则连续 `@lN^±M` 复制将把先前生成的音延长,在未出错小节重复起音导致总时值超过拍长;引入 `Voice.emit_floor`,限制 `expand_mark` 对 v->notes 的时值回写范围;②`rumei` 的 12 声部先前跨组累计,现稳定输出 1833 个 4/4 小节,逐小节 240 tick 均通过;说明多声部跨组累计是正确行为,此前按 1551 检查仅覆盖首组 |

| **v0.3.1+3(本次)** | 作者指示 | ①**定义内 `_` = 空格单元**:每个 `_` 本身占 1 列、使前一音时值延长一单位(`1_{35}_{35}_` = 6 单元:1,_,{35},_,{35},_,音符落在第 1/3/5 单元);仅**标记定义**适用,管道/括号内容的下划线仍为分隔符(§8);②**单单位标记一次性**(§8.2 落地):如 `R&r"{135}"` 不自动重复,该音持满调用跨度;③**无法用单一音值表达的时值用延音线拆分**(6/8 的 5 拍 = 半音+八分 等,§3.3/§4.5),整小节休止音型按小节列数;④**标记调用后的括号单音/`_n` 记号** = 同标记再次调用(`X(3) (5+) (7)` → `X(3) X(5+) X(7)`;`X _6 _46` → `X X_6 X_46`);⑤**连音路径全面修复**:`%%` 双百分号合并为单条目、`%` 组时值/余数分配、tm 约分(`3:2` 等)、音数超过声明宽度的长 `%` 跑句按普通音逐列铺开、连音组跨小节按组整体拆分、`\|__` 细分逐格取整并同列格共位;⑥**延音线拆分语义**:首片保留原 stop、末片保留原 start(拆分支单独起收);⑦修复 `@lN^±M` 平行和声的**死循环**(v->n 上界固化) |
| **v0.3.1+4(本次)** | 作者指示 | ①**`%%` 连音语义定案**:`%345` = 4 单位三连音(紧凑);`%%` **一律按间隔书写形式**——`%%3 4 5`(间隔)与 `%%345678`(紧凑)都在其后 8 列窗口内收集组成员(`%%3 4 5` = 8 单位三连音、`%%345678` = 8 单位六连音、`%%34567` = 8 单位五连音、`%%R R R` 标记调用亦然);组成员在 `%%` 处同列码放、tick = 120/n、tm 约分;②**`\|_` 半单位细分**:`\|_5671234567123456` 每列 2 格(每格 15/2^subdiv tick 取整差分布、同列格共位),16 格 = 8 列恰满;③**单次标记展开的地板修正**:一次性标记只改写自己推入的音,不再回写同列的前一标记之音;④**同列排序稳定化**:Note 增 seq 推入序,同列同型保持书写顺序(此前 qsort 不稳定导致 `%%` 组内音序错乱);⑤写谱层**连音组后续音判定收紧**(前音须同为连音),避免复制声部中重叠的带 tick 音误写
>
> **依据(文档域)**:
> - `rule.md` —— 曲谱书写规范(**唯一权威**,含 rule_0_2 + rule_0_3 + 实践确认条款)
> - `rule_0_2.txt` / `rule_0_3.txt` —— 规范原文(rule.md 的溯源对象)
> - `tymp.h` —— 转换器输出参数(本版**加法扩展**,见 §5.1)
> - `newfileMaker_full0_1.c` + `output/4`(旧格式权威模板)+ `part-IV.txt`(0.3 规范实践曲谱)—— .tymp 文件的实际结构
> - `instruments.h` —— 乐器原型表(part-list 来源)
> - `musicxmlEg.xml` —— 输出 MusicXML 的格式参照(direction 族模板的来源)
> - `tymp2musicXML.c`(现有骨架,需实现)+ `tympUtils.c`(空文件,待填充)
> - `output/漫游仙境圆舞曲.musicxml` 是 MuseScore 导出文件,**不是**本转换器的输出参照,不参与格式决策。

---

## 一、目标与范围

### 1.1 目标

读取一个 .tymp 文本乐谱,输出一个符合 `musicxmlEg.xml` 风格、可被 MuseScore 等软件打开的 MusicXML 4.0 partwise 文件:

1. 解析文件头(标题/作者/instruments 行)与全部「组」(`{`…`}` 块);
2. **动态分类组内各行的角色**(数字行 / 机动行上指 / 机动行下指 / 歌词行 / 和声行 / 伴奏行 / 和弦行 / 时间戳行),取代首版的固定 3 行布局(§7.2);
3. 解析组头/行头元数据(拍号、两种调号语法、速度、力度、乐器名与六种伴奏行头、初始音区 `>` / `*`、特殊表记定义、和弦行、吉他调弦表);
4. 解析数字行、机动行、歌词行,展开全部特殊表记(①~⑤ + 参数化 + 无引号形态 + 自动重复),生成逐小节、逐音符事件;
5. 完成简谱→音名的八度消解(含 `'`、`*`、`.`、`>` 前缀与固定变音增减)、变音计算、跨小节拆分与延音线生成;
6. 处理 `@lN` 行复制、`=` 替换、管道实例、`-` 显式结束记号;
7. **ID 独一化**、乐器定义简写补全、无行头乐器默认钢琴(rule_0_3 移交的职责);
8. 按 `musicxmlEg.xml` 的写法输出 MusicXML,并输出 rule_0_3 第 7 条要求的 **direction(力度/速度/变速)、harmony、unpitched、歌名作者**;
9. **补全副本预处理**(v0.3.1 #6):转换前先把特殊表记与 `@lN`/`@lN^M` 等简化写作补全为一个扩展名为 `.tymp` 的副本,再解析副本(§7.0);
10. **多作品**:同一文档中不同「标题 by 作者」的乐谱行,分别生成多个 .musicxml 文件(v0.3.1 #10)。

### 1.2 硬性约束(用户约定)

| # | 约束 | 对本方案的影响 |
|---|------|----------------|
| C1 | 输出参数以 **§5.1 加法扩展后的 `tymp.h`** 为唯一来源;扩展只增不改,不删除既有字段 | 首版"力度/速度/和弦行不输出"的限制被 rule_0_3 第 7 条解除(见 C2) |
| C2 | 除 rule_0_3 第 7 条明确要求的要素(力度 / 速度 / 变速 → `direction` 族、和弦行 → `harmony`、无音高打击 → `unpitched`、歌名作者 → `work`/`identification`)外,**不写 `musicxmlEg.xml` 中不存在的项目** | 仍不输出 `<beam>`、`<stem>`、`<backup>`、`<forward>`、`<transpose>`、`<print>`、`<slur>`、`<figure-bass>`、`<barline>` |
| C3 | 必须正确处理跨小节音符(延音线) | 跨小节长音符必须按小节线拆分,并以 `<tie>`/`<tied>` start/stop 对连接(§7.4) |
| C4 | 方案精确到每个文件、每个函数 | §6 为逐函数规格,给出签名、职责与关键算法要点 |

> **C1/C2 的边界说明**:rule_0_3 第 7 条要求"在数据结构中实装歌名作者,力度记号,速度记号,变速记号,和弦行,无音高打击乐器"——这是**新增的输出要求**,故 `musicxmlEg.xml` 中已有的 `<direction>` 族按其实例风格输出;`<harmony>`、`<unpitched>`、`<work>`/`<identification>` 虽未见于该示例,但属第 7 条明确要求,予以输出并在 §4 中给出精确格式。

### 1.3 输出要素边界(总览)

**会输出的 MusicXML 要素**(逐一详细规格见 §4):

- **结构**:`score-partwise`、`part-list`、`score-part`、`part-name`、`score-instrument`、`instrument-name`、`midi-instrument`、`midi-channel`、`midi-program`、`midi-unpitched`、`part`、`measure`、`attributes`、`divisions`、`key`、`fifths`、`mode`、`time`、`beats`、`beat-type`、`clef`、`sign`、`line`;
- **音符**:`note`、`chord`、`rest`(含 `measure="yes"`)、`unpitched`、`display-step`、`display-octave`、`pitch`、`step`、`alter`、`octave`、`duration`、`tie`、`instrument`、`voice`、`type`、`dot`、`accidental`、`time-modification`、`actual-notes`、`normal-notes`、`normal-type`、`notations`、`tied`、`tuplet`、`fermata`、`lyric`、`syllabic`、`text`;
- **direction 族**(rule_0_3 #7):`direction`、`direction-type`、`metronome`、`beat-unit`、`per-minute`、`sound`、`dynamics`(**8 级**)、`words`、`wedge`;
- **和声**(rule_0_3 #7):`harmony`、`root`、`root-step`、`root-alter`、`kind`、`bass`、`bass-step`、`bass-alter`;
- **歌名作者**(rule_0_3 #7):`work`、`work-title`、`identification`、`creator`。

**明确不输出**:见 §4.7 逐项列理由。

---

## 二、.tymp 输入格式精读

### 2.1 文件整体结构与实测格式事实

首版给出的"固定 9 行"结构图已**废弃**——rule_0_3 第 4/5 条要求按内容动态判定行角色。以下为**实测事实**(以 part-IV.txt 行 1–31 与 output/4 为准):

| 事实 | 实测值 | 意义 |
|------|--------|------|
| 行头左补空格对齐 | part-IV 中 `\|\|` 位于 0 基列 **62–63**,乐曲区自列 **64** 起;标题行与 instruments 行从列 0 起 | 元数据区靠右对齐;解析前须定位 `\|\|` 而非假定固定列 |
| 行尾右补 | 每行补齐到 ~10020 字符(MAX_NUM=10000 + 行头) | 必须先 `strip_trailing_spaces` |
| 4/4 每小节列数 | output/4 网格 `\|   .   .   .   ` → **16 列**(每拍 4 列) | accu = 16/分母 = 4 |
| 6/8 每小节列数 | part-IV 网格 `\| . . . . . ` → **12 列**(每拍 2 列) | accu = 16/分母 = 2 |
| 结论 | **1 列 = 16 分音符 = 1/4 拍,恒定** | 推翻首版「固定 accu=4」假设(见 §3.2) |
| `#timestamp:` | 行头标签 + `\|\|` + 网格(每小节首个标记为 `\|`,其余拍为 `.`,每拍占 accu 列) | 用于推断 accu;每组出现两处(旋律区前、伴奏区前) |
| 歌词行 | part-IV 行 8/10 = `/*lirics*/ *l\|" "`,**无 `\|\|`** | 元数据区终点须由行头 token 推定 |
| 组末行 | part-IV 行 31 = `}\|\|`,其后无组末元数据 | 组末元数据可空 |
| 伴奏行表头 | 旧版输出(part-IV、output/4、2_4.txt 等):每 3 行仅**第一行**有完整表头,后两行无表头;修改后的生成器:**每一行**都有表头(首组完整形态、续组短式) | 两种形态都须接受;行角色判定按内容(§7.2.2),行头只提供乐器/音区元数据 |

**结构示意(动态)**:

```
第 1 行:  「标题 by 作者」
第 2 行:  「instruments:结构id0 结构id1 …」
第 3 行起:  若干「组」({…}),组内各行的角色按内容动态判定:
     #timestamp:…            ← 网格行(跳过,推断 accu)
     { …                     ← 组头行(元数据 + 全行定义扫描)
     chords,…:               ← 和弦行(可选)
     …                       ← 机动行(可选,上指 | 或下指 \)
     …                       ← 数字行(声部之一)
     …                       ← 歌词行(可选,*l 分隔或裸按列)
     [ …                     ← 和声数字行(可多个,行数不限)
     ( …  / *Vln| / * #N:…   ← 伴奏数字行(六种行头形态)
     } …                     ← 组末行
```

> 要点:左大括号(`{`)之前的行可以是任何内容(位置参考),解析时忽略;组首元数据行不可省略,后续组可空置并沿用上一组(§2.2);组末行 `}` 之后的内容 = 本组组末元数据(可空,实测 part-IV 行 31 即空)。

### 2.2 组头元数据(rule.md §2)

| 记号 | 含义 | 转换器的处理 |
|------|------|--------------|
| `\|6/8`(`\|`可省略) | 从该竖线对应小节起拍号;只允许出现在组首行;曲首必须存在 | 解析 → `<time>`;缺省按上一组(曲首缺省 4/4 并告警);**同时决定 accu = 16/分母** |
| `\|=C` `\|=G` `\|=#G` `\|=Dm` | 调号表示方法 1(简谱视同);后缀 `~` 表示和弦行**不**跟随转调 | 解析 → `<key><fifths><mode>`;`~` 记录为和弦解析标志 |
| `\|@[7-3-6-]` | 调号表示方法 2:固定变音;C 调为 `\|@[0]` | 解析 → `KeyState.fixed_alter[8]`;能与标准调号等价时用 `<key>`,否则 fifths=0 走逐音 `<accidental>` |
| `\|-7-3-6` | 旧写法(等价 `\|@[7-3-6-]`) | 兼容接受 |
| `\|vb180`、`\|vb120.000000` | 绝对速度(从该拍起);曲首必须有,缺省 120 | 解析 → **输出 `<direction>` + `<sound tempo>`**(rule_0_3 #7);接受浮点 |
| `\|。` `\|vt` | 延音号 | 解析 → 输出 `<fermata>`(挂当前列音符);见 §4.5 |
| `\|vi-` `\|vi+` `\|rit` `\|rall` `\|accel`、`\|rit.` | 渐慢/渐快开始 | 输出 `<direction><words>rit./accel.</words></direction>` |
| `\|ve` `\|vr` `\|vb180` `\|vb`、`\|vb.` | 变速结束/回原速/至指定速 | `\|ve` 仅结束状态(不输出);`\|vr`/`\|vb` → `<words>A tempo</words>` |
| `\|ppp`…`\|fff`,`\|s1`~`\|s10` | 力度 | 输出 `<direction><dynamics>`;s1~s10 查 §4.5 表(rule.md 附录 B-6 匡正:十级力度,缺省力度 5 = mp) |
| `\|si-` `\|si+` `\|cresc` `\|dim` `<   <` `>   >` `\|sr` `\|pr` `\|sb` `\|pb` | 渐强/渐弱开始(`sr/pr/sb/pb` 同义) | 输出 `<wedge type="crescendo">`(dim 类为 `diminuendo`) |
| `\|se` | 渐强渐弱结束 | 输出 `<wedge type="stop"/>` |
| `{D/200232}` | 和弦把位定义(吉他);后定义覆盖先定义;缺省=常用把位表 | 解析入库(供 ③/⑤ 类查询) |
| `&gt: {a=E2,b=A2,…}` | 吉他六弦调弦表(字面即 "&gt" 四字符,避免与力度 `>   >` 混淆) | 解析入库(供 ③ 类 `a`~`f` 消解) |

> **注意**:`>` 有两个身份——`&gt:` 调弦表(字面四字符 + 冒号)与 `>n` 音区前缀(rule_0_3 #11)与力度 `>   >`,靠上下文与长度区分(§7.1)。

### 2.3 行头元数据(rule.md §3)

以「组头元数据区结束后的两个 `||` 的结束点」为分界,此前均为行头元数据区:

1. **乐器名**:主旋律行与 `[` 行不需要;中文/英文用 `<` 分割(`Nylon Guitar<古典尼龙吉他`)。
2. **初始音区**:`>4`(现行,rule_0_3 #11)/ `*4`(旧写法,兼容)。
3. **初始力度**:输出 `<direction><dynamics>`(§4.5)。
4. **特殊表记定义**(可在**元数据行的任何位置**,定义域为**整行**):
   - ① 瞬时:`x~{135}` / `x～{一三五}` / `x～{CEG}`;
   - ② 多瞬时:`x&"_{358}/7/3-6-"`;
   - ③ 吉他节奏型:同 ② 但用小写 `a`~`f`;
   - ④ 特殊和弦定义:`{Cmy/300353}`;
   - ⑤ 和弦节奏型:`x&r"{358}7654345"`;
   - **无引号形态**:`Q&{13}_{13}_{13}_`(实践 P7);
   - **参数化形态**:`X(V)&r"V_{35}_"`(rule_0_3 #5),代入值为绝对唱名;
   - 标记名规则:1~4 字母、不含 ABCDEFG、首尾非数字、不重复;
   - 使用处可写 `x@|3-6-7-4+` / `x@|E-A-B-F+` 施加临时绝对变音。

5. **六种伴奏行头形态**(§7.2.5 给出解析算法):
   `(Accomponiment#N:ID<EN; Initial pitch range: >4` / 省略关键词的变体 / `(-ID-` 短行头 / `(-ID-后缀`+下行全名 / `*#N:ID<Name$P` 简洁定义 / 谱内就地 `*Vln|`;**无行头 → 默认钢琴**。

### 2.4 曲谱记载规则(rule.md §4,列模型已修正)

1. **列模型**:1 列 = 16 分音符 = 1/4 拍;每拍列数 accu = 16/拍号分母;每小节列数 = 16×分子/分母。(首版「每拍 4 格」以 x/4 拍号为准。)
2. **「单位」双义**(与 rule.md §1.3 一致):`[…]` 类占位以**拍**计(1 单位 = 1 拍 = 16/分母 列);`%` 类与特殊表记声明以**列**计(1 单位 = 1 列 = 16 分音符)。
3. **事件字符**:
   - `1`~`8` 音级、`0` 停止/休止;
   - `(1+)` 同一音符标注(不补空格,时值 = 一拍);实践变体 `(1)`、`(5.)`、`(7)`;
   - `[123]` 占**1 拍**(= 16/分母 列)内部 n 等分;`[~123]` 每多一个 `~` 多占 1 拍(内部 3 等分);
   - `{12}` 同拍同刻双音;
   - `%234` 把 4 列(4 个十六分)三连音化(4/4 下恰为 1 拍;6/8 下跨 2 拍,见 §13-24);`%%234` 把 8 列三连音化;`%12345`、`%1234543` 把 8 列 5/7 连音化;
   - 特殊表记标记名本身是一个事件(可带参数:`X(3)`);
   - **`=`**:由同列机动内容替换(§7.2);
   - **`-`**:显式结束正在进行的节奏型/复制状态(实践 P16);
   - **`@lN`**:行复制记号(rule_0_3 #10);
   - **`>n`**:谱内音区前缀(rule_0_3 #11)。
4. **后缀标记**(需占下一个四分之一拍谱位;前后加 `_` 可防占用):`*` 升八度组(其前音)、`.` 降八度组(其后音)、`+` 升、`-` 降、`～` 还原、**`'` 升八度组(其前音,实践 P1,与 `*` 同义)**。
5. **八度判断**:纯四度(含)以内默认连续;超出取最近八度(§7.5)。
6. **歌词行**:与曲行一对一;`-` 成为延音线;可放力度;**`*l""` / `*l|""`(上指)/ `*l\""`(下指)分隔符**;引号内文字原样输出;拍内无歌词视同上文;**歌词与谱数字不必同列、但须同一行,按「逐字句一一对应」匹配**(v0.3.1 #5,见 §7.2.4)。
7. **和弦行**:可选;`-` 表示无和弦;`_` 标注根音位置。
8. **固定变音增减**:可在数字行内以 `|@[…]` / `\@[…]` 增减(rule_0_3 #3)。

### 2.5 机动行(rule.md §6;方向语义已定案)

**方向语义(作者确认,本方案与 rule.md 采用同一表述)**:

- **`|`(上指)= 该机动内容位于数字行之上,服务于其下面一行**;`|` 所在列即该标记的作用位置。
- **`\`(下指)= 该机动内容位于数字行之下,服务于其上面一行**;`\` 所在列即作用位置。
- **单独符号**(仅一个符号、无数字串):属"在下的机动行"时必须夹在 `\` 与 `\` 之间,**反斜杠不可省略**;属"在上方的机动行"时直接书写。
- 两个数字行**紧邻**时,只有"单独符号"与 `|` 开头的段落算机动内容,其余数字归其所在声部行(rule_0_3 #1)。
- 机动行最多**同时两个**(某数字行上、下各一);伴奏声部可**借用**邻行作机动行;**在上的机动行也可以有它的在上机动行,规范完全相同**(v0.3.1 #7,可向上堆叠)。
- 数字行**自身可携带机动段**(part-IV 行 17 的 `|{72}_{61}_{5+7}`),服务其下方数字行的同列 `=`。

**实测例证**:行 16 的 `|{42}_{31}_` 与行 17 的 `====X` 同列配对(上服务下 ✔);行 19 的单独符号 `'`/`.'`/`|@[0]` 服务行 20(上服务下 ✔);行 21 的 `\@[3-6-7-]`、`\ ' .' . \` 服务其**上方**行(下服务上 ✔)。

**`=` 替换**:数字行写 `=`,机动行同列写 `|内容` / `\内容` 代替之;`====` 连续等号只需标注起终点(也可连续标注);`|` 后紧跟若干 `_` 表示内部时值划分不同(`|__…` 把 4 列拆成 16 格);省略 `_` 则自动平分。机动行内空格用 `/` 代替。

**管道实例**:`|{42}_{31}`、`|{72}_{61}_{5+7}`、`|[{31'}_{27'}_{16-}_{75'}_]` —— `|` 前缀 + `{…}` 柱式格 / `[…]` 等分格 + `_` 分格 + 格内变音(`5+`/`3-`)。

### 2.6 rule_0_3 变更与 part-IV 实践变动逐条清单

> 本节是首版**未能覆盖**的全部条目,逐条列出(不合并),每条给出方案落点。R = rule_0_3.txt 追加章节;P = part-IV.txt 实践。

#### 2.6.1 rule_0_3 的 12 条改动

| # | 改动(rule_0_3.txt 行号) | 方案落点 |
|---|---|---|
| R1 | 机动行归属:机动行一般仅出现在数字行上方紧邻一格;但两个数字行可紧邻——只有"单独符号"与 `\|` 开头段落才算机动行,其余数字归其声部行(190-191) | §7.2 行角色动态分类 |
| R2 | 反斜杠下指:属"在下的机动行"的内容必须在 `\` 之后;单独符号须夹在 `\` 与 `\` 之间;反斜杠不可省略(192;正文 167-168) | §2.5 方向语义;`scan_aux_line` / `apply_aux_marks` |
| R3 | 固定变音改版 `\|@[7-3-6-]`(C 调 `\|@[0]`),`\|` 可由 `\` 代;可在数字行内增减(193;正文 46) | `parse_fixed_accidental`、`KeyState.fixed_alter`、`FixedAcc` |
| R4 | 伴奏排列灵活化:行数不定、按内容动态判定;可借用邻行作机动行;机动行最多 2 个;行首无乐器定义视为**钢琴**;新增简洁定义 `*#1:Inst#1<Instrument1$1`(`#1`/`<Instrument1`/`$1` 均可省略,`$1` 缺省先搜 `Inst#1` 再填钢琴;谱内就地 `*Vln\|` 自动编号与 id);**ID 独一化责任移交解析器**(194-209) | `classify_line`、`parse_inline_instrument`、`getInstrumentList`、ID 独一化步骤 |
| R5 | 和声/主旋律**行数不再限制为 3**;歌词在数字行正下方即可识别、与机动行一致是动态的;**歌词分隔符 `*l""` / `*l\|""`(上)/ `*l\""`(下)**,引号内文字即歌词(201-202) | §7.2 行分类;`parse_lyric_separator`、`attach_lyrics` |
| R6 | 参数化和弦定义 `X(V)&r"V_{35}_"`,解析时把调用点 `X(V)` 的 V 代入(注意 V 是**绝对唱名**,不是和弦根音/三音)(211-213) | `SpecialMarkDef.param`、`expand_special_mark` 文本代入 |
| R7 | 自动重复语义:大于一个单位时长的标记自动重复;引源是和弦(r)时每换和弦循环重头开始;等于一个单位时长的标记一次性(214-215) | `expand_special_mark` 的 `units`/`repeat` 逻辑;与 `=X`(P9)联动 |
| R8 | 数据结构实装:**歌名作者、力度、速度、变速、和弦行、无音高打击乐器**;无音高打击若为鼓组映射鼓谱(常用),否则任何数字同等解读为一次发音(216-217) | §5.1 tymp.h 加法扩展;§4.5 输出;§7.10 打击处理 |
| R9 | 特殊表记定义可发生在元数据行**任何位置**,定义域为**整行**(218-219) | §2.3-4、`parse_group_head`/`parse_line_head` 全行扫描 |
| R10 | 新记号 `@l9`:发生在任意数字行,表示从此以后直到被某事件覆盖,该声部均存在,且复制**文档总第 9 行**的内容(220-221) | `CopyState`、`resolve_line_copy`、§7.9 |
| R11 | 音区前缀 `*` → `>`,允许出现在**正文内部**:`>4\|`、`\|>4\|`、`\>4\|`(223-224) | `>`/`*` 双轨前缀;`scan_digit_line` 的谱内前缀事件 |
| R12 | 和弦 `_` 标注根音位置:`G_6`、`F_46`、`Em7_34`、`Bmin7_56`、`bBmaj7_2`(自注"根音分别是 7,1,7,2,6");七和弦"3/5/7 音"= 根向上第 2/3/4 音;三和弦"2/4/6/7 音"= 紧邻 1/3/5 音上方、紧邻 8 音下方的调内音(226-233) | `ChordSym.bass[]`、`parse_chord_bass`、⑤ 类消解;规则例句列为**实现期验证向量** |

#### 2.6.2 part-IV.txt 实践变动(P1–P21)

| # | 实践写法 | 位置 | 方案落点 |
|---|---|---|---|
| P1 | **`'` = 升八度组后缀**(规则未记载,实践 10 行出现;与 `*` 同义) | 行 6/7/17/18/19/21/24 | §2.4-4;`scan_digit_line` 后缀集;`resolve_degree_pitch` 的 shift |
| P2 | `>4` 音区前缀,与旧 `*3`/`*4` 并存 | 行 7/17/20/23/26/29;output/4 全用 `*` | §3.2/§7.5;行头与谱内两处 |
| P3 | `@l7` / `@l9`(行头 `>4\|\|@l7`、行中 `…3 @l7`、行尾 `\|@l7  \|` 三处位置) | 行 18/24/29/30 | §7.9 `resolve_line_copy` |
| P4 | 管道柱式和弦 `\|{42}_{31}_`(行16@156 与行17 `====X`@156 同列配对)、`\|{72}_{61}_{5+7}`、`\|{572}_{4+61}_{3-57}_` | 行 16/17 | `parse_pipe_instance`、`apply_aux_marks` |
| P5 | 管道括号等分 `\|[{31'}_{27'}_{16-}_{75'}_]`、`\|[{3-5}{24}{13-}{72}]`(与行18 裸 `=` 同列配对) | 行 17 | 同上(`PIPE_BRACKET` 类) |
| P6 | 参数化调用 `X(3)`、`T(4)`、`T(7)` | 行 17-18 | `expand_special_mark` 参数代入 |
| P7 | **无引号定义** `Q&{13}_{13}_{13}_` | 行 4 | `SpecialMarkDef.quoted=false` |
| P8 | 定义内 `'` / `.` / `8` / `0` / `_` / `(7/8)` / 首导根音 `1_` | 行 4 | 表记内容解析器(§8) |
| P9 | **等号与标记名连用** `====X`、`======X`、`====W`、`=T(4)`、`=R` | 行 17-18 | **rule.md 附录 B-1 匡正**:等号段由**同列机动段**替换;`=` 后跟的标记名属于**其后**内容,与等号段无关(§8 P9) |
| P10 | `\|rit.` / `\|vb.` 带句点;`\|vb120.000000` 浮点速度 | 行 4 + 组尾列 | `scan_meta_mark` 记号集与句点容错 |
| P11 | 混搭行头(`/*LV.pitch range of 1st note:*/ >4`、`(Accomponiment#N:Inst.#N<Instrument#N;  >4` 两种)、`/*lirics*/` 拼写、双行行头 `(-ID-后缀`+下行全名、短行头 `(-ID-` | 行 7/8/17/20;output/4 | §7.2.5 六形态行头解析 |
| P12 | 和弦行:`-`=无和弦、`#Fmin`、`bAmaj_46`、`G7_56 G7_34 G7_2` 多根音、紧排多和弦 | 行 5 | `resolve_chords_line` |
| P13 | 22 个表记定义散布 `{` 行 `\|\|` 之后的乐曲区列位,与 `\|rit.`/`\|vb.` 混排 | 行 4 | 全行扫描(R9) |
| P14 | 歌词行**无 `\|\|`**,以行头 token 推定元数据区终点(`/*lirics*/ *l\|" "`) | 行 8/10 | `find_metadata_end` 回退链 |
| P15 | 大跨度裸括号 `(   …   )`(实测跨行7 列 1263–1360,约 98 列) | 行 7 | **rule.md 附录 B-8 匡正**:从属于人声段的括号 → 段内内容在补全副本阶段**转移至伴奏声部**,不分配歌词;否则跳过 + 告警 |
| P16 | `**`(行12`\|\|**`,作者确认为笔误)、`-H`(应拆为 `-` 显式结束 + `H` 标记调用)、错位孤立 `+`(忽略) | 行 12/21 等 | `-` 记号处理;其余跳过 + 告警 |
| P17 | 紧排跑句(每列一音)与一拍一音(每拍一音)与长持音并存;`0` 休止 | 行 7 | 列模型(§1.3);`scan_digit_line` |
| P18 | 括号单音变体 `(1)`、`(5.)`、`(7)` | 行 18 | `scan_digit_line` 的 `(n)` 解析扩展 |
| P19 | `\@[7-6-3-]`、`\@[0]`、`\@[3-6-7-]`;`\'....`、`\ ' .' . \`(下指机动内容) | 行 19/21 | §2.5 方向语义;`scan_aux_line` 的 `\` 段 |
| P20 | 歌词 `*l\|" "` 已实际使用(现为空);**歌词任何字符原样输出**;`-` 仍为延音线;拍内无歌词视同上文 | 行 8/10 | `parse_lyric_separator`/`attach_lyrics` |
| P21 | 未闭合引号容错(`J&r"1353531'3.3'5'5.` 无闭合引号) | 行 4 | **rule.md 附录 B-11 匡正**:取至**其后第一个空格**(以空白终止)+ 告警 |

#### 2.6.3 与首版结论的**冲突修正**

| 项 | 首版结论 | 本版修正 |
|---|---|---|
| 每拍格数 | 固定 accu=4,6/8 每小节 24 列 | **accu = 16/分母**(6/8 → 2),6/8 每小节 12 列;1 列恒 = 16 分音符 |
| divisions 公式 | `4 × accu × L` | `4 × lcm(全部有理分母)`(§3.2;两者在 4/4 下数值相同,6/8 下不同) |
| 机动行方向 | 未定,按"服务下方"处理 | `|` 服务下方、`\` **服务上方**(作者确认) |
| 行结构 | 固定旋律区 3 行 / 和声 2 行 / 伴奏 3 行 | **全部动态判定**,和声与主旋律行数不限;机动行可向上堆叠(v0.3.1 #7) |
| 力度/速度/和弦行/无音高打击/歌名作者 | 解析校验但不输出 | **全部输出**(rule_0_3 #7 + 约束 C2 修订) |
| `tymp.h` | 不修改 | **加法扩展**(§5.1);并修正首版 §4.5 的一处笔误——`note_pitch_mxml` 原本**没有** time-modification 字段 |
| 歌词附着 | 按列分拍(每拍首字符,CJK 2 列) | **逐字句一一对应**,不依赖列(v0.3.1 #5);拍内无歌词视同上文保留 |
| 转换流程 | 直接解析原文件 | **先补全成 .tymp 副本再解析**(v0.3.1 #6,§7.0);`@lN` 不复制歌词 |
| 和弦品质 `maj` | (未特别定义) | **`maj` 三和弦 = 增三和弦**(v0.3.1 #11;`maj7` 不受影响) |

#### 2.6.4 v0.3.1 版本调整(rule_0_3.txt「v0.3.1 版本调整」,V1-V11)

| # | 调整(rule_0_3.txt v0.3.1) | 方案落点 |
|---|---|---|
| V1 | `@l13` 复制调整行,但是**不复制其歌词** | §7.9;`resolve_line_copy` |
| V2 | `@l13^+3`:在第 13 行音轨的**上方三度**置平行和声。`lN^M`:符号代表方向(+/-),数字代表上/下方几度;\|M\| 必须为 7n+{3,4,5,6,8}(n 非负)。取音:a. 该行音是此刻和弦内音 → 寻找这一和弦以这个音为根音的那个形式,按 §7.3「诸音定义」取对应音;b. 否则取该行调号下的调内音(上/下方 \|M\| 度);c. 持续到下一个有效符号 | §7.9 平行和声;在补全副本阶段展开(§7.0) |
| V3 | `H_6`:H 为第一行中按和弦节奏型声明的标记;用**此处活动和弦**与 **6 所代表的根音位置**替换**此行**解析所得和弦,不影响其他行(如活动 `G_46` 处写 `H_6` → 本行按 `G_6`)。`_3`、`_5` 代表原位三和弦;`_7` 代表原位七和弦 | §7.3;`parse_chord_bass`、`scan_digit_line` |
| V4 | 特殊表记声明内部,引号内可以出现**空格** | §8 格解析(空格并入 content,不视为分隔) |
| V5 | 歌词不一定和谱数字位于同一列,但一定位于相同的行;以「**逐字句完成了对应**」为标准,依次一一对应 | §7.2.4;`attach_lyrics` |
| V6 | 生成 musicxml 时,先将特殊表记与 `@l13`/`@l13^3` 等简化写作**补全**,生成一个扩展名为 `.tymp` 的副本;补全时统一将调整行放于**上方**(该行该处为空);否则先在其上方**生成一个新空行**存放调整行 | §7.0 第零遍;`make_tymp_copy` |
| V7 | 在上的机动行也可以有它的在上机动行,规范完全相同 | §2.5、§7.2.3(堆叠绑定) |
| V8 | 全部和弦的吉他把位应由**数据库**提供(经把位标注转换音位);新增把位表注声明 `X&g"xxxxxx"`,x=1..6 代表弦(1 为低) | §8 ⑥类;`parse_voicing_g` |
| V9 | 维护工具:增添空乐谱行;改某行拍号但不改变精度;对某行增加空伴奏行(须指明行)或空和声/旋律行;**增行操作须对其下方所有 `@lxx` 的行号相应修改** | 属**生成侧工具**,不在本转换器范围(§13-30);转换器消费其产出(行号以当前文档为准) |
| V10 | 支持不同乐谱行具有不同的「歌名 by 作者」,分别生成两个 musicxml 文件(原文「革命」系「歌名」之误) | §1.1-10、§7.0、`main` 多输出 |
| V11 | **`maj` 三和弦是增三和弦,而不是大三和弦** | §4.5 `<kind>` 映射、§7.3 |
| V12 | **行角色判定不再通过表头**(仅节拍行 `#timestamp:` 例外);生成器改为**每一行伴奏行都写出行头**(首组完整形态、续组短式) | §7.2.2 重写;`newfileMaker_full0_1.c` 已同步修改;新旧两种输出形态都须接受 |

---

## 三、输出格式规范(依据 musicxmlEg.xml)

### 3.1 文档骨架

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC
  "-//Recordare//DTD MusicXML 4.0 Partwise//EN"
  "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="4.0">
  <work><work-title>标题</work-title></work>                        ← rule_0_3 #7 新增
  <identification><creator type="composer">作者</creator></identification>
  <part-list> … </part-list>
  <part id="…"> … </part>
  …
</score-partwise>
```

- `initNewMusicXML` 写 `xmlHead`(原样,与 `tymp.h` 一致);随后按 DTD 顺序写 `work?` → `identification?` → `part-list` → `part+`。
- 输出一律 UTF-8。
- 标题**原样保留**(如 `4-1-说了再见再跳舞` 的前缀 `4-1-` 不剥离)。

### 3.2 列、单位与 divisions 计算(**本版修正**)

**坐标模型**

```
1 列    = 16 分音符 = 1/4 拍(拍 = 四分音符)
accu    = 每拍列数 = 16 ÷ 拍号分母          (4/4 → 4;6/8 → 2)
列/小节  = 16 × 拍号分子 ÷ 拍号分母          (4/4 → 16;6/8 → 12)
1 单位  = 1/4 拍 = 1 列
```

accu 的确定:**优先取该组 `#timestamp` 网格的实测间距 ÷ 拍号分子**;无网格行时按上式计算;与实测不符时以实测为准并告警。

**divisions 公式**

```
要求:  4 | divisions,且对每个「相对列长的有理数分母 q」有 q | (divisions/4)
        其中 q 来自:连音(%-类)实际音数 ÷ 声明列数、[…] 等分、管道段内部细分等的最简分母
推荐:  divisions = 4 × lcm(全部 q),最小 4
```

- 推导:某记号占 L 列分 n 个音 → 每音 L/n 列(约分 p/q)→ 每音 tick = p·divisions/(4q);因 gcd(p,q)=1,须 q | divisions/4。
- 例:`%234`(4 列,3 音)→ 每音 4/3 列 → q=3;`%12345`(8 列,5 音)→ 8/5 → q=5;6/8 下 `[123]`(1 拍 = 2 列,3 音)→ 2/3 → q=3 → divisions = 4×lcm(3,5) = **60**。
- 若对某记号算出的 tick 无法整除(理论上不该发生),告警并把该音时值四舍五入到最近 tick。
- **每列 tick = divisions/4**(恒为整数,由 4 | divisions 保证)。

### 3.3 时值的 MusicXML 表示算法

任意事件时值(以「拍」为单位的有理数 d)按下述顺序判定(§6 `dur_to_type`):

1. 若 d = 4/T(T∈{16,8,4,2,1}):`type` = 16th/eighth/quarter/half/whole,无点、无 `<time-modification>`;
2. 否则若 d = 6/T:`type` = T,`isDot = true`(单附点);
3. 否则按连音表示:取最大的 T 使 4/T ≥ d(不够则 T=16);约分 d·T/4 = p/q,则
   `<time-modification><actual-notes>q</actual-notes><normal-notes>p</normal-notes>[<normal-type>T</normal-type>]</time-modification>`
   (当 T 恰等于本音 `type` 时省略 `normal-type`,与 musicxmlEg.xml 一致);
4. tick 数 = (列数 × divisions) / 4,须为整数(由 §3.2 构造保证)。

> `note_pitch_mxml.time_modification` 是 §5.1 新补字段(首版 §4.5 曾误以为已存在)。

### 3.4 小节模型与事件流

- 每小节宽度 = 16 × 分子 ÷ 分母 列;数字行/机动行/歌词行的乐曲区按此宽度切分为小节。
- 音符时值 = 事件起点列 → 下一事件起点列(§7.1);跨小节者由 §7.4 拆分。
- **声明长度类**(`%` 系、`[…]` 系、`{…}` 系、特殊表记调用、`=` 段)以其声明长度定长(`%` 系/标记/`=` 段以列计;`[…]` 系以拍计,拍 → 列 = 16/分母):
  - 列距 = 声明长度 → 直接使用;
  - 列距 > 声明长度 → 多出的列距记为**休止**并入其后,并告警(提示作者补齐);
  - 列距 < 声明长度 → 按列距截断内部划分并告警。
  - (书写宽度与声明长度可以不等,例:`[123]` 5 字符 vs 1 拍 = 4 列。)
- 休止:由 `0` 起始,持续到下一事件;整小节休止 → `<rest measure="yes"/>`。
- 和弦 `{…}` 与特殊表记展开的同刻多音:第一音写 duration,其余写 `<chord/>`(duration 相同)。
- **多行声部 → 输出模型**:同一乐器的每个数字行输出为**独立 `<part>`**(id 加 `-2`/`-3` 后缀、同名 part-name、独立 midi-channel)。理由:part-IV 中同乐器多数字行存在**时间重叠**(行 20 密集跑句与行 22 稀疏低音,列 1048–1145),独立 part 无需 `<backup>`(C2 精神)即可正确表达,且 MuseScore 直读。

### 3.5 direction / harmony 的定位与排序

- `direction` 与 `harmony` 不进 `note_pitch_mxml`,而挂在 `Measure` 的**有序链表**上,内部字段 `col`/`offset` 仅作**排序键**。
- 输出时机:在 `write_measure` 内,把 direction/harmony 与音符**按列(offset)归并输出**——某记号的列 ≤ 某音符的起点列时,先输出该记号。
- **v0.3.1+2 修订**:`direction` 仍按文档顺序输出(不用 `<offset>`);**`<harmony>` 使用 `<offset>` 元素按列位码放**——同一小节内从第二个和弦起,输出 `<offset>`(值 = (该和弦列 − 上一和弦列) × 列 tick)使各和弦落在其声部对应位置;首个和弦若不在第 0 列同样输出 `<offset>`(相对小节起点)。
- 同列时顺序:先 `harmony`,再 `direction`,最后 `note`。

---

## 四、MusicXML 输出要素清单(一一详细)

> 本节是输出端的完整要素规格。每个要素给出:来源、输出时机、精确写法、与 musicxmlEg.xml 的对照。§4.7 列出明确不输出的要素及理由。

### 4.1 文档级要素

| 要素 | 来源 | 时机 | 精确写法 |
|------|------|------|----------|
| `<?xml version…?>` / DOCTYPE / `<score-partwise version="4.0">` | `tymp.h` 之 `xmlHead` | 打开输出文件时 | 原样 |
| `<work>` | `Score.work_title`(第 1 行 `" by "` 之前) | part-list **之前** | `<work><work-title>4-1-说了再见再跳舞</work-title></work>` |
| `<identification>` | `Score.composer`(`" by "` 之后) | `<work>` 之后、part-list 之前 | `<identification><creator type="composer">久未至</creator></identification>` |
| `</score-partwise>` | — | 全部 part 写完后 | 原样 |

> 标题或作者缺失时对应元素整体省略(不输出空标签)。

### 4.2 part-list 要素

| 要素 | 来源 | 时机 | 精确写法 |
|------|------|------|----------|
| `<part-list>` | — | 所有 `<part>` 之前 | 包住所有 `<score-part>` |
| `<score-part id="…">` | 声部 id:主旋律 `P0`;第 i 个非主旋律数字行 `Pi`;伴奏 = 结构 id **经独一化清洗** | 每个参与输出的声部一个 | `<score-part id="NGTR1">`、`<score-part id="Inst.-0">` |
| `<part-name>` | `Instrument.part_name_En`(instruments.h 原型表);旋律/和声无乐器 → `Melody` / `Harmony`(第 2 个起 `Harmony 2`…);**同乐器多数字行**(row ≥ 2)→ 同名不重复编号 | 每个 score-part 内 | `<part-name>Nylon Guitar</part-name>` |
| `<score-instrument id="…-I1">` | 乐器 id = score_part_id + `-I1` | 同上 | `<score-instrument id="NGTR1-I1">` |
| `<instrument-name>` | 同 part-name | 同上 | `<instrument-name>Nylon Guitar</instrument-name>` |
| `<midi-instrument id="…-I1">` | 同 score-instrument id | 同上 | 原样 |
| `<midi-channel>` | 按输出顺序 1,2,3,… 递增 | 每个声部一个 | `<midi-channel>1</midi-channel>` |
| `<midi-program>` | `InstrumentPrototype.gm_program`(仅 INS_PITCHED) | 同上 | `<midi-program>25</midi-program>` |
| `<midi-unpitched>` | `InstrumentPrototype.unpitched`(仅 INS_UNPITCHED,GM 鼓键位) | 同上 | `<midi-unpitched>38</midi-unpitched>` |

**三分支**(§6.2 `makeInstrumentPart`):

1. **INS_PITCHED** → `midi-channel` + `midi-program`;
2. **INS_UNPITCHED**(单件无音高,如 `SD1` 军鼓)→ `midi-channel` + `midi-unpitched`,**不写 `midi-program`**;
3. **INS_DRUMKIT**(架子鼓)→ `midi-channel` + `midi-program`(=1)+ **为该声部内出现的每件鼓各写一个 `<score-instrument id="…-I2">…<instrument-name>…</instrument-name></score-instrument>` 及其 `<midi-instrument><midi-unpitched>键位</midi-unpitched></midi-instrument>`**,音符以 `<instrument id="…-I2"/>` 逐音引用(§7.10)。

### 4.3 part / measure 级要素

| 要素 | 来源 | 时机 | 精确写法 |
|------|------|------|----------|
| `<part id="…">` | 同 score-part id | 每个声部一个 | `<part id="NGTR1">` |
| `<measure number="n">` | 全曲小节全局编号 1,2,3…(跨组连续) | 每个小节 | `<measure number="1">` |
| `<attributes>` | — | **该声部第 1 小节**,以及调号/拍号变化的小节 | 见下 |
| `<divisions>` | §3.2 全局 divisions | 上述 attributes 内 | `<divisions>60</divisions>` |
| `<key>` | 当前 `KeyState` | 同上 | `<key><fifths>0</fifths><mode>major</mode></key>` |
| `<fifths>` | 调号等价五度圈数(§7.6) | 同上 | `<fifths>-3</fifths>` |
| `<mode>` | 大调 `major` / 小调 `minor` | 同上 | `<mode>major</mode>` |
| `<time>` | 当前 `TimeState` | 同上 | `<time><beats>6</beats><beat-type>8</beat-type></time>` |
| `<beats>` / `<beat-type>` | 拍号分子/分母 | 同上 | 同上 |
| `<clef>` | 有音高声部一律 G 谱号;无音高打击/鼓组用打击谱号 | 同上 | `<clef><sign>G</sign><line>2</line></clef>` / `<clef><sign>percussion</sign><line>2</line></clef>` |
| `<direction>` / `<harmony>` | `Measure.dirs` / `Measure.harms` | 小节内按列归并(§3.5) | 见 §4.5 |

### 4.4 note 级要素(按 DTD 顺序输出)

> 固定顺序:①`<chord/>`(若有)②`<pitch>` 或 `<unpitched>` 或 `<rest>` ③`<duration>` ④`<tie>`×0~2 ⑤`<instrument id="…"/>`(若有)⑥`<voice>` ⑦`<type>` ⑧`<dot/>`(若有)⑨`<accidental>`(若有)⑩`<time-modification>`(若有)⑪`<notations>`(若有)⑫`<lyric>`(若有)

| 要素 | 来源(tymp.h 字段) | 时机 | 精确写法 |
|------|-------------------|------|----------|
| `<chord/>` | `isChord == true` | 同刻和弦的第 2 个及以后各音;**在 `<pitch>` 之前** | `<chord/>` |
| `<rest/>` / `<rest measure="yes"/>` | `isRest` | 休止;占满整小节者用 `measure="yes"` | `<rest/>` |
| `<unpitched>` | `is_unpitched == true` | 无音高打击音符 | 包住 display-step/display-octave |
| `<display-step>` / `<display-octave>` | `display_step` / `display_octave`(键位表 §7.10) | 同上 | `<display-step>C</display-step><display-octave>5</display-octave>` |
| `<pitch>` | `!isRest && !is_unpitched` | 有音高音符 | 包住 step/alter/octave |
| `<step>` / `<alter>` / `<octave>` | `pitch.step` / `pitch.alter` / `pitch.octave` | 同上 | `<alter>` 仅 ≠0 时输出;`<octave>` 1~7 |
| `<duration>` | `duration`(tick) | 每个音符(含休止、和弦各音) | `<duration>60</duration>` |
| `<tie type="start\|stop"/>` | `tie[0]`、`tie[1]` | 每个延音线端点 | `<tie type="start"/>` |
| `<instrument id="…"/>` | `instrument_ref`(鼓组逐音符引用;其余 NULL 不输出) | 鼓组音符 | `<instrument id="DRUMS-I2"/>` |
| `<voice>` | `voice`(当前恒 1;每数字行独立 part,故不引多 voice) | 每个音符 | `<voice>1</voice>` |
| `<type>` | `type` 枚举 16/8/4/2/1 | 每个音符(含休止) | `<type>eighth</type>` |
| `<dot/>` | `isDot` | 附点时(单附点) | `<dot/>` |
| `<accidental>` | `accidental` | 该音非当前调号调内音时(§7.7) | `sharp`/`flat`/`natural` |
| `<time-modification>` | `time_modification`(**本版新补字段**) | 连音音 | `<actual-notes>3</actual-notes><normal-notes>2</normal-notes>[<normal-type>…]` |
| `<notations>` → `<tied>` | `notations[i].notation_type == tied` | 延音线端点(与 `<tie>` 双写) | `<tied type="start"/>` |
| `<notations>` → `<tuplet>` | `notation_type == tuplet` | 连音组首/末音 | `<tuplet type="start" number="1" bracket="no" show-number="actual" placement="above"/>` / `<tuplet type="stop" number="1"/>` |
| `<notations>` → `<fermata>` | 延音号 `\|。`/`\|vt` 所在列的音符 | 见 §4.5 | `<fermata/>` |
| `<lyric>` → `<syllabic>` / `<text>` | `lyric.syllabic`(全 `single`)/ `lyric.text` | 有歌词的音符(休止不附着) | `<lyric><syllabic>single</syllabic><text>我</text></lyric>` |

### 4.5 direction / harmony / unpitched 级要素(**rule_0_3 #7 新增**)

**`<direction>`**

| 记号 | 输出 |
|------|------|
| `\|vb120` / `\|vb120.000000` | `<direction><direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>120</per-minute></metronome></direction-type><sound tempo="120"/></direction>` |
| `\|rit` `\|rall` `\|vi-` `\|rit.` | `<direction><direction-type><words>rit.</words></direction-type><sound tempo="…"/></direction>`(tempo 取当前绝对速度 ×0.9 之类的**降速值**? — **不**,见下注) |
| `\|accel` `\|vi+` | `<direction><direction-type><words>accel.</words></direction-type>…</direction>` |
| `\|vr` `\|vb` `\|vb.` | `<direction><direction-type><words>A tempo</words></direction-type><sound tempo="初始速度"/></direction>` |
| `\|ppp`…`\|fff` | `<direction><direction-type><dynamics><f/></dynamics></direction-type></direction>` |
| `\|s1`~`\|s10` | 查表:`s1=pppp` `s2=ppp` `s3=pp` `s4=p` `s5=mp` `s6=mf` `s7=f` `s8=ff` `s9=fff` `s10=ffff`;曲首缺省力度 = **5(mp)**(rule.md 附录 B-6 匡正) |
| `<   <` `\|si-` `\|cresc` `\|sr` `\|pr` | `<direction><direction-type><wedge type="crescendo"/></direction-type></direction>` |
| `>   >` `\|si+` `\|dim` `\|sb` `\|pb` | `<direction><direction-type><wedge type="diminuendo"/></direction-type></direction>` |
| `\|se` | `<direction><direction-type><wedge type="stop"/></direction-type></direction>` |
| `\|。` `\|vt` | 不生成独立 direction,而是给**该列的音符**加 `<notations><fermata/></notations>` |
| 组尾(`}` 后)的力度 | 同力度规则(全局),输出到**该组最后一小节的末尾** |
| 需要下置时 | 加 `placement="below"`(如 musicxmlEg.xml 的 `<direction placement="below">`) |

> **注**:`\|rit`/`\|accel` 类**不输出 `<sound tempo>`**——变速的精确速度曲线不由 .tymp 给出;`<sound tempo>` 仅在 `\|vb` 类绝对速度处输出,回原速处输出初始 `vb` 值。

**`<harmony>`**(和弦行)

```xml
<harmony>
  [<offset>30</offset>]
  <root><root-step>C</root-step>[<root-alter>1</root-alter>]</root>
  <kind>major</kind>
  [<bass><bass-step>E</bass-step>[<bass-alter>-1</bass-alter>]</bass>]
</harmony>
```

- `<offset>`(v0.3.1+2):小节内非首个和弦按 §3.5 输出,`<harmony>` 的第一个子元素(位于 `<root>` 之前)。
- `<kind>` 取值映射:`m`/`min` → `minor`;**`maj` → `augmented`(增三和弦,v0.3.1 #11,不是大三和弦)**;`maj7` → `major-seventh`(不受 #11 影响);**`min7`/`m7` → `minor-seventh`(v0.3.1+2)**;`7` → `dominant`;`dim` → `diminished`;`-64`/`-6` 等无标准对应 → 忽略位置后缀;
- 品质解析修正(v0.3.1+2):`maj7`/`dim` 中的 `m` **不算**小三和弦标志;
- `_` 标注的低音(如 `F_46`)按 §7.7 的**根音位置标注**消解为 (step, alter) 后写 `<bass>`;每个 `<harmony>` 至多一个 `<bass>`。
- `-`(无和弦)不输出任何 `<harmony>`。

### 4.6 与 tymp.h 字段的逐一映射(完备性检查)

| tymp.h 字段 | 输出 | 说明 |
|-------------|------|------|
| `isChord` | `<chord/>` | |
| `isRest` | `<rest/>` / `<rest measure="yes"/>` | |
| `is_unpitched` + `display_step`/`display_octave` | `<unpitched>` | **本版新增字段** |
| `pitch.step/alter/octave` | `<step>`/`<alter>`(≠0)/`<octave>` | |
| `duration` | `<duration>` | |
| `type` | `<type>` | 枚举 16/8/4/2/1 |
| `tie[0..1]` | `<tie type>` ×1~2 | |
| `instrument_ref` | `<instrument id="…"/>` | **本版新增字段**(鼓组) |
| `voice` | `<voice>` | |
| `isDot` | `<dot/>` | |
| `accidental` | `<accidental>` | |
| `time_modification` | `<time-modification>` | **本版新补字段**(首版误记已存在) |
| `notations[0..1]` | `<notations>` 内 `<tied>`/`<tuplet>`/`<fermata>` | `attribute2` = tuplet 组号 |
| `lyric.syllabic` / `lyric.text` | `<syllabic>` / `<text>` | |
| `direction_mxml`(新) | `<direction>` 族 | 解析阶段由 `DirectionMark` 产生 |
| `harmony_mxml`(新) | `<harmony>` | 解析阶段由和弦行产生 |
| `Score.work_title` / `composer`(新) | `<work>` / `<identification>` | |

`note_tymp` 各字段来源:`solfa`(音级 1~8)、`alter`、`octave`、`dur`、`pos`、`isChord`、`lyc`;换算见 §6.3 `ev2mxml`。

### 4.7 明确不输出的要素(及理由)

| 要素 | 来源 | 不输出的理由 |
|------|------|--------------|
| `<beam>` | musicxmlEg.xml 三连音处有 | tymp.h 无 beam 字段(C2) |
| `<stem>`、`<staff>`、`<print>`、`<backup>`、`<forward>` | 均未见于音乐规范 | C2;多行声部用独立 part 表达(§3.4),故不需要 backup |
| `<transpose>` | instruments.h 注释提及移调乐器 | 不实现(扩展点) |
| `<slur>`、`<figure-bass>`、`<barline>` | 规范无对应记号 | C2 |
| `<offset>` | MusicXML 通用 | v0.3.1+2 起 **`<harmony>` 使用**(§3.5):同小节多和弦按列位码放;`direction` 仍不用 |
| `<movement-title>` | MusicXML 通用 | 标题走 `<work><work-title>`(rule_0_3 #7 的"歌名") |
| 大跨度裸括号 `(…)` 的连线 | part-IV 行 7 | 不产连线;人声段括号 → 内容在补全副本阶段转移至伴奏声部、不分配歌词(rule.md 附录 B-8 匡正);否则跳过 + 告警(P15) |

---

## 五、文件划分与内部数据结构

```
tymp.h              ← 【本版:加法扩展】(输出结构与 xmlHead;只增不改,见 §5.1)
instruments.h       ← 不修改(乐器原型表)
rule.md             ← 不修改(权威规则文本)
tymp_internal.h     ← 新增:解析器内部类型(见 §5.2)
tympUtils.c         ← 新增实现:工具函数(字符串/乐理/时值/行分类/复制)
tymp2musicXML.c     ← 扩展现有骨架:解析管线 + 转换 + 输出 + main
```

### 5.1 tymp.h 加法扩展(rule_0_3 #7;不破坏现有字段)

```c
/* ---- note_pitch_mxml 追加字段(现有 11 个字段全保留) ---- */
time_modification time_modification;  /* 【新补】连音参数;非连音时 actualNotes=0 */
bool  is_unpitched;                   /* 【新增】无音高打击音符 */
char  display_step;                   /* 【新增】<display-step>(打击音符谱面位置) */
int   display_octave;                 /* 【新增】<display-octave> */
char *instrument_ref;                 /* 【新增】<instrument id="…"/>;鼓组逐音符引用;NULL 不输出 */

/* ---- 新增:direction 事件(rule_0_3 #7 力度/速度/变速) ---- */
typedef enum DirectionKind {
    DIR_TEMPO, DIR_DYNAMIC, DIR_WORDS, DIR_WEDGE, DIR_FERMATA
} DirectionKind;

typedef enum mxml_dynamic {          /* 10 级(rule.md 附录 B-6 匡正) */
    DYN_PPPP, DYN_PPP, DYN_PP, DYN_P, DYN_MP, DYN_MF, DYN_F, DYN_FF
} mxml_dynamic;

typedef struct direction_mxml {
    DirectionKind kind;
    double tempo;        /* DIR_TEMPO:|vb120 → metronome + <sound tempo>;DIR_WORDS 回原速时同用 */
    char  *words;        /* DIR_WORDS:"rit." / "accel." / "A tempo" */
    int    dynamic;      /* DIR_DYNAMIC:mxml_dynamic */
    int    wedge;        /* DIR_WEDGE:0 无;1 crescendo start;-1 stop;2 diminuendo start;-2 stop */
    int    col;          /* 小节内起点列(排序键;不输出 <offset>) */
    char   placement;    /* 0 = 默认;'b' = placement="below" */
    struct direction_mxml *next;
} direction_mxml;

/* ---- 新增:harmony(rule_0_3 #7 和弦行) ---- */
typedef struct harmony_mxml {
    char root;           /* A~G */
    int  root_alter;     /* -1..1 */
    char kind[16];       /* major/minor/major-seventh/dominant/diminished/other */
    int  has_bass;
    char bass_step;      /* <bass-step> */
    int  bass_alter;
    int  col;            /* 小节内起点列(排序键) */
    struct harmony_mxml *next;
} harmony_mxml;

/* ---- 新增:标题作者(rule_0_3 #7 歌名作者) ---- */
/* 由 Score 携带:char *work_title; char *composer; —— 见 §5.2 */
```

### 5.2 tymp_internal.h 内部类型

```c
typedef struct { int num, den; } Fraction;            /* 通用有理数(以拍为单位,1 拍 = 四分音符) */

typedef struct {                                      /* 调号状态 */
    char root; int alter; bool is_minor; bool chord_no_follow;
    int  fifths;                                      /* 等价标准调号五度圈数 */
    int  fixed_alter[8];                              /* 【新增】度 1~7 的固定变音 -1/0/+1(|@[…]) */
} KeyState;

typedef struct { int num, den; } TimeState;           /* 拍号 */
typedef struct { char str[6][4]; bool valid; } GuitarTuning;   /* &gt: {a=E2,…} */

typedef struct SpecialMarkDef {                       /* ①~⑤ 特殊表记定义 */
    char name[8];        /* 标记名(1~4 字母) */
    int  kind;           /* 1..5;6 = 吉他把位表注 X&g"xxxxxx"(v0.3.1 #8) */
    bool chord_relative; /* ⑤ / ③:r —— "引源是和弦" */
    bool quoted;         /* 【新增】是否带引号(P7 无引号形态为 false) */
    char param;          /* 【新增】参数化定义的参数名(如 'V');0 = 非参数化 */
    int  total_units;    /* 【新增】声明长度(单位数),供 R7 自动重复与 = 段填充 */
    char *content;       /* 定义原文(引号内 / 括号内) */
    struct SpecialMarkDef *next;
} SpecialMarkDef;

typedef struct ChordSym {                             /* 和弦行条目 */
    int  col;            /* 起点列 */
    char root; int root_alter;
    char kind[16];       /* 【新增】品质原文:min/maj7/7/dim/-64… */
    bool is_minor, has_seventh, is_maj7, is_dim;      /* 兼容位(由 kind 派生) */
    int  bass_n; int bass[8];                         /* 【新增】_ 后的音序数字(1~8) */
    bool is_none;        /* 【新增】"-" = 无和弦 */
    struct ChordSym *next;
} ChordSym;

typedef struct AuxMark {                              /* 机动行记号 */
    int  col;
    int  kind;   /* SOLO_SHARP/FLAT/NATURAL/OCT_UP_STAR(*)/OCT_UP_APOS(')/OCT_DOWN(.)/
                    PIPE_PARA(|段)/BACK_PARA(\段)/FIXED_ACC(|@[…])/RANGE(>n)/PIPE_BRACKET(|[…]) */
    char *content;       /* 段内容(格序列)或参数 */
    int  subdiv;         /* | 后下划线数(0 = 自动平分) */
    struct AuxMark *next;
} AuxMark;

typedef struct CopyState {                            /* @lN 行复制状态 */
    int  src_no;         /* 复制源 = 文档总行号(1 基) */
    int  from_col;       /* 从该列起生效 */
    bool active;
} CopyState;

typedef struct FixedAcc { int degree[7]; bool set; } FixedAcc;   /* 声部级固定变音状态(跨组携带) */

typedef struct {                                      /* 行头解析结果 */
    char *id, *name_en, *name_zh;
    int  program; InsKind kind; int unpitched_key;
    bool has_head;        /* 是否有行头(无 → 默认钢琴) */
    bool inline_def;      /* 是否谱内就地定义(*Vln|) */
} PartHeader;

typedef struct Event {                                /* 解析后的原子事件 */
    bool  is_rest;
    bool  is_abs; char abs_step;
    int   degree;        /* 1~8;0 = 绝对音/无音高打击 */
    int   alter;         /* -2..2 */
    int   octave_shift;  /* ' * . 累计(±n 组) */
    Fraction start, dur; /* 小节内起点/时值(拍) */
    bool  is_chord;
    char *lyric; bool tie_lyric;
    bool  tuplet; time_modification tm; int tuplet_no, tuplet_pos;
    bool  is_unpitched; char display_step; int display_octave;    /* 【新增】 */
    int   row_no;        /* 【新增】所属数字行序(决定输出 part) */
    bool  from_copy;     /* 【新增】来自 @lN */
    int   event_units;   /* 【新增】声明长度(单位);0 = 由列距推断 */
    char  mark_name[8];  /* 【新增】调用的标记名(供 - 结束/调试) */
    struct Event *next;
} Event;

typedef struct Measure {                              /* 小节 */
    Event *first; int number; TimeState time; KeyState key;
    direction_mxml *dirs;                             /* 【新增】排序键 = col */
    harmony_mxml  *harms;                             /* 【新增】排序键 = col */
} Measure;

typedef struct Part {
    char *id; char *name; Instrument inst;
    Measure **ms; int n;
    int   row_no;                                     /* 【新增】数字行序(1 起) */
    InsKind kind; int unpitched_key;                  /* 【新增】打击信息 */
    FixedAcc fixed;                                   /* 【新增】固定变音状态(跨组) */
    CopyState copy;                                   /* 【新增】@lN 状态(跨组) */
    SpecialMarkDef *local_defs;                       /* 【新增】本乐器作用域定义 */
    bool  is_harmony;                                 /* 【新增】是否和声声部 */
    Event *pending_tie;                               /* 未完成延音线(跨组携带) */
} Part;

typedef struct Score {
    Part **parts; int nparts;
    int divisions; 
    char *work_title, *composer;                      /* 【新增】rule_0_3 #7 */
    SpecialMarkDef *global_defs;                      /* 【新增】全局定义表 */
    ChordSym  *chords_all;                            /* 【新增】全曲和弦行(供 ⑤ 消解与 harmony 输出) */
    int next_inst_no;                                 /* 【新增】就地定义自动编号 */
    char **part_ids_used; int n_ids;                  /* 【新增】ID 独一化登记表 */
} Score;
```

---

## 六、逐文件、逐函数实现方案

### 6.1 tympUtils.c(工具层,纯函数)

**保留(签名不变)**:

| 函数 | 签名 | 职责与算法要点 |
|------|------|----------------|
| `strip_crlf` | `void strip_crlf(char *line)` | 去除行尾 `\r\n`(兼容 Windows 文件) |
| `strip_trailing_spaces` | `void strip_trailing_spaces(char *line)` | 去除行尾空格;.tymp 模板每行有约 10000 个填充空格,必须先去尾再解析 |
| `read_all_lines` | `char **read_all_lines(const char *path, int *n)` | 整文件读入为行数组;行缓冲 ≥ 32KB(容纳 MAX_NUM=10000 填充);失败返回 NULL。**本版:同时返回行号数组(1 基)供 `@lN` 行复制使用** |
| `line_is_timestamp` | `int line_is_timestamp(const char *s)` | 判 `^\s*#timestamp` → 该行跳过 |
| `find_next_nonspace` / `find_prev_nonspace` | `int …(const char*, int from, int limit)` | 乐曲区内找下一个/上一个非空字符列 |
| `parse_time_spec` | `TimeState parse_time_spec(const char *tok)` | `\|6/8` → {6,8};非法 → 沿用上一组并告警 |
| `parse_guitar_tuning` | `GuitarTuning parse_guitar_tuning(const char *tok)` | `&gt: {a=E2,b=A2,c=D3,d=G3,e=B3,f=E3}` → 六弦表 |
| `parse_chord_voicing` | `int parse_chord_voicing(const char *tok, VoicingTable *vt)` | `{D/200232}` → 把位表条目(后定义覆盖) |
| `frac_add/sub/mul/div`、`frac_reduce` | `Fraction …` | 有理数四则与约分 |
| `dur_to_type` | `void dur_to_type(Fraction d, notetype *type, bool *dot, time_modification *tm)` | §3.3 三步判定(标准型 → 附点 → 连音);非整格时值走 time-modification 分支 |
| `step_is_diatonic` | `bool step_is_diatonic(int fifths, char step, int alter)` | 由五度圈数判断 (step,alter) 是否属于当前调号 → 决定 `<accidental>` 是否输出 |
| `split_cross_measure` | `int split_cross_measure(Event *ev, int measure_rest_slots, int slot_tick, note_pitch_mxml **out, int *n_out)` | §7.4:跨小节事件拆 2~n 个 note_pitch_mxml,自动注入 tie 端点(start / start+stop / stop) |
| `key_to_fifths` | `int key_to_fifths(char root, int alter, bool minor)` | C0 D2 E4 F-1 G1 A3 B5 + alter×7;与 `parse_key_spec` 内部复用 |
| `dup_str` | `char *dup_str(const char *s)` | 字符串复制(malloc+memcpy) |
| `utf8_display_width` | `int utf8_display_width(const char *s)` | 显示宽度:CJK 计 2 列、ASCII 计 1(复用 tymp.h 的 UTF-8 解码函数);歌词分格用 |

**修改**:

| 函数 | 签名 | 变化要点 |
|------|------|----------|
| `find_metadata_end` | `int find_metadata_end(const char *line)` | 首版:找第一个 `\|\|` 的列;无则扫描行头已知元数据 token 后推定。**本版**:无 `\|\|` 行的回退链新增——扫过 `/*…*/` 注释、`*l…` 歌词分隔 token、音区 `>n`/`*n`、乐器头 token,终点 = 最后一个行头 token 末尾(P14) |
| `scan_meta_mark` | `int scan_meta_mark(const char *line, int from, int to, char *buf, int *col)` | 首版:从 `\|` 起读一个完整元数据记号(`\|=G`、`\|vb120`、`\|-7-3-6`、`&gt: {…}`、`{D/200232}`),返回文本与列位,组头/组末/机动行共用。**本版**:记号集新增 `\|@[…]`、`\|rit.`/`\|vb.`(句点并入 token,不误判为八度记号)、浮点 `\|vb120.000000`、`\|s1`~`\|s8`、`\|si[+-]`/`\|sr\|pr\|sb\|pb\|se`、`\|。`/`\|vt` |
| `parse_key_spec` | `KeyState parse_key_spec(const char *tok, const KeyState *prev)` | 首版:`\|=C`/`\|=#G`/`\|=Dm`/`\|=#G~` → root/alter/minor/chord_no_follow;`\|-7-3-6` → 尝试等价标准调号(降 7/3/6 度 = bB,bE,bA = Eb 大调 fifths=-3),不等价时记「逐音变音表」并 fifths=0。**本版**:双语法——`\|=C`/`\|=#G~` 与 `\|@[7-3-6-]`/`\|@[0]`(R3);旧 `\|-7-3-6` 兼容;产出 `fixed_alter[]`;等价标准调号判定不变 |
| `parse_tempo_mark` | `int parse_tempo_mark(const char *tok, int col, DirectionMark *out)` | 首版:识别 `\|vb<num>`、`\|vi[+-]`、`\|rit`/`\|rall`/`\|accel`、`\|ve`、`\|vr`、`\|vb`、`\|。`、`\|vt`,只校验不产出。**本版**:**改为产出** DirectionMark —— `\|vbN` → DIR_TEMPO;`\|vi±`/`\|rit`/`\|rall`/`\|accel` → DIR_WORDS("rit."/"accel.");`\|ve` → 仅结束状态;`\|vr`/`\|vb`/`\|vb.` → DIR_WORDS("A tempo");`\|。`/`\|vt` → DIR_FERMATA |
| `parse_dynamic_mark` | `int parse_dynamic_mark(const char *tok, int col, DirectionMark *out)` | 首版:识别 `\|ppp`…`\|fff`、`\|s1`~`\|s8`、`\|si[+-]`、`\|cresc`/`\|dim`、`<  <`/`>  >`(机动行内)、`\|sr`/`\|pr`/`\|sb`/`\|pb`(同义)、`\|se`,只校验不产出。**本版**:**改为产出** DIR_DYNAMIC / DIR_WEDGE(`<   <`/`>   >`/`\|si±`/`\|cresc`/`\|dim`/`\|sr\|pr\|sb\|pb` → wedge start;`\|se` → wedge stop) |
| `resolve_degree_pitch` | `pitch resolve_degree_pitch(pitch anchor, const KeyState *k, int degree, int alter, int shift)` | 首版:简谱音级→音名+八度,核心八度规则见 §7.5,`shift` 为 `*`/`.` 累计组偏移。**本版**:叠加 `fixed_alter`(固定变音 → 调内音 → 后缀变音);`shift` 接受 `*` 与 `'`(同义,升八度组,作用于其前音)累计 |
| `degree_to_step` | `void degree_to_step(const KeyState *k, int degree, int alter, char *step, int *alter_out)` | 首版:1→调根音名,按大/小调音阶走 2 全 3 半…,再叠加 alter,返回最终 step/alter。**本版**:先叠 `fixed_alter[degree]`,再叠调内音阶与参数 alter |
| `compute_divisions` | `int compute_divisions(Event *all, int n_events)` | 首版:§3.2 收集全部事件时值/连音分母 → L → 4·accu·L。**本版**:**公式修正**——收集全部事件的"占列数 ÷ 音数"约分后的分母 q(连音、`[…]` 等分、管道段细分),返回 `4 × lcm(q)`,最小 4(§3.2) |

**新增**:

| 函数 | 签名 | 职责与算法要点 |
|------|------|----------------|
| `parse_fixed_accidental` | `int parse_fixed_accidental(const char *tok, FixedAcc *out)` | 解析 `@[7-3-6-]`→{7:-1,3:-1,6:-1};`@[0]`→清空;支持 `+`/`-`/`～`;非法告警 |
| `map_dynamic` | `int map_dynamic(int s_level, mxml_dynamic *out)` | `\|s1`~`\|s10` → pppp…ffff(§4.5 表,rule.md 附录 B-6 匡正);越界告警 |
| `parse_chord_bass` | `int parse_chord_bass(const char *tok, int *out, int *n)` | 解析 `_` 后的音序数字串(如 `_46`→{4,6});**v0.3.1+2**:按 §7.7 根音位置标注消解(非音级序列):`_3`/`_5` = 原位三和弦、`_7` = 原位七和弦、`_6`/`_56` = 三音为低音、`_46`/`_34` = 五音为低音、`_2` = 七音为低音(实装为 `chord_bass_resolve`) |
| `parse_inline_instrument` | `int parse_inline_instrument(const char *s, PartHeader *h)` | 解析 `*#N:ID<Name$P`(各部分可省略);与 music 区 `*Name\|` 形态(前瞻匹配 base_id);缺省补全:编号自动、program 查原型表、查无 → 钢琴 |
| `classify_line` | `LineRole classify_line(const char *line, int lineno, const Ctx *ctx)` | **§7.2.2 全套分类规则**(按内容判角色,仅节拍行按表头)的单行实现;产出角色 + 行头元数据 + `AuxMark` 列表 |
| `parse_lyric_separator` | `int parse_lyric_separator(const char *head, char **lyric, int *dir)` | 解析 `*l""`/`*l\|""`(dir=上)/`*l\""`(dir=下);未闭合引号取至**其后第一个空格** + 告警(rule.md 附录 B-11 匡正);无 `*l` → 返回"旧式裸按列"标志 |
| `parse_pipe_instance` | `int parse_pipe_instance(const char *s, int col, AuxMark *m)` | `\|` 或 `\` + [若干 `_`,subdiv] + `{…}` 柱式格 / `[…]` 等分格,格间 `_` 分隔;每格内容暂存(格内再按 `scan_digit_line` 语义解析) |
| `resolve_line_copy` | `int resolve_line_copy(char **lines, int nlines, const CopyState *cs, const Ctx *ctx, Event **out)` | 复制文档第 `src_no` 行自 `from_col` 起的列对齐内容,按**本声部**音区锚点重新消解为事件;越界 → 告警忽略;循环引用(源行经 `@lN` 链回到本声部)→ 检出、截断 + 告警(§7.9) |
| `cols_to_frac` | `Fraction cols_to_frac(int cols)` | 列数 → 拍数有理数(cols/4);单位数 → cols 的换算同理 |
| `make_tymp_copy` | `int make_tymp_copy(const char *src, const char *dst)` | **v0.3.1 #6 补全副本**:展开特殊表记与 `@lN`/`@lN^M` 简化写作,写出扩展名为 `.tymp` 的副本;调整行统一放于目标行上方(该行该处为空);否则先在其上方**生成一个新空行**存放调整行;`@lN^M` 逐列按 §7.9 取音规则展开为平行和声行;`@lN` 复制时**不复制歌词**(#1);**插入新空行后对副本中所有行数引用(`@lN`/`@lN^M`)补正**(rule.md 附录 B-3 匡正);**人声段括号内容转移至伴奏声部**(B-8)。副本生成后,后续全部解析针对副本 |
| `parse_voicing_g` | `int parse_voicing_g(const char *tok, SpecialMarkDef *def)` | 解析 ⑥ 类 `X&g"xxxxxx"`(v0.3.1 #8):x∈1..6 弦号(1 为低弦);展开时经把位数据库(§2.5)转换为音位;把位未收录 → 告警 |

### 6.2 tymp2musicXML.c(主逻辑)—— 输出层

**已有(保留):**

| 函数 | 签名 | 说明 |
|------|------|------|
| `initNewMusicXML` | `FILE *initNewMusicXML(char *filename)` | 已有实现:补 `.musicxml` 后缀、写 `xmlHead`;**保持原样** |
| `write_pitch` | `int write_pitch(FILE *fp, pitch p)` | 首版保留:`<pitch><step>..</step>[<alter>n</alter>]<octave>..</octave></pitch>`;alter=0 省略(§4.4) |

**修改**(首版描述 + 本版变化):

| 函数 | 签名 | 职责与算法要点 |
|------|------|----------------|
| `makeInstrumentPart` | `int makeInstrumentPart(FILE *fp, const Instrument *inst, int channel)` | 首版:写一个 `<score-part>` 块(§4.2 各要素);按 `inst.prototype.kind` 分支——INS_PITCHED → midi-program;INS_UNPITCHED/INS_DRUMKIT → 返回 0 并置「跳过」标志。**本版**:**三分支全部输出**(§4.2):INS_PITCHED → midi-program;INS_UNPITCHED → `midi-unpitched`(**不再跳过**);INS_DRUMKIT → program=1 + 鼓组多 score-instrument;id 经独一化清洗 |
| `getInstrumentList` | `int getInstrumentList(char **lines, int n, const char *instr_line, Instrument *out, int max, int *count)` | 首版:解析 `instruments:NGTR1 SD1`——按空白分词;对每个结构 id **剥离末尾数字**得 base_id 在 `InstrumentPrototypeList` 中精确匹配;构造 Instrument(score_part_id=原 id、instrument_id=id+"-I1"、名称取原型表);匹配失败回退 INST 原型。**本版**:输入不再只有 instruments 行——累计各组行头(六形态,§7.2.5)、`*#N:` 简写、`*Vln\|` 就地定义、默认钢琴;**完成 ID 独一化**(§7.11) |
| `write_part_list` | `int write_part_list(FILE *fp, const Score *s)` | 首版:按 P0(旋律)/P1..(和声)/伴奏顺序输出 `<part-list>`;跳过标志的乐器不输出。**本版**:顺序 = 旋律 `P0` → 和声 `P1`.. → 伴奏各 row(同乐器 row≥2 加 `-2`/`-3` 后缀);打击分支(§4.2);新增 `<work>`/`<identification>` 在 part-list **之前**(§4.1) |
| `write_note` | `int write_note(FILE *fp, const note_pitch_mxml *n, const KeyState *key)` | 首版:§4.4 固定顺序输出一个 `<note>`——chord 在 pitch 前;rest 时跳过 pitch/accidental/lyric;`tie[0..1]` 逐一输出 `<tie>`;accidental 仅非调内时输出;notations 中 tied 与 tuplet 按 tymp.h 字段逐项输出。**本版**:顺序修正(§4.4)——`<chord>` → pitch/unpitched/rest → duration → tie → **`<instrument>`** → voice → type → dot → accidental → **time-modification** → notations → lyric;`is_unpitched` 分支走 `write_unpitched`;fermata 记入 notations |
| `write_attributes` | `int write_attributes(FILE *fp, int divisions, const KeyState *k, const TimeState *t, InsKind kind)` | 首版:`<attributes>` 内含 divisions/key(fifths+mode)/time/clef(G/2)。**本版**:新增 `kind` 参数——打击/鼓组 → `<clef><sign>percussion</sign><line>2</line></clef>` |
| `write_measure` | `int write_measure(FILE *fp, const Measure *m, int no, const KeyState *prev_k, const TimeState *prev_t, int divisions)` | 首版:`<measure number>`;若 no==1 或 key/time 有变 → write_attributes;依次 write_note;`</measure>`。**本版**:按 col 归并输出 direction / harmony / note(§3.5) |
| `write_part` | `int write_part(FILE *fp, const Part *p, int divisions)` | 首版保留:`<part id>` + 逐小节 write_measure(携带上一小节 key/time 状态) |
| `write_score` | `int write_score(FILE *fp, const Score *s)` | 首版:part-list + 全部 part + 结束标签。**本版**:work/identification → part-list → 全部 part → 结束标签(§4.1) |

**新增**:

| 函数 | 签名 | 职责 |
|------|------|------|
| `write_unpitched` | `int write_unpitched(FILE *fp, char step, int octave)` | `<unpitched><display-step>F</display-step><display-octave>4</display-octave></unpitched>` |
| `write_direction` | `int write_direction(FILE *fp, const direction_mxml *d)` | §4.5 全表:metronome+sound / dynamics / words / wedge;`placement="below"` |
| `write_harmony` | `int write_harmony(FILE *fp, const harmony_mxml *h)` | `<harmony><root><root-step>…` + `<kind>` + 可选 `<bass>` |
| `write_work_identification` | `int write_work_identification(FILE *fp, const char *title, const char *author)` | `<work><work-title>…</work-title></work>` + `<identification><creator type="composer">…` |
| `write_fermata` | `int write_fermata(FILE *fp)` | 供 `write_note` 在 notations 内输出 `<fermata/>` |

### 6.3 tymp2musicXML.c(主逻辑)—— 解析层

**修改**:

| 函数 | 签名 | 职责与算法要点(首版 + 本版变化) |
|------|------|--------------------------------|
| `parse_header` | `int parse_header(char **lines, int n, Score *s, char *instr_line)` | 首版:第 1 行按 `" by "` 切标题/作者(找不到分隔符则整行为标题);第 2 行取 `instruments:` 后内容;返回乐器行号。**本版**:前缀保留(如 `4-1-`);写入 `Score.work_title/composer`(rule_0_3 #7)。**v0.3.1**:多作品文档(不同「标题 by 作者」)按标题行切分,记录各行所属作品序号(§7.0、main) |
| `parse_group_head` | `int parse_group_head(const char *line, GroupState *g, Measure *m0)` | 首版:对 `{` 后的元数据逐个 `scan_meta_mark`:拍号/调号/速度/力度/`&gt:`/把位定义;`{` 后无内容 → 沿用上一组(§2.1)。**本版**:调号双语法(R3);速度/力度 → **产出** DirectionMark;**全行扫描定义**(R9/P13) |
| `parse_group_tail` | `int parse_group_tail(const char *line, GroupState *g, Measure *last)` | 首版:`}` 之后的元数据:力度/速度(全局生效类)——解析校验,不产出。**本版**:力度/速度 → **产出** DirectionMark(全局),输出到该组最后一小节 |
| `parse_line_head` | `int parse_line_head(const char *line, LineInfo *li, SpecialMarkDef **defs)` | 首版:行头元数据——乐器名(按 `<` 切中/英文两段)、`*4` 音区、初始力度、①~⑤ 定义(`name~{…}`/`name&"…"`/`name&r"…"` 三种形态,含标记名合法性校验:1~4 字母、不含 ABCDEFG、首尾非数字、不重复)。**本版**:定义形态增至五种(新增 `name&{…}…` P7、`name(V)&[r]"…"` P6 参数化,参数化与同名普通可共存);音区 `>`/`*` 双轨;未闭合引号容错(P21);定义域:组头/组尾 → 全局,乐器行头 → 本乐器 |
| `resolve_chords_line` | `int resolve_chords_line(const char *music_area, ChordSym **out)` | 首版:每个和弦符号(根音字母+可选 #b+品质 m/min/maj7/7/dim…)记其列;一个符号从列生效至下一个符号。**本版**:分词解析(P12 全集):`-` → is_none;`#Fmin`/`bAmaj_46`;品质 `min/maj7/7/dim/-64…`;`_` 低音多位(`_3`/`_5` = 原位三和弦、`_7` = 原位七和弦,v0.3.1 #3);**`maj` → 增三和弦**(#11);紧排多和弦(每 token 一个 ChordSym);产出 `harmony_mxml`。**v0.3.1+2**:方法 1 转调(和弦字母 = 调内级数名,`~` 不跟随)、低音按根音位置标注消解(§7.7)、多作品按下标区间隔离、`\|`/`\` 记号过滤为方向 |
| `scan_digit_line` | `int scan_digit_line(const char *music_area, const LineInfo *li, const GroupState *g, Event **out)` | 首版:**主扫描器**(§7.1):逐列识别 数字/0/`(n)`/`[~..123]`/`{..}`/`%..`/特殊表记名/前后缀标记(`* . + - ～`)与 `_`(等价空格);生成 Event 链(时值=下一事件列−起点列,见 §7.2);`%` 类与 `[…]` 类按「声明格数」定长(与书写宽度无关)。**本版**:新增识别 `'`(升八度组后缀,作用其前音)、谱内 `>n` 音区前缀(**须与渐强 `>   >` 区分**:`>` 后紧跟数字才算)、`=`+标记(`====X`/`=T(4)`,等号个数 = 单位数)、`-` 结束记号、`@lN` 行复制、`(n.)`/`(n+)`/`(n-)` 变体;`**`、孤立 `"…"`、错位孤立 `+` → 跳过 + 告警(P15/P16);标记调用时值 = 定义声明单位数(不填到下一事件,§7.1);**标记调用带根音位置 `H_6`**(v0.3.1 #3,只替换本行消解和弦);`@lN^M` 记号(#2)交由补全副本阶段展开(§7.0) |
| `scan_aux_line` | `int scan_aux_line(const char *music_area, const LineInfo *li, AuxMark **out)` | 首版:机动行扫描——同一列上方的 `.`/`+`/`-`/`～`(作用于数字行同列音符);`\|` 起始的多符号段(按列对齐);`\|_…_` 细分格段;`=` 对应的 `\|段` 替换。**本版**:重写为**段分词**——`\|` 起始段、`\` 起始段(至下一 `\`/`\|`/行尾)、`\ … \` 单独符号段、`\|@[…]`/`\@[…]`、`\|>n\|`/`\>n\|`、`\|{…}_{…}`、`\|[{…}…]`、`\|__…` 细分、单独符号(含 `'`);产出 AuxMark 链(带 `dir` 方向) |
| `apply_aux_marks` | `int apply_aux_marks(Event **evs, AuxMark *marks, const Ctx *ctx)` | 首版:把机动行标记合并进数字行事件——同列后缀 → 修改对应事件的 alter/octave_shift;`\|段` → 替换 `=` 事件(段内再跑一次 scan_digit_line,时值按段长);`\|` 力度/速度段 → 校验丢弃。**本版**:四类合并——①单独符号 → 按方向绑定(上指 → 下方数字行;**下指 → 上方数字行**)同列/近列音符;②`\|`/`\` 段 → `=` 配对替换(时值 = 等号段列数,每 `=` 1 列;格内按 `subdiv` 或自动平分;配对目标 = 段首列同列的等号段,rule.md 附录 B-1 匡正)或独立注入;③`@[…]` → 在列处更新声部 `FixedAcc`(小节起点 → 改 KeyState 并重出 attributes;小节内 → 逐音变音 + 告警);④`>n` → 重置音区锚点。力度/速度段不再丢弃(→ DirectionMark) |
| `expand_special_mark` | `int expand_special_mark(const SpecialMarkDef *def, const ChordSym *chord, const Ctx *ctx, pitch anchor, Event **out)` | 首版:§8 按 ①~⑤ 展开为 Event 链。**本版**:+ **参数化文本代入**(R6)+ 无引号形态;格解析:`_` 分格、`/` 分拍、`{…}` 柱式、`(7/8)` 条件七音、`@[6-7-3-]` 标记变音、`'`/`.`/`+`/`-`/`～` 格内前后缀、`8`/`0`/首导根音 `1_`(P8);**自动重复(R7)**:标记声明 > 1 单位 → 循环填充至下一事件;= 1 单位 → 一次性;r 类在**和弦更换**处循环重头。**v0.3.1+3**:定义内 `_` 为**空格单元**(本身占 1 列、前一音时值延长一单位,`1_{35}_{35}_` = 6 单元、音符在第 1/3/5 单元);**仅定义适用**——管道/`\|[…]` 括号内容的下划线仍为分隔符 |
| `attach_lyrics` | `int attach_lyrics(Event *evs, const char *lyric_area, int accu, int dir)` | 首版:歌词行按拍分格(每拍 accu 显示列,汉字 2 列);每格首非空字符附着到该拍第一个事件;`-` → 置 tie_lyric(不生成歌词文本);休止/连音续片段不附着。**本版**:先 `parse_lyric_separator` 取引号文本(**原样**,含标点空格);无 `*l` 走旧式按列;**逐字句一一对应**(v0.3.1 #5,不依赖列;拍内无歌词视同上文,§7.2.4);引号外记号按力度/机动处理 |
| `events_to_measures` | `int events_to_measures(Event *flat, const Ctx *g, Measure **out, int *n)` | 首版:按小节宽(拍数×accu)切分;行内无下一事件时音符持至本小节末;跨小节音符拆入两小节并标记 tie;每小节结尾按剩余格补休止(整小节 → measure="yes")。**本版**:事件含声明单位数;小节宽 = 16×分子÷分母 列;`@lN` 合成事件 `from_copy = true`;direction/harmony 按列 → 小节 + 排序键 |
| `convert_line` | `int convert_line(const LineRole role, char **lines, const Ctx *g, Part *p)` | 首版:一行(声部)的完整转换——行头 → 数字行/机动行/歌词行识别 → 扫描 → 事件 → 小节;跨组时携带 Part 的「未完成音符(延音线待续)」状态。**本版**:一行(含其机动行合并)→ 一个 part 的小节事件;`row_no` 写入 Part;跨组携带 `FixedAcc` / `CopyState` / 未完成延音线 / 音区锚点 / 行头乐器 |
| `convert_group` | `int convert_group(char **lines, int from, int to, GroupState *prev, Score *s)` | 首版:一组(乐段)的调度——识别各行角色(组头/和弦/数字/机动/歌词/`[`/`(`/`}`/时间戳),依次调用上述函数;伴奏行按 instruments 行顺序挂接。**本版**:**全组重写**——`classify_line` 逐行 → 组头/和弦/歌词/和声/伴奏/时间戳分支 → 机动行**按方向绑定**到目标数字行 → 各声部按行组装(每数字行一个 Part) → 组尾;机动行 > 2 → 告警 |
| `ev2mxml` | `int ev2mxml(const Event *e, const KeyState *k, int col_tick, note_pitch_mxml *out)` | 首版:事件 → tymp.h 结构——时值→duration/type/isDot/tm;音级+变音+八度→pitch;tie_lyric/跨小节→tie[];连音→notations.tuplet(attribute2=组号字符串);歌词→lyric.syllabic=SYL_SINGLE、text。**本版**:填充新字段 time_modification(新补)、is_unpitched/display_step/display_octave、instrument_ref(鼓组) |
| `tymp2musicxml` | `int tymp2musicxml(const char *src, const char *dst)` | 首版:总管线——read_all_lines → parse_header → 逐组 convert_group → compute_divisions → 逐声部 ev2mxml+split_cross_measure 填 note_pitch_mxml → write_score。**本版**:管线(§7.0)——read_all_lines(留行号)→ parse_header → **第零遍**补全副本(make_tymp_copy,展开特殊表记/`@lN`/`@lN^M`,v0.3.1 #6)→ 重新读入副本 → **第一遍**全曲行分类 + 定义收集(全局/本乐器)+ 和弦行收集 → **第二遍**逐组 convert_group(含 `@lN` 解析与循环防护)→ compute_divisions → ID 独一化 → 输出层;**多作品**(#10)按「标题 by 作者」分组,每组各输出一个 .musicxml |
| `main` | `int main(int argc, char **argv)` | 用法不变:`tymp2musicxml 输入.tymp [输出.musicxml]`(缺省输出=输入名换后缀);返回 0/非 0。**v0.3.1**:多作品文档(多个「标题 by 作者」)自动生成**多个**输出文件(标题行分组,每组换后缀 `-1`/`-2`…;单作品行为不变) |

**无删除函数**。

---

## 七、核心算法细则

### 7.0 总管线(两遍)

```
read_all_lines(保留行号,原文)
  → parse_header(标题/作者/instruments 行;多作品按标题行分组,v0.3.1 #10)
  ── 第零遍(补全副本,v0.3.1 #6) ──────────────
   make_tymp_copy: 展开特殊表记与 @lN / @lN^M 简化写作 → 写出 .tymp 副本
       调整行统一放于目标行上方(该处为空);否则先在其上方生成新空行
       @lN^M 逐列按 §7.9 取音规则展开为平行和声行;@lN 不复制歌词(#1)
  → read_all_lines(副本,保留行号)── 以下全部针对副本 ──
  ── 第一遍(全曲) ──────────────────────────────
   对每组每行: classify_line → 行角色与行头
       收集: 全局定义(组头/组尾)、本乐器定义(乐器行头)、和弦行、时间戳网格(定 accu)
  ── 第二遍(逐组) ──────────────────────────────
   convert_group: 机动行按方向绑定 → 各数字行 scan_digit_line
                → apply_aux_marks → expand_special_mark → resolve_line_copy
                → attach_lyrics → events_to_measures
  ── 收尾 ─────────────────────────────────────
   compute_divisions(全曲) → ID 独一化 + part-list 构造
   → 逐 part: ev2mxml + split_cross_measure → write_score
   (多作品:对每个「标题 by 作者」分组重复上述过程,各写一个 .musicxml)
```

### 7.1 数字行主扫描器(状态机)

按列扫描乐曲区,状态:`空闲 / 音符持时中 / 连音组中 / 标记名匹配中 / 复制状态(@lN)`。

**事件起始字符**

| 起始字符 | 事件 | 声明长度 |
|---|---|---|
| `1`~`8` | 单音 | 由列距决定(§下) |
| `0` | 休止/停止 | 同上 |
| `(` | 括号单音 `(1+)`/`(1)`/`(5.)`/`(7)` | 一拍(不补空格) |
| `[` | `[123]` 等分 / `[~123]` 多拍等分 | 1 拍(16/分母 列)/ 每多一个 `~` 多 1 拍 |
| `{` | 柱式和弦(同刻 n 音) | 书写宽度 |
| `%` | `%234` / `%%234` / `%12345` / `%1234543` | 4 列 / 8 列 / 8 列 / 8 列 |
| 标记名首字母 | 特殊表记调用(可带参数 `X(3)`) | 定义声明的单位数(`total_units`) |
| `=` | `=` 段(连续 `=`;段后**可跟**标记名,与该段无关) | 段时值 = 等号个数 × 1 列;替换内容来自段首列同列机动段(§8 P9,rule.md 附录 B-1 匡正) |
| `-` | 显式结束记号(P16) | 0(不占时值,仅改状态) |
| `@l` + 数字 | 行复制记号(R10) | 0(仅改状态) |
| `>` + 数字 | 谱内音区前缀(R11) | 0(仅改锚点) |
| `.`(前缀) | 其后数字降八度组 | 并入其后事件 |
| `'` `*` `+` `-` `～`(后缀) | 归属**前一事件**,不产生新事件 | 计入前一事件时值 |
| `_` | 等价空格(分隔符) | — |

**时值规则(核心)**

1. **一般事件**:时值 = 起点列 → 下一事件起点列的**列距**(列 → 拍:`列数/4`)。
2. **声明长度类**(`%`、`[…]`、`{…}`、标记调用、`=段`):
   - 列距 = 声明长度 → 直接使用;
   - 列距 > 声明长度 → 内部按声明长度铺排,余下列距记为**休止**(等价自动 `0`)并告警;
   - 列距 < 声明长度 → 按列距截断并告警。
3. **行内无下一事件** → 持续到该行覆盖的最后一小节末;若其后(同组后续小节/下一组)仍无事件,则跨小节线/组边界延续,直到遇到事件或 `0` —— 这是跨小节长音符的来源;**中途终止必须写 `0`**。
4. 每组覆盖的小节数 = 组内最长乐曲行长度 ÷ 每小节列数,向上取整。
5. 后缀标记列计入其所属事件;前缀 `.`/`>n` 列计入其后事件。
6. 事件起点列 + 声明长度 不得超过该行的小节边界(违反告警并截断)。
7. **`>` 的三义判别**:`>   >`(大于号后接 ≥2 空格再接 `>`,且中间无数字)→ 力度渐强;`>` 紧跟 1 位数字且处于乐曲区 → 音区前缀;`&gt:`(字面四字符 + 冒号)→ 调弦表(仅在元数据区)。

**标记调用与 `-` 结束**

- 标记调用(无 `=` 前缀)的时值 = 定义的声明单位数,**不**自动填到下一事件(否则 part-IV 行 17 末尾的 `X` 会无限重复)。
- 若下一事件的距离大于声明单位数,按 1-2 的规则产生休止并告警。
- **`-`** 终止当前的"持续节奏型"状态(标记自动重复 / `@lN` 复制);`-H` 拆为 `-` + `H` 两个记号。

### 7.2 行角色动态分类与机动行绑定

#### 7.2.1 预处理

对组内每行:`strip_trailing_spaces` → 跳过行首空格 → 求元数据区终点第 1 个 `||`(无 `||` 的行按 §6.1 `find_metadata_end` 回退链推定)→ 得到 head 区与 music 区。

#### 7.2.2 行角色判定(按内容;仅节拍行按表头)

行角色**不再通过表头判定**——唯一例外是**节拍行**(`#timestamp:`,无谱面内容,只能以行头识别)。行头仅提供元数据(乐器名/音区/定义),不决定角色;组头/组尾以 `{`/`}` 结构定界。

| 判定条件 | 角色 | 处理 |
|---|---|---|
| 行头含 `#timestamp:` | 节拍行(**唯一按表头识别**) | 跳过;由网格间距 ÷ 拍号分子推断 accu(4/4→4、6/8→2);无网格则按公式并告警 |
| 行首为 `{` / `}` | 组头行 / 组尾行(**结构定界**) | `scan_meta_mark` 逐个解析;**整行**扫特殊表记定义(R9/P13);`}` 后为组末元数据 → DirectionMark(全局) |
| music 区内容为**和弦符号序列**(根音字母+品质+`_n`/`-`,不含简谱事件) | 和弦行 | `resolve_chords_line`(P12 全集) |
| 含 `*l…"` 歌词分隔符,或位于数字行正下方且内容非事件记号 | 歌词行 | `parse_lyric_separator`(§7.2.4) |
| 含事件字符(数字/`(n)`/`[…]`/`{…}`/`%…`/标记名/`=`/`@lN`) | 数字行 | 主扫描器(§7.1);**声部/乐器归属取本行行头元数据**(伴奏行每行均有行头,§7.2.5;无行头 → 伴奏区默认钢琴、旋律区为旋律声部;`[` 行头仅作和声声部元数据,不参与角色判定) |
| 仅含单独符号与 `\|`/`\` 段 | 机动行 | 按方向绑定(§7.2.3) |
| 仅含笔误段(`**`、孤立 `"…"`) | 空行 | 跳过 + 告警(P15/P16) |

#### 7.2.3 内容判定(核心;**方向语义已定案**)

把 music 区分词为 5 类段:

| 段 | 形态 | 归属 |
|---|---|---|
| ① | `\|` 起始段 | **上方机动内容 → 绑定下方紧邻数字行** |
| ② | `\` 起始段(至下一 `\`/`\|`/行尾) | **下方机动内容 → 绑定上方紧邻数字行** |
| ③ | 单独符号(`.` `*` `+` `-` `～` `'`) | 同所在行方向(在上 → 绑下方;在下 → 绑上方) |
| ④ | 数字事件串(§7.1 起始字符) | **属本行所在声部**;该行同时是数字行(多行声部第 n 行) |
| ⑤ | 笔误段(`**`、孤立 `"…"`) | 跳过 + 告警(P15/P16) |

**绑定算法**:

1. 逐行确定其"归属声部"(最近一个已定义的乐器 / 主旋律 / 最近 `[` 声部);
2. 对每个 ①/②/③ 段:在**方向所指的行**上寻找目标数字行 —— 上指找下方最近的可作数字行的行,下指找上方最近的;
3. 目标数字行不存在 → 告警丢弃该段;
4. 同一数字行同时有上、下两个机动行是合法的(rule_0_3 #4 的"最多两个");出现第 3 个 → 告警并忽略;
5. **两个数字行紧邻**时:中间行只保留"单独符号"与 `|` 段作为机动内容,其余数字归**其所在声部**(R1)。

**实测校验点**:行 16 `|{42}_{31}_` → 行 17 `====X`(上指 ✔);行 19 单独符号 → 行 20(上指 ✔);行 21 `\@[3-6-7-]`、`\ ' .' . \` → 其**上方**数字行(下指 ✔)。

#### 7.2.4 歌词行

- `parse_lyric_separator` 取引号内文本(**原样**,含标点/空格/符号);`*l""`/`*l|""` → 歌词属**上方**数字行;`*l\""` → 属**下方**数字行。
- **歌词与谱数字不必同列、但须同一行**(v0.3.1 #5):按「**逐字句完成了对应**」匹配——将引号内字句**依次一一对应**到事件流(一个事件一个音节/字句);同一拍内多个字句依次附着该拍各事件,不足则延续到后续事件;汉字仍按显示宽度 2 列计(仅影响引号内文本的切分)。裸按列旧式仍按列对应。
- **拍内无歌词 → 视同上文**(继承上一拍的歌词,即延音;不产生新 `<lyric>`)。
- `-` → `tie_lyric`(不产生文本);引号**外**的记号按力度/机动记号处理。
- 无 `*l` 的行 → **旧式裸按列歌词**(output/4 形态):歌词文字直接按列对齐数字行。
- 休止音符不附着歌词;未闭合引号取至**其后第一个空格** + 告警(P21,rule.md 附录 B-11 匡正)。

#### 7.2.5 伴奏行头六形态解析

1. `(Accomponiment#N:ID<EN; Initial pitch range: >4` → 提取 N、id、中英文名、音区;
2. 省略 `Initial pitch range:` 的对齐式 → 同上(靠 `;` 与 `>n` 定位);
3. `(-ID-` 短行头 → 沿用上一组同 id 的完整信息;
4. `(-ID-后缀` + **下一行 head 仅含名称型 token**(如 `Violin`) → 双行行头合并,P11;
5. `*#N:ID<Name$P` 简洁定义 → `#N` 省略则自动编号;`<Name` 省略则查原型表;`$P` 省略则先按 `ID` 查 `InstrumentPrototypeList`,查得则用其 `gm_program`,**查无 → 钢琴**(program=1);
6. 谱内就地 `*Vln|` → music 区以 `*` + 原型 base_id + `|` 前瞻匹配;自动编号与 id;
7. 无任何行头 → **默认钢琴**(rule_0_3 #4);
8. 乐器名中英文以 `<` 分割;主旋律与 `[` 行可无乐器名。

#### 7.2.6 多行声部 → 输出组装

- 每个**数字行** → 一个 `Part`(D6);`Part.row_no` 记录行序(1 起);
- 同乐器的第 2、3 行 → part id 加 `-2`/`-3` 后缀,part-name 相同,midi-channel 独立;
- 机动行**不产出** Part(其内容已合并进目标数字行);
- 每个 Part 的小节数 = 全曲小节数(短行按 §7.8 补休止)。

### 7.3 和弦行 → ChordSym / harmony

- 分词:每个 token = 一个和弦;`-` = 无和弦(is_none);
- 根音 `A`~`G` + 可选 `#`/`b`(前缀,如 `#Fmin`/`bAmaj_46`);
- 品质:从 token 剩余部分取(`m`/`min`/`min7`/`maj7`/`7`/`dim`/`-64`/`-6`…);同时派生兼容位 `is_minor`/`has_seventh`/`is_maj7`/`is_dim` 供 ⑤ 类消解;**`maj7`/`dim` 中的 `m` 不算小三和弦标志**(v0.3.1+2,品质解析修正);
- `_` 后的数字串 → `bass[]`(如 `_46` → {4,6});
- 一个符号从其列生效至下一个符号;
- **转调(方法 1,v0.3.1+2)**:`|=K`(视同简谱)时和弦行跟随谱面一道移调——和弦字母即**调内级数名**(C = 1 级),按目标调号经 `degree_to_step_oct` 逐级映射(大调/小调音阶各自);`|=C` 恒等;后缀 `~`(`chord_follow=0`)时不跟随。根音与低音位置在**解析时**按当时的调号固化(调号中途改变不影响已解析和弦);
- 低音按 §7.7 根音位置标注消解(解析时固化);
- 输出:`harmony_mxml`(每 token 一条),按 `col` 排序;`is_none` 不输出;同一小节多个和弦按 §3.5 用 `<offset>` 码放。
- **多作品隔离(v0.3.1+2)**:和弦表全局累积,但按「标题 by 作者」分组记录各作品的**下标区间**,输出时各作品只取自己的区间。
- **`maj` = 增三和弦**(v0.3.1 #11):根音上方大三度 + 增五度;`maj7` 保持大三七和弦。→ `<kind>augmented</kind>`。
- **`H_6` 替换**(v0.3.1 #3):H 须为**第一行**中按和弦节奏型(⑤ 类)声明的标记;`H_6` 处用「此处活动和弦 + 6 所代表的根音位置」替换**本行**解析所得和弦,不影响其他行(如活动 `G_46` 处写 `H_6` → 本行按 `G_6` 消解);`H_3`/`H_5`/`H_7` 同理。H 未在第一行声明 → 告警(§13-29)。
- **和弦行中的 `\|`/`\` 记号**(v0.3.1+2):按方向记号处理(`record_direction`),不再产出伪和弦。

### 7.4 跨小节拆分与延音线

对每个跨小节事件:

1. 事件流层面按小节线切分:片段 i 的时值 = 到小节线为止的剩余列数;
2. 拆分出的 2~n 个片段共享同一音高;
3. 注入延音线端点:首片段 `tie[0]=TIE_START`;中间整小节片段 `tie[0]=TIE_START, tie[1]=TIE_END`;末片段 `tie[1]=TIE_END`;
4. 每个端点双写 `<tie>` 与 `<notations><tied>`(musicxmlEg.xml 风格);
5. **歌词 `-` 延音线**:歌词 `-` 附着于事件 X → X 加 `tie[0]=TIE_START`,其后第一个有音高事件加 `tie[1]=TIE_END`;两端音高不同 → 告警并丢弃该延音线;
6. 跨小节休止同样拆分,但**不**产生 tie;
7. 拆分后每片段 tick 须为整数(由 §3.2 构造保证);
8. 延音线状态作为声部上下文**跨组携带**(part-IV 行 29/30 的 `@l7`/`@l9` 复制整行时尤其重要)。

### 7.5 简谱八度消解

- **行/组初始锚点**:行头 `>n` 或 `*n` → 该行调根音 1 = MIDI 八度 n(C 大调 `>4` → 1 = C4 中央 C);`[` 行无音区 → 继承主旋律;`#` 开头的和声行 → 锚点 = 主旋律**首音实际音高**(待确认项,§13)。
- **逐音推进**:当前锚点 = 上一事件的最终音(和弦取最后一个音的八度)。
- **纯四度(含)以内默认连续**:候选音距锚点 ≤ 纯四度(5 个半音)时取同向连续音;超出时取最近八度;两侧均在四度内时取上行。
- **八度后缀**:`*` 与 `'` → 在其**前**音起升一个八度组(同义);`.` → 从其**后**音起降一个八度组。实现:`octave_shift` 累计,作用于消解结果。
- **谱内音区前缀 `>n`**:重置本声部锚点为该八度(rule_0_3 #11),其后继续逐音推进。
- 绝对音(A~G 字母 / 吉他弦名)直接定音并成为新锚点。
- **无音高打击声部不做八度消解**(§7.10)。

### 7.6 调号消解与固定变音

- `|=C` → fifths=0;`|=G`→1;`|=D`→2;`|=A`→3;`|=E`→4;`|=B`→5;`|=#F`→6;`|=#C`→7;`|=F`→-1;`|=bB`→-2;`|=bE`→-3;`|=bA`→-4;`|=bD`→-5;`|=bG`→-6;`|=bC`→-7;小调 mode=minor。
- `|@[7-3-6-]` 型:把变音集合与标准调号比对,能匹配则等价(降 7/3/6 度 = bB/bE/bA = Eb 大调 fifths=-3);不能匹配(如只降 7 度)→ fifths=0,以逐音 `<accidental>` 表达。
- **固定变音增减(R3/P19)**:`|@[…]`/`\@[…]` 出现在数字行:
  - 位于**小节起点列** → 更新该声部 `KeyState.fixed_alter`,并在该小节重出 `<attributes>`;
  - 位于**小节内部** → 自该列起对本声部后续音逐音施加变音,并告警(无法用单一 `<key>` 表达);
  - `|@[0]` 清空全部固定变音。
- **声部级状态**:`FixedAcc` 随 `Part` 跨组携带。
- 音级→音名:1 = 根音;2/3/4/5/6/7 按大/小调音阶步进;叠 `fixed_alter` → 叠后缀 `+`/`-`/`～`。

### 7.7 和弦 `_` 低音消解与变音输出

- `parse_chord_bass`:取 `_` 后的音序数字串(如 `F_46` → {4,6});
- **音序定义(rule_0_3 #12)**:
  - 七和弦:`3音/5音/7音` = 从根音向上的第 **2/3/4** 个音;
  - 三和弦:`2音/4音/6音/7音` = 紧邻 1 音上方 / 紧邻 3 音上方 / 紧邻 5 音上方 / 紧邻 8 音下方的调内音;
- 消解结果为 (step, alter),写入 `<bass>`;
- **消解规则(根音位置标注,v0.3.1+2 落地;此前误按调内级数消解)**:
  - `_3`、`_5` = 原位三和弦、`_7` = 原位七和弦 → 低音 = 根音;
  - `_6`、`_56` = **三音为低音** → 低音 = 根音上方三音(大/小三度随和弦品质;`dim` 减三度);
  - `_46`、`_34` = **五音为低音** → 低音 = 根音上方五音(纯五度;`aug` 增五度;`dim` 减五度);
  - `_2` = **七音为低音** → 低音 = 根音上方七音(七和弦按品质取大/小七度;三和弦取紧邻 8 音下方的**调内音**);
  - `_4`(实践出现)按五音为低音处理;
- **规则例句验证向量(已消解,rule.md 附录 B-5 匡正)**:`G_6`、`F_46`、`Em7_34`、`Bmin7_56`、`bBmaj7_2` 的自注"根音分别是 7,1,7,2,6"按**低音位置映射**完全吻合——C 大调下:`G_6` 低音 = 三音 B(=7)、`F_46` 低音 = 五音 C(=1)、`Em7_34` 低音 = 五音 B(=7)、`Bmin7_56` 低音 = 三音 D(=2)、`bBmaj7_2` 低音 = 七音 A(=6)。实现以该映射为准,并保留此五例为单元测试向量。
- **变音与 `<accidental>`**:
  - `pitch.alter` = 音级消解 + 固定变音 + 后缀变音 的合成;
  - `<alter>` 仅 alter ≠ 0 时输出;
  - `<accidental>`:当 (step, alter) 非当前调号调内音时输出 `sharp`/`flat`/`natural`;调内自然音不输出。

### 7.8 休止与整小节休止

- `0` 起始的休止持续到下一事件;相邻休止自然合并;
- 声明长度类"列距 > 声明长度"的多余间距 → 记为休止(§7.1-2);
- 某小节所有事件恰为一个占满全小节的休止 → `<rest measure="yes"/>` + duration = 小节 tick + type=whole;
- 其他休止:duration = 实际 tick,type 由 §3.3 计算;
- 短行/空行部分 → 按剩余列数补休止(整小节 → `measure="yes"`)。

### 7.9 `@lN` 行复制

- **语义(R10)**:`@l9` 出现在任意数字行,表示"从此以后,直到下一次某事件覆盖了这种状态,该声部均存在,且复制**文档总第 9 行**的内容"。
- **算法**:
  1. `N` = 文档**总行号**(1 基,含标题行与所有行);
  2. 自记号所在**列**起,按**列对齐**复制源行该列起的数字内容;
  3. 复制内容按**本声部**的音区锚点与 `FixedAcc` **重新消解**(不继承源声部状态);
  4. 本声部自身事件在同一列上**覆盖**复制内容;
  5. 终止:`-` 记号(§7.1)或**任意其余符号**(任何显式事件,rule.md 附录 B-2 匡正);
  6. 越界(`N` 超出行数 / 源行为空)→ 告警忽略;
  7. **循环防护**:若源行经 `@lN` 链回到本声部 → 检出、在该列截断 + 告警(防无限展开)。
- **实测校验**:part-IV 行 29 `>4||@l7`(伴奏 #4 复制主旋律行)、行 30 `||@l9`(复制和声行)、行 18/24 行中行尾的 `@l7`。
- **不复制歌词**(v0.3.1 #1):`@lN` 复制调整行时忽略源行的歌词行。
- **`@lN^M` 平行和声**(v0.3.1 #2,在补全副本阶段 §7.0 展开):
  - 语义:在第 N 行音轨的上方(`+`)/下方(`-`)|M| 度置一个平行和声;
  - 约束:|M| 必须为 7n+{3,4,5,6,8} 之一(n 非负整数),否则告警忽略;
  - 取音规则(逐列):
    - a. 该行音是**此刻的和弦内音** → 寻找这一和弦**以这个音为根音的那个形式**,按 §7.3「和弦诸音的定义」取对应音;
    - b. 否则,取**对该行调号而言的调内音**,使其在该音上方或下方 |M| 度;
    - c. 持续到下一个有效符号被检出;
  - 展开产物:一条调整行,放于源行**上方**(该处为空);若已非空,先在源行上方**插入新空行**再写入(#6);**插入空行后须对副本中所有行数引用(`@lN`/`@lN^M`)补正**(rule.md 附录 B-3 匡正)。

### 7.10 无音高打击与鼓组映射

- **非鼓组**(`INS_UNPITCHED`,如 `SD1` 军鼓):任何数字**同等解读为一次发音**(每数字 1 单位),不做音高消解;
  - 音符输出 `<unpitched><display-step>F</display-step><display-octave>4</display-octave></unpitched>`,display 位置按乐器键位查表:

    | 乐器 | 键位 | display | 乐器 | 键位 | display |
    |---|---|---|---|---|---|
    | BD 底鼓 | 36 | F4 | TOML 低通鼓 | 45 | F5 |
    | SD 军鼓 | 38 | C5 | TOMH 高通鼓 | 50 | A5 |
    | ESD 电军鼓 | 40 | B4 | CRSH 吊镲 | 49 | B4 |
    | CHH 闭镲 | 42 | G5 | RIDE 叮叮镲 | 51 | F5 |
    | OHH 开镲 | 46 | A5 | COW 牛铃 | 56 | G5 |
    | TAMB 铃鼓 | 54 | F5 | CONGA 康加 | 64 | C5 |
    | SHAKE 沙锤 | 70 | B4 | WD 木鱼 | 76 | E5 |
    | (未列出者) | — | F4(缺省) | | | |

  - part-list 写 `<midi-unpitched>键位</midi-unpitched>`,**不写** `<midi-program>`;
  - 谱号:`<clef><sign>percussion</sign><line>2</line></clef>`。
- **鼓组**(`INS_DRUMKIT`):映射到**鼓谱**;
  - 默认鼓组映射(可整体替换):`1=BD36`、`2=SD38`、`3=CHH42`、`4=OHH46`、`5=TOML45`、`6=TOMH50`、`7=CRSH49`、`8=RIDE51`;
  - 为出现的每件鼓生成一个 `<score-instrument id="…-I2">`,音符以 `<instrument id="…-I2"/>` 逐音引用;
  - 音符同样用 `<unpitched>` + display(按上表);
  - 谱号用打击谱号。

### 7.11 ID 独一化与 part-list 构造(D4)

1. **清洗**:把 id 转为合法 XML NCName —— `#`/`:`/`(`/`)`/空格 → `-`(如 `Inst.#0` → `Inst.-0`;`Inst.#0-2` 用于第 2 行);
2. **去重**:与 `Score.part_ids_used` 比较,重复则追加 `-2`、`-3`…;
3. **就地定义**(`*Vln|`、`*#N:…`)→ 自动编号 `P<n>` 或按原型 base_id + 序号;
4. **instrument_id** = `score_part_id` + `-I1`(鼓组另加 `-I2`、`-I3`…);
5. **midi-channel** 按输出顺序 1,2,3,… 递增;
6. part 输出顺序:主旋律 `P0` → 和声 `P1..` → 伴奏(按 instruments 行顺序,同乐器内按 row 序)。

---

## 八、特殊表记展开细则

| 类 | 定义形态 | 展开规则 |
|----|----------|----------|
| ① 瞬时 | `x~{135}` / `x～{一三五}` / `x～{CEG}` | 展开为**一个**事件,内部 n 个同刻音:数字 = 简谱音级(按当前调);中文数字 = 同简谱;字母 = 绝对音。事件时值 = 一拍 |
| ② 多瞬时 | `x&"_{358}/7/3-6-"` | 按 `/` 分拍、`_` 分单位;`{…}` = 该拍柱式和弦;单个数字 = 该拍单音;`3-` = 带变音音级。总时值 = 声明单位数 |
| ③ 吉他节奏型 | `x&"a/c/e/a"` | 同 ②,但 `a`~`f` = 琴弦:该拍有活跃和弦 → 按把位表(④ / `{D/200232}` / 默认表)取弦上音;无和弦 → 按 `&gt:` 调弦表取空弦音 |
| ④ 特殊和弦定义 | `{Cmy/300353}` | **不产生事件**;登记「和弦名 → 把位」供 ③ 查询 |
| ⑤ 和弦节奏型 | `x&r"{358}7654345"` | 数字为**和弦相对音**(1,3,5,7,8,9… 为和弦内音;其他取调内音);`(7/8)` = 7 是和弦内音则奏七音、否则奏 8 音;`[56]` = 该拍时值被平分;`@[6-7-3-]` = 标记变音;穿插 A~G = 绝对音;`+`/`-`/`～` 可跟在数字或字母后;使用处 `x@|3-6-7-4+` = 临时绝对变音。**和弦切换 → 节奏组重头**(仅 ⑤);**和弦根音八度:以当前锚点最近原则** |
| ⑥ 吉他把位表注 | `X&g"xxxxxx"` | **v0.3.1 #8**:x∈1..6 = 弦号(**1 为低弦**);声明一段按弦号书写的吉他谱型,展开时经**把位数据库**(§2.5)转换成音位;把位未收录 → 告警 |
| **无引号** | `Q&{13}_{13}_{13}_` | 与 `&"…"` 同义(`quoted = false`) |
| **参数化** | `X(V)&r"V_{35}_"` | 解析时把调用点 `X(3)` 的实参 V **文本代入**定义内容(`V` → `3`),代入后按定义自身类别解析。`V` 为**绝对唱名**,不是和弦根音/三音(rule_0_3 #5) |
| 通用 | 谱面写 `x` 或 `x@|…` 或 `x(参数)` | 标记名占位一个事件;时值 = 声明单位数;标记名不可重复、不含 ABCDEFG、首尾非数字 |
| **自动重复** | — | **声明单位数 > 1 → 自动重复**(填充至其声明长度);**= 1 → 一次性**;引源含 `r` 时,**每当和弦更换,循环重头开始**;`-` 记号可显式终止重复(rule_0_3 #6 / P16) |

**格内容解析(②③⑤ 定义内部)**

- `_` = 单位分隔;**`/`** = 拍分隔(仅原型定义 ②,②的 `/` 划分拍);
- `{…}` = 柱式(同刻多音);
- `(7/8)` = 条件音;`(A-)` = 绝对音带变音;`[56]` = 该拍平分;`@[6-7-3-]` = 标记变音;
- `8` = 高八度根音;`0` = 停止;
- **首导根音** `1_`(P8)= 以根音起始并占一个单位;
- 格内后缀 `'`/`*`/`.`/`+`/`-`/`～` 按 §7.5/§7.7 处理。

**`=` 段与标记的结合(P9,rule.md 附录 B-1 匡正)**

- **等号段** = 连续 `=` 的最大串;段的**开头那个 `=`** 永远满足且仅满足以下三种情况之一:a. 正上方为 `|`;b. 正下方为 `\`;c. 是标记一段等号结束的那个 `=`。由此即可区分"两次单 `=`"与"省略中间段的长等号段"。
- 段替换内容 = **段首列同列**的机动段(上方 `|段` / 下方 `\段`);段时值 = 等号个数 × 1 列(每 `=` 1 列);无机动段配对 → 告警 + 按休止处理。
- **`=` 后跟随的标记名针对其后的部分,与等号段没有任何关系**(如 `====X` 中的 `X`、`=T(4)` 中的 `T(4)` 均为段结束之后的普通标记调用,时值 = 声明单位数)。

---

## 九、错误处理与容错

- 两级诊断:**警告**(可恢复,继续解析)与**错误**(中止)。全部写入 stderr,格式 `行号: 类别: 信息`。
- **典型警告**:
  - 未识别记号(按字面跳过,含行列位置);
  - 特殊表记名未定义 / 参数化调用缺参数;
  - `=` 无对应机动段;`-` 无进行中的状态;
  - 孤立 `+`、`**`、`"…"` 段(笔误段,P15/P16);
  - 声明长度与列距不符(过长 → 补休止;不足 → 截断);
  - 总占用列数超过小节列数(截断);
  - 复制行(`@lN`)越界 / 循环引用;
  - 机动行数 > 2(忽略多余);
  - 小节内固定变音变更(退化为逐音变音);
  - `_` 低音音序非法;
  - ID 重名已自动改名;
  - 定义引号未闭合(取至**其后第一个空格**,rule.md 附录 B-11 匡正);
  - 延音线两端音高不同(丢弃);
  - 拍号缺省(沿用上一组,曲首按 4/4);
  - 组头缺失(沿用上一组);
  - 大跨度裸括号 `(…)`:人声段 → 内容在补全副本阶段转移至伴奏声部、不分配歌词(rule.md 附录 B-8 匡正);否则跳过不产出。
- **典型错误**:文件不存在/空、instruments 行缺失、`{`/`}` 不配对、曲首无拍号、内存分配失败。
- **编码**:输入输出均 UTF-8;非法 UTF-8 字节按单字节容错处理。

---

## 十、测试用例(两套)

### 10.1 套 A —— 合成用例(覆盖全部功能)

> 设计意图:一次覆盖 —— 文件头与 instruments 行;**`<work>`/`<identification>` 输出**;part-list 三种来源(旋律/和声/伴奏乐器/**非鼓组 unpitched**);4/4 与 6/8(**6/8 每小节 12 列,accu=2**);`|=C` 与 `|=G`;单音、八分、附点、休止、整小节休止、全音符;`{13}` 柱式和弦与特殊表记展开的柱式和弦;`%234`(4/4 一拍三连音)、`%12345`(五连音)、6/8 下 `[123]`(一拍三等分)、`|%654`(机动行 `=` 替换);`*`/`.`/`+`/`～`/`'` 五种后缀;`>4` 与 `*4` 双轨音区;`#` 和声行;歌词(**`*l|"…"` 与旧式裸歌词双轨**)与歌词 `-` 延音线;**跨小节音符(tie 三形态)**;特殊表记 ①~⑤ + 参数化 + 无引号 + 自动重复;`=`+标记(`====X`、`=T(4)`);`-` 结束记号;`@l7` 行复制;`|@[7-3-6-]` 中途增减;`\` 下指段;管道实例 `|{42}_{31}_`;和弦行(含 `_` 低音、`-` 无和弦、中途换和弦);**`|vb120.000000`/`|rit.`/`|s5` → `<direction>` 输出**;`|。` → `<fermata>`;**SD1 非鼓组 → `<unpitched>` 输出**;组末力度;第二组拍号/调号变化与 attributes 重输出;**v0.3.1 要素另见 §10.3**。

```text
test-song by ClaudeCode
instruments:NGTR1 SD1
#timestamp:|||   .   .   .   |
{ |4/4 |=C |vb120.000000 &gt: {a=E2,b=A2,c=D3,d=G3,e=B3,f=E3}}||  X(V)&r"V_{35}_{35}_" Q&{13}_{13}
chords:||C_6        Am       -
||
/*LV.pitch range of 1st note:*/ >4 ch~{135}||1   2   %234 5       4+  1 3 ch
*l|"我 爱 你 晚 风"||
[#/*Harmony*/||1*  .6  1*  .6  0
||
#timestamp:|||   .   .   .   |
(NGTR1:Nylon Guitar<古典尼龙吉他; >4 ar&"{135}/1" pa&r"{135}1"||ar      pa      {13}%12345  0
(SD1:Snare Drum<军鼓; >4||1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2
}  |s5
#timestamp:||| . . . . . |
{ |6/8 |=G |vb90||
||            |%654
/*LV.pitch range of 1st note:*/ *4||1   2  3  =  5  7~
/*lirics*/||我 你 他
[/*Harmony*/||
||(5)
#timestamp:||| . . . . . |
(-NGTR1-||
||====X              0   [123]
(-SD1-||
||
}
```

> 说明:
> - 第 1 组(4/4,|=C,每小节 16 列):旋律 m1 = `1`(我) `2`(`-` 延音线) `%234`(爱,一拍三连音) `5`(你,跨小节起点);m2 = `5` 延续(tie stop) `4+`(升 F,晚) `1`(风) `3` `ch`(① 类 → C4,E4,G4 柱式和弦)。和声行 `1*`(升八度 C5) `.6`(降八度 A3) ×2,m2 的 `0` 停在小节线、m2 整小节休止。伴奏 m1 = `ar`(② 类) `pa`(⑤ 类,Am 相对音);m2 = `{13}` `%12345`(五连音) `0`。SD1(非鼓组)→ **每个数字一次发音**,`<unpitched>` 输出(不再跳过)。组末 `|s5` → 第 1 组末小节输出 `<direction><dynamics><mp/></dynamics></direction>`。和弦行 `C_6`(低音 A)、`Am`、`-`(无和弦)→ `<harmony>` 输出 2 条。`|vb120.000000` → `<metronome>120</metronome>`。
> - 第 2 组(6/8,|=G,每小节 12 列):旋律 `1 2 3`(八分) `=`(机动行 `|%654` 替换为 16 分三连音 E5,D5,C5) `5`(D5) `7~`(还原)。和声 `(5)` → D4 附点二分。伴奏 `====X`(4 列等号段由同列机动段替换;`X` 为段后独立标记调用,rule.md 附录 B-1 匡正) `0` `[123]`(1 拍 = 2 列三等分 G2,A2,B2)。第 2 组 attributes 重输出(|=G、6/8)。
> - divisions = 4×lcm(3,5) = **60**;4/4 小节 = 240 tick,6/8 小节 = 180 tick。

### 10.2 套 B —— part-IV 行 1-31 全曲转换(权威回归)

**结构性验收**:

1. part 数 = **16**(1 主旋律 + 2 和声 + 伴奏 #0×3 行、#1×3 行、#2×3 行、#3×2 行、#4×2 行;纯机动行不产 part,实施时按实测复核);
2. 每 part 每小节 duration 和 = 小节 tick(6/8 = 3×divisions);
3. direction/harmony 输出列(排序键)单调;
4. 无悬空 tie(每个 start 有 stop);
5. 无"警告升级为错误"(除 P15/P16 笔误段的预期警告外)。

**行片段断言组**:

| 行 | 断言 |
|---|---|
| 行 4 | 22 个定义全部注册(含 `X(V)`/`T(I)` 参数化、`Q` 无引号、`J` 未闭合引号容错);`\|rit.`/`\|vb.` 产出 direction |
| 行 6/7 | 机动 `+`@339、`'`@574 绑定行 7 对应列;行 7 事件间距直方图复现(2 列八分/1 列紧排/长持音);`(…)` 大跨度括号跳过 + 告警 |
| 行 9/13 | 和声声部自 ~1048 列起,此前整小节休止 |
| 行 16/17 | `\|{42}_{31}_` 替换 `====X` 的 4 列等号段(上指 ✔);`======X` 的 6 列段同理由机动段替换,`X` 为段后独立调用;`=T(4)` = `=` 段 + 段后 `T(4)` 调用;裸 `=` 与 `\|[…4 格…]` 配对等分(rule.md 附录 B-1 匡正) |
| 行 19→20 | 单独符号(含 12 连 `-`? — 按实测)绑定行 20 对应列;`\|@[0]` 清空固定变音;行 19 尾数字归本行声部 |
| 行 21→上方行 | `\@[3-6-7-]`、`\ ' .' . \` 绑定**上方**数字行(下指 ✔,作者确认);`@l9`/`-H` 按 §7.1 拆解 |
| 行 23-27 | 各伴奏行事件计数与标记展开;`-` 结束记号 |
| 行 29/30 | `@l7`/`@l9` 分别复制文档第 7/9 行全列,part 数与时长守恒 |
| 行 31 | `}\|\|` 空组尾正常 |

**对照回归**:output/4(空曲,`*3`/`*4` 旧格式)必须不报错转出骨架。

### 10.3 v0.3.1 新增要素小用例

```text
4-2 如梦人生的X日终焉 by 久未至
instruments:Inst.#0 Inst.#1
#timestamp:||| . . . . . |
{ |6/8 |=C |vb120.000000 &gt: {a=E2,b=A2,c=D3,d=G3,e=B3,f=E3}}||  H&r"{135(7/8)}_1_1_1_1_" G&g"654321" X(V)&r"V_{35}_{35}_"
chords:||G_46       Cmaj      G7_7
||
/*LV.pitch range of 1st note:*/ >4||1   2   3   H_6   @l5^+3   0
*l|"我 们 一起 唱 着 歌"
[/*Harmony*/||
||
#timestamp:||| . . . . . |
(Accomponiment#0:Inst.#0<Instrument#0; >4||G      0
||
}
```

> 断言:
> 1. `@l5^+3` → 补全副本阶段在源行(文档总第 5 行 = 本数字行)上方生成平行和声调整行;各列按 §7.9 规则 b(本行音非和弦内音时取调内音上方 3 度)展开,持续到 `0`;
> 2. `H_6`:活动和弦 `G_46` 处按 `G_6`(低音 = 6 音)消解本行的 ⑤ 类内容;其他行不受影响;
> 3. `Cmaj` → `<kind>augmented</kind>`(增三和弦);`G7_7` → 原位七和弦;
> 4. `G&g"654321"` → ⑥ 类,经把位数据库展开为吉他六弦音位;
> 5. 歌词「我 们 一起 唱 着 歌」与音符**逐字句一一对应**(不等列);
> 6. 补全副本以 `.tymp` 扩展名落盘,后续解析针对副本;若文档再含第二个「标题 by 作者」行 → 输出两个 .musicxml。

### 10.4 首版用例(保留,基于旧列模型)

> **首版原文保留**。注意:本用例按**旧列模型**书写(固定 accu=4,6/8 每小节 24 列),已被 §10.1 套 A(新列模型)取代;此处保留备查,其预期输出见 §11.2。旧模型要点:`||` 统一在第 84 字节处、`*4` 音区前缀、裸按列歌词、6/8 每小节 24 格、divisions = 4×accu×L = 240。

```text
test-song by ClaudeCode
instruments:NGTR1 SD1
#timestamp:                                                                        |||   .   .   .   |
{ |4/4 |=C |vb120 &gt: {a=E2,b=A2,c=D3,d=G3,e=B3,f=E3}}                             ||
chords:                                                                            ||C       Am
                                                                                   ||
/*LV.pitch range of 1st note:*/ *4 ch~{135}                                        ||1   2   %234 5       4+  1 3 ch
/*lirics*/                                                                         ||我  - 爱 你 晚 风
[#/*Harmony*/                                                                      ||1*  .6  1*  .6  0
/*lyrics*/                                                                         ||
[#/*Harmony2*/                                                                     ||1   1*  5
/*lyrics*/                                                                         ||
#timestamp:                                                                        |||   .   .   .   |
(NGTR1:Nylon Guitar<古典尼龙吉他; *4 ar&"{135}/1" pa&r"{135}1" gt&"a/c/e/a"         ||
                                                                                   ||ar      pa      {13}%12345  0
(SD1:Snare Drum<军鼓; *4                                                            ||
                                                                                   ||
}  |s5                                                                             ||
#timestamp:                                                                        |||   .   .   .   |   .   .   .
{ |6/8 |=G |vb90                                                                   ||
                                                                                   ||
                                                                                   ||            |%654
/*LV.pitch range of 1st note:*/ *4                                                 ||1   2  3  =  5  7~
/*lirics*/                                                                         ||
[/*Harmony*/                                                                       ||
                                                                                   ||(5)
/*lyrics*/                                                                         ||
[/*Harmony2*/                                                                      ||        0
/*lyrics*/                                                                         ||
#timestamp:                                                                        |||   .   .   .   |   .   .   .
(-NGTR1-                                                                           ||
                                                                                   ||gt              0   [123]
(-SD1-                                                                             ||
                                                                                   ||
}                                                                                  ||
```

> 首版说明:
> - 第 1 组(4/4,|=C):旋律 m1=`1`(我) `2`(`-` 延音线) `%234`(爱) `5`(你,跨小节起点);m2=`5` 的延续(tie stop) `4+`(晚,升F) `1`(风) `3` `ch`(特殊表记① → C4,E4,G4 柱式和弦)。和声行(带 `#`):`1*`(升八度 C5) `.6`(降八度 A3) ×2,行末 m2 的 `0` 使 A3 停在小节线、m2 成为整小节休止(rest measure="yes")。和声2行:`1`(C4) `1*`(C5) `5`(G4,行内无后续事件 → 持至行末,即跨 2 条小节线:第 1 组 m2 全小节为中间片段 start+stop)。伴奏行 m1=`ar`(②类:{135}→C4,E4,G4 柱式和弦 + 1→C4) `pa`(⑤类,Am 和弦相对音:1,3,5→A3,C4,E4 + 1→A3);m2=`{13}`(C4+E4) `%12345`(五连音 C4~G4) `0`(四分休止)。组末 `|s5` 只解析不输出。SD1 为无音高打击 → 整行跳过并告警(首版行为;本版已改为输出,见 §7.10)。
> - 第 2 组(6/8,|=G):旋律 `1 2 3`(G4,A4,B4 八分) `=`(机动行 `|%654` 替换为 16 分三连音 E5,D5,C5) `5`(D5) `7~`(还原 F♮5 → `<accidental>natural</accidental>`)。和声行 `(5)` → D4 附点二分(整小节,isDot)。和声2行 `0` 终止跨组延音线(G4 的 stop 片段)+ 二分休止。伴奏行 `gt`(③类,空弦 a/c/e/a → E2,D3,B3,E2) `0`(八分休止) `[123]`(一拍三等分 G2,A2,B2)。
> - 首版期待行为:tie 三形态齐全(G4-旋律:start/stop;G4-和声2:start / start+stop / stop);歌词 `-` 生成 D4→D4 延音线;attributes 在第 3 小节因 |=G、6/8 重输出;divisions=4×4×lcm(3,5)=240。

---

## 十一、预期输出(关键片段)

### 11.1 套 A 关键片段(divisions=60)

> 完整输出由 §10 套 A 按 §4 规格生成。此处给出**新增要素**的精确写法片段(旧要素与 musicxmlEg.xml 一致的从略)。divisions=60:四分=60、八分=30、16 分=15、%234 三连音=20、%12345 五连音=24、6/8 小节=180。

```xml
<score-partwise version="4.0">
  <work><work-title>test-song</work-title></work>
  <identification><creator type="composer">ClaudeCode</creator></identification>
  <part-list>
    <score-part id="P0"><part-name>Melody</part-name>…</score-part>
    <score-part id="NGTR1">
      <part-name>Nylon Guitar</part-name>
      <score-instrument id="NGTR1-I1"><instrument-name>Nylon Guitar</instrument-name></score-instrument>
      <midi-instrument id="NGTR1-I1"><midi-channel>4</midi-channel><midi-program>25</midi-program></midi-instrument>
    </score-part>
    <score-part id="SD1">
      <part-name>Snare Drum</part-name>
      <score-instrument id="SD1-I1"><instrument-name>Snare Drum</instrument-name></score-instrument>
      <midi-instrument id="SD1-I1"><midi-channel>5</midi-channel><midi-unpitched>38</midi-unpitched></midi-instrument>
    </score-part>
  </part-list>

  <!-- 主旋律 第 1 小节:速度 direction + 和弦 harmony 按列归并 -->
  <part id="P0">
    <measure number="1">
      <attributes>
        <divisions>60</divisions>
        <key><fifths>0</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <direction>
        <direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>120</per-minute></metronome></direction-type>
        <sound tempo="120"/>
      </direction>
      <harmony>
        <root><root-step>C</root-step></root>
        <kind>major</kind>
        <bass><bass-step>A</bass-step></bass>
      </harmony>
      <note>…(C4 四分,lyric 我)…</note>
      <note>…(D4 四分,tie start,lyric - → tied)…</note>
      <note>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>20</duration>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
        <notations><tuplet type="start" number="1" bracket="no" show-number="actual" placement="above"/></notations>
        <lyric><syllabic>single</syllabic><text>爱</text></lyric>
      </note>
      …
    </measure>
  </part>

  <!-- 军鼓(非鼓组):unpitched + 打击谱号 -->
  <part id="SD1">
    <measure number="1">
      <attributes>
        <divisions>60</divisions>
        <key><fifths>0</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>percussion</sign><line>2</line></clef>
      </attributes>
      <note>
        <unpitched><display-step>C</display-step><display-octave>5</display-octave></unpitched>
        <duration>15</duration>
        <voice>1</voice>
        <type>16th</type>
      </note>
      …
    </measure>
  </part>
</score-partwise>
```

### 11.2 首版完整预期输出(保留,基于旧列模型 divisions=240)

> 由 §10.4 首版用例按旧模型规格生成的完整预期输出,首版原文保留。逐音符时值(divisions=240):4/4 下格=60、四分=240、八分=120、附点八分=180、三连八分=80、五连八分=96、全音符=960;6/8 下格=30、八分(一拍)=120、16 分三连=40、附点二分=720。**本版差异**:无 `<work>`/`<identification>`、SD1 整行跳过(无 `<unpitched>`)、无 `<direction>`/`<harmony>`、6/8 每小节 24 格。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC
  "-//Recordare//DTD MusicXML 4.0 Partwise//EN"
  "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="4.0">

  <part-list>
    <score-part id="P0">
      <part-name>Melody</part-name>
      <score-instrument id="P0-I1">
        <instrument-name>Melody</instrument-name>
      </score-instrument>
      <midi-instrument id="P0-I1">
        <midi-channel>1</midi-channel>
        <midi-program>1</midi-program>
      </midi-instrument>
    </score-part>
    <score-part id="P1">
      <part-name>Harmony</part-name>
      <score-instrument id="P1-I1">
        <instrument-name>Harmony</instrument-name>
      </score-instrument>
      <midi-instrument id="P1-I1">
        <midi-channel>2</midi-channel>
        <midi-program>1</midi-program>
      </midi-instrument>
    </score-part>
    <score-part id="P2">
      <part-name>Harmony 2</part-name>
      <score-instrument id="P2-I1">
        <instrument-name>Harmony 2</instrument-name>
      </score-instrument>
      <midi-instrument id="P2-I1">
        <midi-channel>3</midi-channel>
        <midi-program>1</midi-program>
      </midi-instrument>
    </score-part>
    <score-part id="NGTR1">
      <part-name>Nylon Guitar</part-name>
      <score-instrument id="NGTR1-I1">
        <instrument-name>Nylon Guitar</instrument-name>
      </score-instrument>
      <midi-instrument id="NGTR1-I1">
        <midi-channel>4</midi-channel>
        <midi-program>25</midi-program>
      </midi-instrument>
    </score-part>
  </part-list>

  <!-- ========== 主旋律 ========== -->
  <part id="P0">
    <measure number="1">
      <attributes>
        <divisions>240</divisions>
        <key><fifths>0</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
        <lyric><syllabic>single</syllabic><text>我</text></lyric>
      </note>
      <note>
        <pitch><step>D</step><octave>4</octave></pitch>
        <duration>240</duration>
        <tie type="start"/>
        <voice>1</voice>
        <type>quarter</type>
        <notations><tied type="start"/></notations>
      </note>
      <note>
        <pitch><step>D</step><octave>4</octave></pitch>
        <duration>80</duration>
        <tie type="stop"/>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
        <notations>
          <tied type="stop"/>
          <tuplet type="start" number="1" bracket="no" show-number="actual" placement="above"/>
        </notations>
        <lyric><syllabic>single</syllabic><text>爱</text></lyric>
      </note>
      <note>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>80</duration>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
      </note>
      <note>
        <pitch><step>F</step><octave>4</octave></pitch>
        <duration>80</duration>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
        <notations><tuplet type="stop" number="1"/></notations>
      </note>
      <note>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>240</duration>
        <tie type="start"/>
        <voice>1</voice>
        <type>quarter</type>
        <notations><tied type="start"/></notations>
        <lyric><syllabic>single</syllabic><text>你</text></lyric>
      </note>
    </measure>
    <measure number="2">
      <note>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>240</duration>
        <tie type="stop"/>
        <voice>1</voice>
        <type>quarter</type>
        <notations><tied type="stop"/></notations>
      </note>
      <note>
        <pitch><step>F</step><alter>1</alter><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
        <accidental>sharp</accidental>
        <lyric><syllabic>single</syllabic><text>晚</text></lyric>
      </note>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
        <lyric><syllabic>single</syllabic><text>风</text></lyric>
      </note>
      <note>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
    </measure>
    <measure number="3">
      <attributes>
        <divisions>240</divisions>
        <key><fifths>1</fifths><mode>major</mode></key>
        <time><beats>6</beats><beat-type>8</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>A</step><octave>4</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>B</step><octave>4</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>E</step><octave>5</octave></pitch>
        <duration>40</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
        <notations><tuplet type="start" number="2" bracket="no" show-number="actual" placement="above"/></notations>
      </note>
      <note>
        <pitch><step>D</step><octave>5</octave></pitch>
        <duration>40</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
      </note>
      <note>
        <pitch><step>C</step><octave>5</octave></pitch>
        <duration>40</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
        <notations><tuplet type="stop" number="2"/></notations>
      </note>
      <note>
        <pitch><step>D</step><octave>5</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>F</step><octave>5</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
        <accidental>natural</accidental>
      </note>
    </measure>
  </part>

  <!-- ========== 和声 ========== -->
  <part id="P1">
    <measure number="1">
      <attributes>
        <divisions>240</divisions>
        <key><fifths>0</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>C</step><octave>5</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>A</step><octave>3</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>C</step><octave>5</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>A</step><octave>3</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
    </measure>
    <measure number="2">
      <note>
        <rest measure="yes"/>
        <duration>960</duration>
        <voice>1</voice>
        <type>whole</type>
      </note>
    </measure>
    <measure number="3">
      <attributes>
        <divisions>240</divisions>
        <key><fifths>1</fifths><mode>major</mode></key>
        <time><beats>6</beats><beat-type>8</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>D</step><octave>4</octave></pitch>
        <duration>720</duration>
        <voice>1</voice>
        <type>half</type>
        <dot/>
      </note>
    </measure>
  </part>

  <!-- ========== 和声2(跨两条小节线的延音线) ========== -->
  <part id="P2">
    <measure number="1">
      <attributes>
        <divisions>240</divisions>
        <key><fifths>0</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>C</step><octave>5</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>480</duration>
        <tie type="start"/>
        <voice>1</voice>
        <type>half</type>
        <notations><tied type="start"/></notations>
      </note>
    </measure>
    <measure number="2">
      <note>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>960</duration>
        <tie type="start"/>
        <tie type="stop"/>
        <voice>1</voice>
        <type>whole</type>
        <notations>
          <tied type="start"/>
          <tied type="stop"/>
        </notations>
      </note>
    </measure>
    <measure number="3">
      <attributes>
        <divisions>240</divisions>
        <key><fifths>1</fifths><mode>major</mode></key>
        <time><beats>6</beats><beat-type>8</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>240</duration>
        <tie type="stop"/>
        <voice>1</voice>
        <type>quarter</type>
        <notations><tied type="stop"/></notations>
      </note>
      <note>
        <rest/>
        <duration>480</duration>
        <voice>1</voice>
        <type>half</type>
      </note>
    </measure>
  </part>

  <!-- ========== 伴奏(尼龙吉他) ========== -->
  <part id="NGTR1">
    <measure number="1">
      <attributes>
        <divisions>240</divisions>
        <key><fifths>0</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>A</step><octave>3</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>A</step><octave>3</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
    </measure>
    <measure number="2">
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>96</duration>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>5</actual-notes><normal-notes>4</normal-notes></time-modification>
        <notations><tuplet type="start" number="1" bracket="no" show-number="actual" placement="above"/></notations>
      </note>
      <note>
        <pitch><step>D</step><octave>4</octave></pitch>
        <duration>96</duration>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>5</actual-notes><normal-notes>4</normal-notes></time-modification>
      </note>
      <note>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>96</duration>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>5</actual-notes><normal-notes>4</normal-notes></time-modification>
      </note>
      <note>
        <pitch><step>F</step><octave>4</octave></pitch>
        <duration>96</duration>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>5</actual-notes><normal-notes>4</normal-notes></time-modification>
      </note>
      <note>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>96</duration>
        <voice>1</voice>
        <type>eighth</type>
        <time-modification><actual-notes>5</actual-notes><normal-notes>4</normal-notes></time-modification>
        <notations><tuplet type="stop" number="1"/></notations>
      </note>
      <note>
        <rest/>
        <duration>240</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
    </measure>
    <measure number="3">
      <attributes>
        <divisions>240</divisions>
        <key><fifths>1</fifths><mode>major</mode></key>
        <time><beats>6</beats><beat-type>8</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note>
        <pitch><step>E</step><octave>2</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>D</step><octave>3</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>B</step><octave>3</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>E</step><octave>2</octave></pitch>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <rest/>
        <duration>120</duration>
        <voice>1</voice>
        <type>eighth</type>
      </note>
      <note>
        <pitch><step>G</step><octave>2</octave></pitch>
        <duration>40</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
        <notations><tuplet type="start" number="2" bracket="no" show-number="actual" placement="above"/></notations>
      </note>
      <note>
        <pitch><step>A</step><octave>2</octave></pitch>
        <duration>40</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
      </note>
      <note>
        <pitch><step>B</step><octave>2</octave></pitch>
        <duration>40</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>
        <notations><tuplet type="stop" number="2"/></notations>
      </note>
    </measure>
  </part>

</score-partwise>
```

### 校验要点(测试通过标准)

1. 每小节各声部 duration 之和 = 小节 tick(4/4 = 4×divisions、6/8 = 3×divisions);
2. 每个 `<tie type="start">` 必有同音高的 `<tie type="stop">`;中间小节同时含 start 与 stop;
3. 每个 tuplet start 有同 number 的 stop;number 按声部内出现顺序递增;
4. 调号/拍号变化的小节重新输出 `<attributes>`;
5. `<chord/>` 出现在 `<pitch>` 之前,仅和弦第 2 音起;
6. `<work>`/`<identification>` 在 `<part-list>` 之前;
7. direction/harmony 按 col 单调归并、无 `<offset>` 元素;
8. 打击音符无 `<pitch>`,有 `<unpitched>`;打击谱号 `percussion`;
9. 全部 id 均为合法 NCName(无 `#`/`:`/空格),重名已加后缀;
10. divisions 全局唯一,所有 duration 为整数。

---

## 十二、实现里程碑

| 里程碑 | 内容 | 验收 |
|--------|------|------|
| M1 | tymp.h **加法扩展**(time-modification/unpitched/direction/harmony 等)+ tymp_internal.h + tympUtils.c 基础(字符串/有理数/调号/时值表示/divisions 新公式) | 单测通过;扩展编译不破坏旧字段 |
| M2 | 文件读入、`classify_line` 行角色动态分类(全表)+ 组头行头元数据(双语法调号/速度力度产出)+ 行头六形态 + ID 独一化 + 默认钢琴 + `make_tymp_copy` 骨架(`@lN` 行复制补全,v0.3.1 #6) | 能打印结构树;part-IV 行 1-31 分类正确 |
| M3 | 数字行主扫描器(含 `'`/`>n`/`=`+标记/`-`/`@lN`)+ 机动行方向绑定(含向上堆叠 #7)+ 管道实例 + 特殊表记①~⑥+参数化+自动重复+`H_6` 替换+`@lN^M` 平行和声补全(含插入空行) | 事件流正确 |
| M4 | 事件 → 小节切分 + 跨小节拆分 + 延音线 + 歌词(`*l` 分隔/旧式双轨、**逐字句对应** v0.3.1 #5)+ direction/harmony 归位 + 打击事件 | 事件 → note_tymp 正确 |
| M5 | ev2mxml + 输出层(part-list 三分支/attributes 打击谱号/direction/harmony/unpitched/work/fermata) | 输出格式与 musicxmlEg.xml 及 §11 一致 |
| M6 | 全管线 + main + 错误处理 | 通过 §10.1 套 A + §10.3 v0.3.1 小用例 |
| M7 | **part-IV 行 1-31 全曲转换 + 行片段断言组(§10.2)+ output/4 旧格式回归** | 全绿;结构性验收通过 |

---

## 十三、待确认决策清单

**已被 rule_0_3 / 用户决策消解的原条目**:

1. ~~力度/速度/渐强渐弱/延音号不输出~~(原 #3)→ rule_0_3 #7 + 用户 D1:**全部输出**(§4.5);延音号 → `<fermata>`。
2. ~~和弦行不输出 `<harmony>`~~(原 #4)→ 输出(§4.5);`_` 低音多位拆分规则见 §7.7。
3. ~~无音高打击跳过~~(原 #5)→ 输出 `<unpitched>`/鼓谱(§7.10)。
4. ~~标题/作者不输出~~(原 #10)→ 输出 `<work>`/`<identification>`(§4.1)。
5. ~~机动行方向~~→ 已定案(§2.5):`|` 服务下方、`\` 服务上方。

**保留待确认**:

6. **组 n 与八度的对应**:本方案约定 `>n`/`*n` → 该行调根音 1 = MIDI 八度 n(C 大调 `>4` → 1=C4)。若约定不同,只改 `resolve_degree_pitch` 一处。
7. **`#` 的语义**:按「和声行初始锚点 = 主旋律首音实际音高」处理;若实际语义为「首音与主旋律首音同音」,在 §7.5 调整。
8. **`<beam>`**:musicxmlEg.xml 给三连音加 beam 但 tymp.h 无字段;作为可选扩展(对 16th/eighth 短音符按组推断 begin/continue/end)。
9. **歌词 `<lyric>`**:syllabic 全部 `single`;`*l` 引号内文本原样输出,拍内无歌词视同上文。
10. **`[~123]` 等多拍括号与 `(n)` 时值**:每 `~` 多 1 拍;6/8 下 1 拍 = 2 列;`(n)` 括号单音时值约定 = 一拍(至下一事件或小节末,不补空格)(首版 #8/#9,保留)。

**新增待确认**(本版引入):

11. **`_根音` 例句验证**(R12):经 rule.md 附录 B-5 匡正**已消解**——`_6`/`_56` = 三音为低音、`_46`/`_34` = 五音为低音、`_2` = 七音为低音;在 C 大调下五例低音恰为 7,1,7,2,6(§7.7);实现以该映射为准并保留五例为单测向量。
12. **`=` 段语义**(原「内容按 R7 填充/截断」系误解,rule.md 附录 B-1 匡正后**已废弃**):段 = 连续 `=` 最大串;替换内容 = 段首列同列机动段(上方 `|`/下方 `\`);段后标记名属其后内容(§8 P9)。
13. **`@lN` 越界/循环**:告警忽略 / 检出截断(§7.9);复制内容的八度按**本声部**锚点重消解。
14. **`|[…]` 无 `=` 配对的时长**:暂定按 `[…]` 声明(1 拍)独立注入(§7.2.3);若实践另有配对语义再改。
15. **大跨度裸括号 `(…)`**(P15,rule.md 附录 B-8 匡正):从属于**人声段**的括号 → 段内内容在补全副本阶段**转移至伴奏声部**,不分配歌词;非人声段 → 跳过 + 告警,不产连线。
16. **`(5.)` 的 `.`**:按降八度组前缀执行;**附点意图未被实现、也不会被实现**(rule.md 附录 B-9 匡正)。
17. **双行行头名称合并规则**(P11):下一行 head 仅含名称型 token 时并入本行乐器名(方案拟定)。
18. **s1-s10 映射表、鼓组映射表、打击 display 键位表**:见 §4.5/§7.10,均可整体替换(改 `map_dynamic` 与两张表即可)。
19. **`|rit.` 是否补 `<sound tempo>`**:推荐不补(§4.5 注)。
20. **fermata 输出形态**:推荐挂当前列音符的 `<notations><fermata/>`(§4.5)。
21. **标题前缀 `4-1-` 保留与否**:推荐保留(原样输出,§4.1)。
22. **定义「本乐器」作用域判定**:乐器行头 = 本乐器;组头/组尾 = 全局;查找顺序本乐器 → 全局(§2.3)。
23. **未闭合引号**:取至**其后第一个空格** + 告警(rule.md 附录 B-11 匡正,§7.2.4)。
24. **6/8 下 `%` 类的跨拍**(见 §2.4-2 注):若作者意图「6/8 一拍三连音」应走机动行段——**机动段在上方用 `|654`、在下方用 `\654`**(rule.md 附录 B-12 匡正);此口径待 6/8 实践用例确认。

**v0.3.1 新增待确认**:

25. **`@lN^M` 规则 a 的消解细节**:「以该音为根音的和弦形式 + 诸音定义取 M 音」的具体算法(七和弦/三和弦的音序取值与 M∈7n+{3,4,5,6,8} 的对应),实现期以实例验证;不符则回报修订 rule.md(附录 B-3)。
26. **`maj7` 是否受「maj=增三和弦」影响**:本方案按**不受影响**(大三七和弦)处理;若作者意图不同请确认。
27. **多作品的行归属**:以「标题 by 作者」行切分文档;两组之间的行(组、时间戳等)按文档顺序归属其后出现的作品;两份曲谱各生成一个 .musicxml。
28. **歌词逐字句与「拍内无歌词视同上文」并存细则**(§7.2.4):同一拍多字句依次附着该拍各事件,不足延续到后续事件;超出事件数的字句告警丢弃。
29. **`H_6` 的 H 必须声明于第一行**:未在第一行声明 → 告警(§7.3);是否允许其他行声明的 H 待确认。
30. **维护工具**(v0.3.1 #9:增空行/改拍号/增伴奏或和声行 + `@lxx` 行号重排)属**生成侧工具**,不在本转换器范围;转换器只消费其产出(行号以当前文档为准)。

---

*本方案与 rule.md 为配套文档;冲突时以 rule.md 为准。*
