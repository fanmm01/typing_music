# tymp → MusicXML 转换器实现方案(C 语言)

> 本文档给出将「newfileMaker 生成、严格依照 rule_0_2 规范书写」的 .tymp 乐谱文档转换为 MusicXML 4.0 (partwise) 的完整实现方案,精确到每个文件与每个函数,并附一个覆盖全部功能的短测试用例及其预期输出。
>
> 依据(文档域):
> - `rule_0_2.txt` —— 曲谱书写规范(现行)
> - `tymp.h` —— 转换器可用的全部输出参数(约束:只考虑其中已有字段,不擅自增减)
> - `newfileMaker_full0_1.c` + `output/4`(权威模板参照)+ `part-IV.txt` —— .tymp 文件的实际结构
> - `instruments.h` —— 乐器原型表(part-list 的来源)
> - `musicxmlEg.xml` —— 输出 MusicXML 的唯一格式参照
> - `tymp2musicXML.c`(现有骨架,需实现)+ `tympUtils.c`(空文件,待填充)
> - `output/漫游仙境圆舞曲.musicxml` 是 MuseScore 导出文件,**不是**本转换器的输出参照,不参与格式决策。

---

## 一、目标与范围

### 1.1 目标

读取一个 .tymp 文本乐谱,输出一个符合 `musicxmlEg.xml` 风格、可被 MuseScore 等软件打开的 MusicXML 4.0 partwise 文件:

1. 解析文件头(标题/作者/instruments 行)与全部「组」(`{`…`}` 块);
2. 解析组头/行头元数据(拍号、调号、速度、力度、乐器名、初始音区、特殊表记定义、和弦行、吉他调弦表);
3. 解析数字行、机动行、歌词行,展开全部特殊表记(①~⑤),生成逐小节、逐音符事件;
4. 完成简谱→音名的八度消解、变音计算、跨小节拆分与延音线生成;
5. 按 `musicxmlEg.xml` 的写法输出 MusicXML。

### 1.2 硬性约束(用户约定)

| # | 约束 | 对本方案的影响 |
|---|------|----------------|
| C1 | 输出只考虑 `tymp.h` 中已有的参数,不擅自增减 | 力度(`|ppp`/`|s1`…)、速度(`|vb`/`|vi-`/`|ve`/`|。`…)、渐强渐弱(`|si-`/`< <`…)只做**解析校验、不输出**(tymp.h 无 direction/dynamics 字段);和弦行(Chords)只解析供和弦相对音(⑤类)消解用,**不输出** `<harmony>`;标题/作者不输出 `<work>` |
| C2 | 不写 `musicxmlEg.xml` 中不存在的项目 | 不输出 `<beam>`、`<stem>`、`<backup>`、`<forward>`、`<transpose>`、`<identification>`、`<print>`、`<direction>` 中的各子元素、`<harmony>` 等 |
| C3 | 必须正确处理跨小节音符(延音线) | 跨小节长音符必须按小节线拆分,并以 `<tie>`/`<tied>` start/stop 对连接(详见 §7.4) |
| C4 | 方案精确到每个文件、每个函数 | §6 为逐函数规格,不写每一行代码,但给出签名、职责与关键算法要点 |

### 1.3 输出要素边界(总览)

**会输出的 MusicXML 要素**(逐一详细规格见 §4):`score-partwise`、`part-list`、`score-part`、`part-name`、`score-instrument`、`instrument-name`、`midi-instrument`、`midi-channel`、`midi-program`、`part`、`measure`、`attributes`、`divisions`、`key`、`fifths`、`mode`、`time`、`beats`、`beat-type`、`clef`、`sign`、`line`、`note`、`chord`、`rest`(含 `measure="yes"`)、`pitch`、`step`、`alter`、`octave`、`duration`、`tie`、`voice`、`type`、`dot`、`accidental`、`time-modification`、`actual-notes`、`normal-notes`、`normal-type`、`notations`、`tied`、`tuplet`(含 `number`/`bracket`/`show-number`/`placement`)、`lyric`、`syllabic`、`text`。

**明确不输出**(见 §4.6 逐项列理由)。

---

## 二、.tymp 输入格式精读

### 2.1 文件整体结构(以 output/4、part-IV.txt 为准)

```
第 1 行:  「标题 by 作者」
第 2 行:  「instruments:结构id0 结构id1 …」(伴奏乐器的结构 id 表)
第 3 行起:  若干「组」(乐段),每组:
     #timestamp:|||   .   .   .   |   …       ← 拍点网格行(每拍 accu 格,每小节首格 |,其余拍 .)
     { |拍号 |=调号 |速度 &gt: {吉他调弦}    ||  ← 组首元数据行(纯元数据)
     chords:…                              ||  ← 和弦行(可空)
     (空)                                   ||  ← 机动行(第 3 行,可空)
     /*…*/ *音区 [特殊表记定义…]            ||  ← 数字行(第 4 行,旋律)
     /*lirics*/…                            ||  ← 歌词行(第 5 行)
     (空)                                   ||  ← 和声声部前的空行
     [#/*Harmony*/                          ||  ← 和声行(以 [ 开头;可选 # 表示起始音同主旋律)
     /*lyrics*/                             ||  ← 和声歌词行
     …(每个和声行重复上面两行)…
     #timestamp:|||   …                      ← 第二拍点网格行(伴奏区之前的对齐行)
     (空)                                   ||
     (结构id:英文名<中文名; *音区 [定义…]     ||  ← 伴奏行(以 ( 开头)
     (空)                                   ||  ← 伴奏行下一行(非歌词;力度标记正常读取,其余忽略)
     …(每个伴奏乐器重复上面两行)…
     }  |组末元数据                          ||  ← 组末行(以 } 开头,其后为组末元数据)
```

要点(由 rule_0_2 与 output/4 共同确认):

1. **左大括号前的行**可以是任何内容(位置参考),解析时忽略。
2. **`||` 为元数据区结束标记**(推荐每行都有;若无,则以「行内第一个谱面字符」推定)。`||` 之后的区域为乐曲区,各行的乐曲区列号严格对齐(以 `#timestamp` 网格为基准)。
3. 每组允许出现两处 `#timestamp:` 行:一组在旋律区之前、一组在伴奏区之前;解析时直接跳过(仅用于校验/推断 accu)。
4. **组首元数据行不可省略**;「若曲谱共有多列(后续组),从第二列起可空置组首元数据及行首元数据」→ 后续组的组头可为空 `{`、行头可为空,此时**沿用上一组的拍号/调号/速度/乐器/音区/吉他调弦**。
5. 组末行 `}` 之后的内容 = 本组组末元数据(力度等,可对全局生效)。

### 2.2 组头元数据(rule_0_2 §1.1)

| 记号 | 含义 | 转换器的处理 |
|------|------|--------------|
| `\|6/8`(`\|`可省略) | 从该竖线对应小节起拍号;只允许出现在组首行;曲首必须存在 | 解析 → `<time>`;缺省按上一组(曲首缺省 4/4 并告警) |
| `\|=C` `\|=G` `\|=#G` `\|=Dm` 等 | 简谱视同调号;后缀 `~`(`\|=#G~`)表示和弦行**不**跟随转调 | 解析 → `<key><fifths><mode>`;`~` 记录为和弦解析标志 |
| `\|-7-3-6` | 固定变音:降 7、3、6 度 | 若能等价为标准调号(如本例 = 降E大调 fifths=-3)则用 `<key>`;否则 fifths=0 并以逐音 `<accidental>` 表现 |
| `\|vb180` | 绝对速度(从该拍起);曲首必须有,缺省 120 | **解析校验,不输出**(C1) |
| `\|。` `\|vt` | 延音号 | 解析校验,不输出 |
| `\|vi-` `\|vi+` `\|rit` `\|rall` `\|accel` | 渐慢/渐快开始 | 解析校验,不输出 |
| `\|ve` `\|vr` `\|vb180` `\|vb` | 变速结束/回原速/至指定速 | 解析校验,不输出 |
| `\|ppp`…`\|fff`,`\|s1`~`\|s8` | 力度 | 解析校验,不输出(缺省力度 4 仅作校验) |
| `\|si-` `\|si+` `\|cresc` `\|dim` `<   <` `>   >` `\|sr` `\|pr` `\|sb` `\|pb` `\|se` | 渐强/渐弱及变体(`sr/pr/sb/pb` 同义) | 解析校验,不输出 |
| `{D/200232}` | 和弦把位定义(吉他);后定义覆盖先定义;缺省=常用把位表 | 解析入库(供 ③/⑤ 类解析时查把位) |
| `&gt: {a=E2,b=A2,…}` | 吉他六弦调弦表(新文件头写法为 `&gt:`;字面即 "&gt" 四字符,原因:避免与力度 `>   >` 混淆) | 解析入库(供 ③ 类 a~f 字母消解) |

### 2.3 行头元数据(rule_0_2 §1.2)

以「组头元数据区结束后的两个 `||` 的结束点为分界」,此前均为行头元数据区,内容:

1. **乐器名**(主旋律行与 `[` 行不需要):中文与英文均可,用 `<` 分割,如 `Nylon Guitar<古典尼龙吉他`;伴奏行头同时给出结构 id:`(NGTR1:Nylon Guitar<…`。
2. **初始音区**:`*4`、`*5` 等(组 n;n 与八度的对应见 §13 待确认项 ①)。
3. **初始力度**:解析校验,不输出。
4. **特殊表记定义**(可在任意元数据区,作用域全局或本乐器),五类:
   - ① 瞬时表记(一个音符):`x~{135}`、`x～{一三五}`、`x～{CEG}` —— 括号内为同刻和弦(简谱数字/中文数字/绝对音名)。
   - ② 大于一个瞬时的表记:`x&"{358}/7/3-6-"` —— `_` 为空格替代,`/` 划分拍;`{358}` 为一拍柱式和弦,单独数字为一拍单音,`3-` 等为带变音的音级。
   - ③ 吉他节奏型:与 ② 相同,但内容使用小写 `a`~`f`(琴弦名,按 `&gt` 表消解;若该拍有活跃和弦,按把位表消解)。
   - ④ 特殊和弦定义:`{Cmy/300353}` —— 和弦名 → 指板把位,供 ③ 类查询。
   - ⑤ 任何和弦节奏型:`x&r"{358}7654345"` —— `r` 表示内容为**和弦相对音**(1,3,5,7,8,9 等为和弦内音;和弦外音用调内音);`(7/8)` 表示若 7 为和弦内音演奏七音、否则演奏八音;`[56]` 表示该拍时值平分;`@[6-7-3-]` 为标记变音(绝对变音);数字中可穿插 A~G 绝对音;字母/数字后 `+`/`-`/`～` 为升降还原。
   - 标记名规则:1~4 个大/小写字母;不可含 ABCDEFG;首尾不能是数字;**不可重复**。
   - 使用时在谱面中写 `x@|3-6-7-4+` 或 `x@|E-A-B-F+` 可临时施加绝对变音。

### 2.4 谱面记载规则(rule_0_2 §2)

1. 每个拍子占 4 字节(格);若一拍内等分数 > 5,该拍多于 4 字节。
2. 事件字符:
   - `1`~`8` 简谱数字(音级),`0` 从其位置起停止音符(休止至下一事件);
   - `(1+)` 括号标注同一音符(不补空格;时值 = 一拍);
   - `[123]` 括号内容占同一个单位(拍),内部 n 等分;`[~123]`(每个 `~` 多占一个单位)占两个单位内部 3 等分;
   - `{12}` 同一拍同刻双音(柱式和弦);
   - `%234` 把四个单位(格)三连音化(一拍三连音);`%%234` 把八个单位三连音化;`%12345`、`%1234543` 把八个单位 5/7 连音化;
   - 特殊表记名(①~⑤ 的标记名)本身是一个事件。
   - 总占用位置数必须 ≤ 拍子数;否则需要机动行。
3. 后缀标记(需要占用下一个四分之一拍的谱位;音符与标识前后各加 `_` 可防止占用):
   - `*` 从前面的音起升一个八度组;`.` 从后面的音起降一个八度组;`+` 升;`-` 降;`～` 还原。
4. **八度判断规则**:纯四度(含)以内默认连续(同一八度组);超出时取最近的八度(见 §7.5)。
5. 歌词行与曲行一对一;歌词中的 `-` 成为**延音线**;歌词行可用来放置力度符号(力度照旧不输出)。
6. 被 `(` 括起的谱行可以是柱式和弦伴奏行(吉他),按把位表解析。

### 2.5 机动行(rule_0_2 §三)

- 数字行与机动行(旋律区第 3、4 两行)随时可互换角色:哪行含事件字符即为数字行;第三(第 5)行为歌词行。
- 机动行内容从属于数字行:同一列上方的 `.`、`+` 等作用于数字行的音符(如 `.` 在上、`6` 在下 → 6 降一个八度组)。
- 机动行中多符号标记必须以 `|` 开头,作用位置 = 竖线所在列。
- **`=` 替换格式**:数字行写 `=`,`机动行` 同列写 `|内容` 代替之;`====` 连续等号只需在机动行标注起点与终点(可连续标注)。
- `|` 后紧跟若干 `_` 表示其内部时值划分与外部不同:`|__1234…` 把该四个空格代表的时值拆成 16 格;省略 `_` 则自动平分时值。
- 机动行内空格用 `/` 代替(空格用于区分两处内容)。
- 机动行还可出现:吉他节奏型定义、特殊和弦把位定义、一切 ≥ 一个瞬时的特殊表记。

---

## 三、输出格式规范(依据 musicxmlEg.xml)

### 3.1 文档骨架

与 `tymp.h` 中 `xmlHead` 完全一致(由 `initNewMusicXML` 直接写入):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC
  "-//Recordare//DTD MusicXML 4.0 Partwise//EN"
  "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="4.0">
```

`<part-list>` 在前,各 `<part>` 依次在后,最后 `</score-partwise>`。输出一律 UTF-8。

### 3.2 divisions 计算(全局唯一,对所有声部共用)

```
divisions = 4 × accu × L
其中 accu = 每拍格数(从 #timestamp 网格推断,如 "|   ." → 4;无网格行时默认 4)
      L    = 全曲所有「非整格时值」分母的最小公倍数
             分母集合:每个 % 连音(实际音数 n)、[..] 等分(n)、
             机动行 |_…_ 细分的格数等;无此类时 L=1
```

- 含义:一格 = `divisions×4/(拍号分母×accu)` tick。4/4 下格 = 1/16 四分音符;6/8 下格 = 1/8 四分音符。
- 例:测试用例含 3 连音、5 连音 → L=lcm(3,5)=15,divisions=4×4×15=240。

### 3.3 时值的 MusicXML 表示算法

任意事件时值(以「拍」为单位的有理数 d,四分音符为 1)按下述顺序判定(§6 中 `dur_to_type`):

1. 若 d = 4/T(T∈{16,8,4,2,1}):`type`=16th/eighth/quarter/half/whole,无点无 time-modification;
2. 否则若 d = 6/T(附点):`type`=T,`dot`=1;
3. 否则按连音表示:取最大的 T 使 4/T ≥ d(不够则 T=16);约分 d·T/4 = p/q,则
   `<time-modification><actual-notes>q</actual-notes><normal-notes>p</normal-notes>[<normal-type>T</normal-type>]</time-modification>`
   (当 T 恰等于本音 type 时省略 `normal-type`,与 musicxmlEg.xml 一致);
4. tick 数 = round(divisions × d),必须为整数(由 divisions 的 LCM 构造保证)。

### 3.4 小节模型与事件流

- 每小节宽度 = 拍号分子 × accu 格;数字行/机动行/歌词行的乐曲区按此宽度切分为小节。
- 音符时值 = 事件起点列 → 下一事件起点列(见 §7.2);跨小节者由 §7.4 拆分。
- 休止:由 `0` 起始,持续到下一事件;整小节休止 → `<rest measure="yes"/>`。
- 和弦 `{…}` 与特殊表记展开的同刻多音:第一个音写 duration,其余写 `<chord/>`(duration 相同)。

---

## 四、MusicXML 输出要素清单(一一详细)

> 本节是输出端的**完整要素规格**。每个要素给出:来源(tymp.h 字段/内部变量/规则)、输出时机、精确写法、与 musicxmlEg.xml 的对照。§4.6 列出**明确不输出**的要素及理由。

### 4.1 文档级要素

| 要素 | 来源 | 时机 | 精确写法 |
|------|------|------|----------|
| `<?xml version="1.0" encoding="UTF-8"?>` | `tymp.h` 之 `xmlHead` | 打开输出文件时 | 原样 |
| `<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" "http://www.musicxml.org/dtds/partwise.dtd">` | `xmlHead` | 同上 | 原样 |
| `<score-partwise version="4.0">` | `xmlHead` | 同上 | 原样 |
| `</score-partwise>` | — | 全部 part 写完后 | 原样 |

### 4.2 part-list 要素

| 要素 | 来源 | 时机 | 精确写法 |
|------|------|------|----------|
| `<part-list>` | — | 解析完 instruments 行与各组行头之后,写任何 `<part>` 之前 | 包住所有 `<score-part>` |
| `<score-part id="…">` | 声部 id:主旋律=`P0`;第 i 个和声行=`Pi`;伴奏=instruments 行对应结构 id(如 `NGTR1`) | 每个参与输出的声部一个 | `<score-part id="NGTR1">` |
| `<part-name>` | `Instrument.part_name_En`(instruments.h 原型表 base_name);旋律/和声无乐器 → `Melody`/`Harmony`(和声从第二个起 `Harmony 2`…) | 每个 score-part 内 | `<part-name>Nylon Guitar</part-name>` |
| `<score-instrument id="…-I1">` | 乐器 id = score_part_id + `-I1`(与 newfileMaker 的 instrument_id 约定一致) | 同上 | `<score-instrument id="NGTR1-I1">` |
| `<instrument-name>` | 同 part-name | 同上 | `<instrument-name>Nylon Guitar</instrument-name>` |
| `<midi-instrument id="…-I1">` | 同 score-instrument id | 同上 | 原样 |
| `<midi-channel>` | 按输出顺序 1,2,3,… 递增(musicxmlEg.xml 同此) | 每个声部一个 | `<midi-channel>1</midi-channel>` |
| `<midi-program>` | `InstrumentPrototype.gm_program`(仅 INS_PITCHED) | 同上 | `<midi-program>25</midi-program>` |

> 无音高打击(INS_UNPITCHED)与架子鼓(INS_DRUMKIT)声部:因 tymp.h 无 `<unpitched>` 对应字段,该行**跳过并告警**(§4.6-5)。

### 4.3 part / measure 级要素

| 要素 | 来源 | 时机 | 精确写法 |
|------|------|------|----------|
| `<part id="…">` | 同 score-part id | 每个声部一个 | `<part id="NGTR1">` |
| `<measure number="n">` | 全曲小节全局编号 1,2,3…(跨组连续) | 每个小节一个 | `<measure number="1">` |
| `<attributes>` | — | **该声部第 1 小节**,以及**调号或拍号发生变化的小节**(内容为完整 attributes) | 见下 |
| `<divisions>` | §3.2 的全局 divisions | 上述 attributes 内 | `<divisions>240</divisions>` |
| `<key>` | 当前 KeyState | 同上 | `<key><fifths>1</fifths><mode>major</mode></key>` |
| `<fifths>` | 调号等价五度圈数(§7.6) | 同上 | `<fifths>-3</fifths>` |
| `<mode>` | 大调 `major` / 小调 `minor`(`|=Dm` 等) | 同上 | `<mode>major</mode>` |
| `<time>` | 当前 TimeState(拍号) | 同上 | `<time><beats>4</beats><beat-type>4</beat-type></time>` |
| `<beats>`/`<beat-type>` | 拍号分子/分母 | 同上 | `<beats>6</beats><beat-type>8</beat-type>` |
| `<clef>` | 一律 G 谱号(有音高声部;musicxmlEg.xml 同) | 同上 | `<clef><sign>G</sign><line>2</line></clef>` |
| `<sign>`/`<line>` | 常量 G / 2 | 同上 | 同上 |

### 4.4 note 级要素(按 DTD 顺序输出)

> 每个 `<note>` 的固定输出顺序:①`<chord/>`(若有)②`<pitch>` 或 `<rest>` ③`<duration>` ④`<tie>`×0~2 ⑤`<voice>` ⑥`<type>` ⑦`<dot/>`(若有)⑧`<accidental>`(若有)⑨`<time-modification>`(若有)⑩`<notations>`(若有)⑪`<lyric>`(若有)。

| 要素 | 来源(tymp.h 字段) | 时机 | 精确写法 |
|------|-------------------|------|----------|
| `<chord/>` | `note_pitch_mxml.isChord == true` | 同刻和弦的第 2 个及以后各音;**在 `<pitch>` 之前** | `<chord/>` |
| `<rest/>` | `isRest == true` | 休止符 | `<rest/>` |
| `<rest measure="yes"/>` | `isRest` 且占满整小节(该小节无其他事件) | 整小节休止 | `<rest measure="yes"/>` |
| `<pitch>` | `!isRest` | 有音高音符 | 包住 step/alter/octave |
| `<step>` | `pitch.step`(A~G) | 同上 | `<step>F</step>` |
| `<alter>` | `pitch.alter` | **仅 alter ≠ 0 时输出**(musicxmlEg.xml 同) | `<alter>1</alter>` |
| `<octave>` | `pitch.octave`(1~7) | 同上 | `<octave>4</octave>` |
| `<duration>` | `duration`(tick 数,整数) | 每个音符(含休止、和弦各音) | `<duration>80</duration>` |
| `<tie type="start\|stop"/>` | `tie[0]`、`tie[1]`(TIE_START/TIE_END) | 每个延音线端点;跨小节中间片段 start 与 stop 各一个 | `<tie type="start"/>` |
| `<voice>` | `voice`(当前恒为 1) | 每个音符 | `<voice>1</voice>` |
| `<type>` | `type` 枚举 16/8/4/2/1 → `16th`/`eighth`/`quarter`/`half`/`whole` | 每个音符(含休止) | `<type>eighth</type>` |
| `<dot/>` | `isDot == true` | 附点时(单附点) | `<dot/>` |
| `<accidental>` | `accidental`(sharp/flat/natural) | 该音非当前调号调内音时(§7.7) | `<accidental>natural</accidental>` |
| `<time-modification>` | 连音(tuplet)音 | 每个连音音符(含休止符,若在连音内) | 见下 |
| `<actual-notes>` | `time_modification.actualNotes` | 同上 | `<actual-notes>3</actual-notes>` |
| `<normal-notes>` | `time_modification.normalNotes` | 同上 | `<normal-notes>2</normal-notes>` |
| `<normal-type>` | `time_modification.normalType` | **仅当 normalType ≠ 本音 type 时输出**(与 musicxmlEg.xml 省略风格一致) | `<normal-type>16th</normal-type>` |
| `<notations>` | `notations[2]` 非空 | 有 tied 或 tuplet 记号时 | 包住 tied/tuplet |
| `<tied type="start\|stop"/>` | `notations[i].notation_type == tied` 且 `type`=TIE_START/END | 延音线显示记号(与 `<tie>` 成对,musicxmlEg.xml 同) | `<tied type="start"/>` |
| `<tuplet type="start" number="n" bracket="no" show-number="actual" placement="above"/>` | `notations[i].notation_type == tuplet` 且 `type`=start;number 来自 `attribute2` | 每个连音组的**首音** | 原样(musicxmlEg.xml 同) |
| `<tuplet type="stop" number="n"/>` | tuplet 且 type=stop | 每个连音组的**末音** | 原样(musicxmlEg.xml 同) |
| `<lyric>` | `lyric.text != NULL` | 有歌词的音符(休止符不输出) | 包住 syllabic/text |
| `<syllabic>` | `lyric.syllabic`(SYL_SINGLE→`single`;BEGIN→`begin`;END→`end`;MIDDLE→`middle`;本方案全部为 `single`) | 同上 | `<syllabic>single</syllabic>` |
| `<text>` | `lyric.text`(UTF-8 原样) | 同上 | `<text>我</text>` |

### 4.5 与 tymp.h 字段的逐一映射(完备性检查)

| tymp.h 字段 | 输出 | 说明 |
|-------------|------|------|
| `note_pitch_mxml.isChord` | `<chord/>` | |
| `note_pitch_mxml.isRest` | `<rest/>` / `<rest measure="yes"/>` | |
| `note_pitch_mxml.pitch.step/alter/octave` | `<step>`/`<alter>`(≠0)/`<octave>` | |
| `note_pitch_mxml.duration` | `<duration>` | |
| `note_pitch_mxml.type` | `<type>` | 枚举 16/8/4/2/1 |
| `note_pitch_mxml.tie[0..1]` | `<tie type>` ×1~2 | |
| `note_pitch_mxml.voice` | `<voice>` | |
| `note_pitch_mxml.isDot` | `<dot/>` | |
| `note_pitch_mxml.accidental` | `<accidental>` | |
| `note_pitch_mxml.notations[0..1]` | `<notations>` 内 `<tied>`/`<tuplet>` | notation_name=tied→tied;=tuplet→tuplet(attribute2=组号) |
| `note_pitch_mxml.lyric.syllabic` | `<syllabic>` | |
| `note_pitch_mxml.lyric.text` | `<text>` | |

`note_tymp` 各字段的**来源**(解析阶段如何填):`solfa`(1~8 或 0=绝对音时另有字段)、`alter`(+/-/～/调内变音合成)、`octave`(音区消解后)、`dur`(格数)、`pos`(小节内格偏移)、`isChord`(`{…}`/同刻展开)、`lyc`(歌词文本)。`note_tymp` → `note_pitch_mxml` 的换算函数见 §6 之 `ev2mxml`。

### 4.6 明确不输出的要素(及理由)

| 要素 | 来源(规范/示例) | 不输出的理由 |
|------|-----------------|--------------|
| `<direction>` / `<direction-type>` / `<metronome>` / `<sound tempo>` | 速度记号 `\|vb120` 等;musicxmlEg.xml 有 | tymp.h 无速度字段(C1);解析校验后丢弃 |
| `<dynamics>`(`<f/>` 等) / `<wedge>`(crescendo) | 力度/渐强记号;musicxmlEg.xml 有 | tymp.h 无力度字段(C1) |
| `<words>`(Ritardando/A tempo) | 变速记号 | 同上 |
| `<harmony>` | 和弦行(C/Am/Bmin7) | tymp.h 无 harmony 结构;和弦行仅参与 ⑤ 类消解 |
| `<beam>` | musicxmlEg.xml 三连音处有 | tymp.h 无 beam 字段(C2);可选扩展(§13) |
| `<unpitched>` / `<midi-unpitched>` | 无音高打击乐器(instruments.h INS_UNPITCHED) | tymp.h 只有 `pitch`,无 unpitched 结构;整行跳过并告警 |
| `<work>` / `<identification>` | 标题/作者 | musicxmlEg.xml 无;不输出(扩展点) |
| `<stem>`、`<staff>`、`<print>`、`<backup>`、`<forward>`、`<transpose>`、`<slur>`、`<fermata>`、`<figure-bass>`、`<barline>` | 均未见于 musicxmlEg.xml 或 tymp.h | C2 |
| 移调乐器 `<transpose>`(Bb 单簧管等) | instruments.h 注释提及 | 不实现(扩展点) |

---

## 五、文件划分与内部数据结构

```
tymp.h              ← 不修改(输出结构、xmlHead、UTF-8 工具)
instruments.h       ← 不修改(乐器原型表)
tymp_internal.h     ← 新增:解析器内部类型(见下)
tympUtils.c         ← 新增实现:工具函数(字符串/乐理/时值/拆分)
tymp2musicXML.c     ← 扩展现有骨架:解析管线 + 转换 + 输出 + main
```

### 5.1 tymp_internal.h 内部类型

```c
typedef struct { int num, den; } Fraction;            /* 通用有理数(以拍为单位,1拍=四分音符) */

typedef struct { char root; int alter; bool is_minor; bool chord_no_follow;
                 int fifths; } KeyState;              /* |=C / |=#G~ / |-7-3-6 消解后 */

typedef struct { int num, den; } TimeState;           /* 拍号 */

typedef struct {                                    /* 吉他调弦 &gt: {a=E2,...} */
    char str[6][4];      /* a~f 六弦音名+八度,"E2"… */
    bool valid;
} GuitarTuning;

typedef struct SpecialMarkDef {                      /* ①~⑤ 特殊表记定义 */
    char name[8];        /* 标记名(1~4 字母) */
    int  kind;           /* 1..5 */
    bool chord_relative; /* ⑤:r */
    char *content;       /* 引号内原文 */
    struct SpecialMarkDef *next;
} SpecialMarkDef;

typedef struct ChordSym {                            /* 和弦行条目 */
    int  col;            /* 起点列(格) */
    char root;           /* 根音 A~G */
    int  root_alter;     /* #b */
    bool is_minor, has_seventh, is_maj7, is_dim;     /* 品质位 */
    struct ChordSym *next;
} ChordSym;

typedef struct Event {                               /* 解析后的原子事件(一个音符/休止) */
    bool  is_rest;
    bool  is_abs;         /* 绝对音(A~G 或吉他弦) */
    char  abs_step;       /* 绝对音音名 */
    int   degree;         /* 1~8 简谱音级(绝对音时=0) */
    int   alter;          /* -2..2: + - ～ 与调内合成 */
    int   octave_shift;   /* * . 累计(±n 组) */
    Fraction start, dur;  /* 小节内起点/时值(拍) */
    bool  is_chord;       /* 与前一事件同刻 */
    char *lyric;          /* 歌词文本;NULL 无 */
    bool  tie_lyric;      /* 歌词 "-" → 延音线起点 */
    bool  tuplet;         /* 处于连音组内 */
    time_modification tm; /* 连音参数 */
    int   tuplet_no;      /* 连音组号(全局递增) */
    int   tuplet_pos;     /* 0=组内其他 1=首 2=末 */
    struct Event *next;
} Event;

typedef struct Measure {  Event *first; int number; TimeState time; KeyState key; } Measure;
typedef struct Part   {  char *id; char *name; Instrument inst; Measure **ms; int n; } Part;
typedef struct Score  {  Part **parts; int nparts; int divisions; int accu;
                         char *title, *author; } Score;
```

---

## 六、逐文件、逐函数实现方案

### 6.1 tympUtils.c(工具层,纯函数)

| 函数 | 签名 | 职责与算法要点 |
|------|------|----------------|
| `strip_crlf` | `void strip_crlf(char *line)` | 去除行尾 `\r\n`(兼容 Windows 文件) |
| `strip_trailing_spaces` | `void strip_trailing_spaces(char *line)` | 去除行尾空格;.tymp 模板每行有约 10000 个填充空格,必须先去尾再解析 |
| `read_all_lines` | `char **read_all_lines(const char *path, int *n)` | 整文件读入为行数组;行缓冲 ≥ 32KB(容纳 MAX_NUM=10000 填充);失败返回 NULL |
| `line_is_timestamp` | `int line_is_timestamp(const char *s)` | 判 `^\s*#timestamp` → 该行跳过 |
| `find_metadata_end` | `int find_metadata_end(const char *line)` | 找第一个 `\|\|` 的列;无则回退:扫描行头已知元数据 token(见 6.2 `parse_line_head`)后推定 |
| `find_next_nonspace` / `find_prev_nonspace` | `int …(const char*, int from, int limit)` | 乐曲区内找下一个/上一个非空字符列 |
| `scan_meta_mark` | `int scan_meta_mark(const char *line, int from, int to, char *buf, int *col)` | 从 `\|` 起读一个完整元数据记号(如 `\|=G`、`\|vb120`、`\|-7-3-6`、`&gt: {…}`、`{D/200232}`),返回记号文本与列位;组头/组末/机动行共用 |
| `parse_key_spec` | `KeyState parse_key_spec(const char *tok, const KeyState *prev)` | `\|=C`/`\|=#G`/`\|=Dm`/`\|=#G~` → root/alter/minor/chord_no_follow;`\|-7-3-6` → 尝试等价标准调号(降7、3、6度 = bB,bE,bA = Eb 大调 fifths=-3),不等价时记「逐音变音表」并 fifths=0 |
| `key_to_fifths` | `int key_to_fifths(char root, int alter, bool minor)` | C0 D2 E4 F-1 G1 A3 B5 + alter×7;与 `parse_key_spec` 内部复用 |
| `parse_time_spec` | `TimeState parse_time_spec(const char *tok)` | `\|6/8` → {6,8};非法 → 沿用上一组并告警 |
| `parse_tempo_mark` | `int parse_tempo_mark(const char *tok)` | 识别 `\|vb<num>`、`\|vi[+-]`、`\|rit`/`\|rall`/`\|accel`、`\|ve`、`\|vr`、`\|vb`、`\|。`、`\|vt`;**只做合法性校验,返回值供告警,不产出** |
| `parse_dynamic_mark` | `int parse_dynamic_mark(const char *tok)` | 识别 `\|ppp`…`\|fff`、`\|s1`~`\|s8`、`\|si[+-]`、`\|cresc`/`\|dim`、`<  <`/`>  >`(机动行内)、`\|sr`/`\|pr`/`\|sb`/`\|pb`(同义)、`\|se`;**只校验不产出** |
| `parse_guitar_tuning` | `GuitarTuning parse_guitar_tuning(const char *tok)` | `&gt: {a=E2,b=A2,c=D3,d=G3,e=B3,f=E3}` → 六弦表 |
| `parse_chord_voicing` | `int parse_chord_voicing(const char *tok, VoicingTable *vt)` | `{D/200232}` → 把位表条目(后定义覆盖) |
| `resolve_degree_pitch` | `pitch resolve_degree_pitch(pitch anchor, const KeyState *k, int degree, int alter, int shift)` | 简谱音级→音名+八度。核心八度规则见 §7.5;`shift` 为 `*`/`.` 累计组偏移 |
| `degree_to_step` | `void degree_to_step(const KeyState *k, int degree, int alter, char *step, int *alter_out)` | 1→调根音名,按大/小调音阶走 2 全 3 半…;再叠加 alter;返回最终 step 与 alter |
| `step_is_diatonic` | `bool step_is_diatonic(int fifths, char step, int alter)` | 由五度圈数判断 (step,alter) 是否属于当前调号 → 决定是否输出 `<accidental>` |
| `frac_*` | `Fraction frac_add/sub/mul/div, frac_reduce(Fraction)` | 有理数四则与约分 |
| `dur_to_type` | `void dur_to_type(Fraction d, notetype *type, bool *dot, time_modification *tm)` | §3.3 三步判定算法 |
| `compute_divisions` | `int compute_divisions(Event *all, int accu)` | §3.2:收集全部事件时值/连音分母 → L → 4·accu·L |
| `split_cross_measure` | `int split_cross_measure(Event *ev, int measure_rest_slots, int slot_tick, note_pitch_mxml **out, int *n_out)` | §7.4:把一个跨小节事件拆成 2~n 个 note_pitch_mxml,自动注入 tie 端点(start / start+stop / stop) |
| `dup_str` | `char *dup_str(const char *s)` | 字符串复制(malloc+memcpy) |
| `utf8_display_width` | `int utf8_display_width(const char *s)` | 显示宽度:CJK 计 2 列、ASCII 计 1(复用 tymp.h 的 UTF-8 解码函数);歌词分格用 |

### 6.2 tymp2musicXML.c(主逻辑)

**已有(保留):**

| 函数 | 说明 |
|------|------|
| `FILE *initNewMusicXML(char *filename)` | 已有实现:补 `.musicxml` 后缀、写 `xmlHead`;**保持原样** |

**待实现 —— 输出层:**

| 函数 | 签名 | 职责与算法要点 |
|------|------|----------------|
| `makeInstrumentPart` | `int makeInstrumentPart(FILE *fp, const Instrument *inst, int channel)` | 写一个 `<score-part>` 块(§4.2 各要素);按 `inst.prototype.kind` 分支:INS_PITCHED → midi-program;INS_UNPITCHED/INS_DRUMKIT → 返回 0 并置「跳过」标志(调用方不加入 part-list) |
| `getInstrumentList` | `int getInstrumentList(const char *instr_line, Instrument *out, int max, int *count)` | 解析 `instruments:NGTR1 SD1`:按空白分词;对每个结构 id **剥离末尾数字**得 base_id,在 `InstrumentPrototypeList` 中精确匹配;构造 Instrument(score_part_id=原 id、instrument_id=id+"-I1"、part_name_En/part_name_Zh=原型表、prototype);匹配失败回退 INST 原型 |
| `write_part_list` | `int write_part_list(FILE *fp, const Score *s)` | 按 P0(旋律)/P1..(和声)/伴奏顺序输出 `<part-list>`;跳过标志的乐器不输出 |
| `write_pitch` | `int write_pitch(FILE *fp, pitch p)` | `<pitch><step>..</step>[<alter>n</alter>]<octave>..</octave></pitch>`;alter=0 省略 |
| `write_note` | `int write_note(FILE *fp, const note_pitch_mxml *n, const KeyState *key)` | §4.4 固定顺序输出一个 `<note>`;要点:chord 在 pitch 前;rest 时跳过 pitch/accidental/lyric;`tie[0..1]` 逐一输出 `<tie>`;`accidental` 仅在非调内时输出(sharp/flat/natural);notations 中 tied 与 tuplet(start 带 bracket/no/show-number/placement,stop 带 number)按 tymp.h 字段逐项输出 |
| `write_attributes` | `int write_attributes(FILE *fp, int divisions, const KeyState *k, const TimeState *t)` | `<attributes>` 内含 divisions/key(fifths+mode)/time/clef(G/2) |
| `write_measure` | `int write_measure(FILE *fp, const Measure *m, int no, const KeyState *prev_k, const TimeState *prev_t, int divisions)` | `<measure number>`;若 no==1 或 key/time 有变 → write_attributes;依次 write_note;`</measure>` |
| `write_part` | `int write_part(FILE *fp, const Part *p, int divisions)` | `<part id>` + 逐小节 write_measure(携带上一小节 key/time 状态) |
| `write_score` | `int write_score(FILE *fp, const Score *s)` | part-list + 全部 part + 结束标签 |

**待实现 —— 解析层:**

| 函数 | 签名 | 职责与算法要点 |
|------|------|----------------|
| `parse_header` | `int parse_header(char **lines, int n, char *title, char *author, char *instr_line)` | 第 1 行按 `" by "` 切标题/作者(找不到分隔符则整行为标题);第 2 行取 `instruments:` 后内容;返回乐器行号 |
| `parse_group_head` | `int parse_group_head(const char *line, GroupState *g)` | 对 `{` 后的元数据逐个 `scan_meta_mark`:拍号/调号/速度/力度/`&gt:`/把位定义;`{` 后无内容 → 沿用上一组(§2.1-4) |
| `parse_group_tail` | `int parse_group_tail(const char *line, GroupState *g)` | `}` 之后的元数据:力度/速度(全局生效类) —— 解析校验,不产出 |
| `parse_line_head` | `int parse_line_head(const char *line, LineInfo *li, SpecialMarkDef **defs)` | 行头元数据:乐器名(按 `<` 切中/英文两段)、`*4` 音区、初始力度、①~⑤ 定义(`name~{…}`/`name&"…"`/`name&r"…"` 三种形态,含标记名合法性校验:1~4 字母、不含 ABCDEFG、首尾非数字、不重复) |
| `resolve_chords_line` | `int resolve_chords_line(const char *music_area, ChordSym **out)` | 和弦行解析:每个和弦名(根音字母+可选 #b+品质 m/min/maj7/7/dim…)记其列;一个和弦符号从其列生效至下一个和弦符号 |
| `scan_digit_line` | `int scan_digit_line(const char *music_area, const LineInfo *li, const GroupState *g, Event **out)` | **主扫描器**(§7.1):逐列识别 数字/0/`(n)`/`[~..123]`/`{..}`/`%..`/特殊表记名/前后缀标记(`* . + - ～`)与 `_`(等价空格);生成 Event 链(时值=下一事件列−起点列,见 §7.2);`%` 类与 `[…]` 类按「声明格数」定长(与书写宽度无关) |
| `scan_aux_line` | `int scan_aux_line(const char *music_area, AuxMark **out)` | 机动行扫描:同一列上方的 `.`/`+`/`-`/`～`(作用于数字行同列音符);`\|` 起始的多符号段(按列对齐);`\|_…_` 细分格段;`=` 对应的 `\|段` 替换 |
| `apply_aux_marks` | `int apply_aux_marks(Event **evs, AuxMark *marks, const GroupState *g)` | 把机动行标记合并进数字行事件:同列后缀 → 修改对应事件的 alter/octave_shift;`\|段` → 替换 `=` 事件(段内再跑一次 scan_digit_line,时值按段长);`\|` 力度/速度段 → 校验丢弃 |
| `expand_special_mark` | `int expand_special_mark(const SpecialMarkDef *def, const ChordSym *chord, const GroupState *g, pitch anchor, Event **out)` | §8:按 ①~⑤ 展开为 Event 链 |
| `attach_lyrics` | `int attach_lyrics(Event *evs, const char *lyric_area, int accu)` | 歌词行按拍分格(每拍 accu 显示列,汉字 2 列);每格取首个非空字符附着到该拍第一个事件;`-` → 置 tie_lyric(不生成歌词文本);休止/连音续片段不附着 |
| `events_to_measures` | `int events_to_measures(Event *flat, const GroupState *g, Measure **out, int *n)` | 按小节宽(拍数×accu)切分;行内无下一事件时音符持至本小节末;跨小节音符拆入两小节并标记 tie;每小节结尾按剩余格补休止(整小节 → measure="yes") |
| `convert_line` | `int convert_line(const LineRole role, char **lines, const GroupState *g, Part *p)` | 一行(声部)的完整转换:行头 → 数字行/机动行/歌词行识别 → 扫描 → 事件 → 小节;跨组时携带 Part 的「未完成音符(延音线待续)」状态 |
| `convert_group` | `int convert_group(char **lines, int from, int to, const GroupState *prev, Score *s)` | 一组(乐段)的调度:识别各行的角色(组头/和弦/数字/机动/歌词/`[`/`(`/`}`/时间戳),依次调用上述函数;伴奏行按 instruments 行顺序挂接 |
| `ev2mxml` | `int ev2mxml(const Event *e, const KeyState *k, int slot_tick, note_pitch_mxml *out)` | 事件 → tymp.h 结构:时值→duration(ticks)/type/isDot/tm;音级+变音+八度→pitch;tie_lyric/跨小节 → tie[];连音 → notations.tuplet(attribute2=组号字符串);歌词 → lyric.syllabic=SYL_SINGLE、text |
| `tymp2musicxml` | `int tymp2musicxml(const char *src, const char *dst)` | 总管线:read_all_lines → parse_header → 逐组 convert_group → compute_divisions → 逐声部 ev2mxml+split_cross_measure 填 note_pitch_mxml → write_score |
| `main` | `int main(int argc, char **argv)` | 用法 `tymp2musicxml 输入.tymp [输出.musicxml]`(缺省输出=输入名换后缀);返回 0/非 0 |

---

## 七、核心算法细则

### 7.1 数字行主扫描器(状态机)

按列扫描乐曲区,状态:`空闲 / 音符持时中 / 连音组中 / 特殊表记名匹配中`。

- **事件起点字符**:`1..8`(单音)、`0`(休止)、`(`(n 型单音,时值固定一拍)、`[`(`[123]`/`[~123]` 定长一拍/两拍 n 等分)、`{`(柱式和弦,一拍,内部 n 个同刻音)、`%`(`%234`/`%%234`/`%12345`/`%1234543`,定长 4/8/8/8 格)、特殊表记名首字母(从定义表查得最长匹配)、前缀 `.`(其后数字与该 `.` 同事件)。
- **后缀标记** `* + - ～`:归属前一事件,不产生新事件;效果:octave_shift±1、alter±1/归 0。`_` 等价空格。
- **事件时值**:起点列 → 下一事件起点列(格数);特殊表记与 `%`/`[…]` 类用「声明格数」;**行(乐曲区)内无下一事件 → 持续到该行覆盖的最后一小节末(行末)**;若行末小节之后(同组内后续小节或下一组)无事件,则跨越小节线/组边界延续,直到遇到事件或 `0` —— 这正是跨小节长音符的来源;**中途终止必须写 `0`**(见 §7.8)。每组覆盖的小节数 = 组内最长乐曲行长度 ÷ (拍数×accu),向上取整;短行未写部分视为空白(对未终止音符 = 持时,否则 = 休止)。
- 后缀标记列计入其所属事件的持有时值;前缀 `.` 列计入其后事件。
- 事件起点格与其时值相加后不得超过拍子数×accu(rule_0_2 硬性要求,违反告警并截断)。

### 7.2 机动行合并

1. 旋律区两音乐行:含事件字符多者为数字行(两者都有事件时,取事件起始列更早者;逐小节独立判定);
2. 机动行同列后缀标记 → 修改数字行该列事件的 alter/octave_shift;
3. `|段`:从 `|` 列起为一段;数字行同列若是 `=` → 用段内容替换(段内按 `scan_digit_line` 重新扫描,时值 = `=` 事件原时值);`====` 区间段整体替换(段内容平分区间);
4. `|_…_` 段:下划线数量 = 细分格数(如 `|__` → 该 4 格拆成 16 格),内部字符按新格距扫描;
5. `|` 段内容是力度/速度记号 → 校验后丢弃。

### 7.3 和弦行 → ChordSym

每个和弦符号记列 → 根音(A~G,可带 #/b)→ 品质(m/min/7/maj7/dim/sus…)。供 ⑤ 类展开:该拍的和弦音级消解。`|=…~` 时和弦行不跟随调号转调。

### 7.4 跨小节拆分与延音线(重点)

对每个跨小节事件:

1. 在事件流层面直接按小节切分:片段 i 的时值 = 到小节线为止的剩余格数;
2. 拆分出的 2~n 个片段共享同一音高;
3. 注入延音线端点:
   - 首片段:`tie[0]=TIE_START`;
   - 中间整小节片段:`tie[0]=TIE_START, tie[1]=TIE_END`;
   - 末片段:`tie[1]=TIE_END`;
4. 每个端点同时写 `<tie>` 与 `<notations><tied>`(musicxmlEg.xml 的双写风格);
5. **歌词 `-` 延音线**:歌词 `-` 附着于事件 X → X 加 `tie[0]=TIE_START`,其后第一个有音高事件加 `tie[1]=TIE_END`;若两音音高不同,告警并丢弃该延音线(rule 规定 `-` 即延音线,非同音无意义);
6. 跨小节休止同样拆分,但**不**产生 tie;
7. 拆分后每片段时长均须整除 tick(由 divisions 的 LCM 构造保证);
8. 延音线状态作为声部上下文跨组携带(测试用例中 G4 从第 1 组 m1 延续到第 2 组 m3)。

### 7.5 简谱八度消解

- 行/组初始锚点:行头 `*n` → 该行调根音 1 = MIDI 八度 n(C 大调下);`[` 行无 `*n` → 继承主旋律 `*n`;`#` 开头的和声行 → 锚点 = 主旋律**首音实际音高**(待确认②)。
- 逐音推进:当前锚点为上一事件的最终音(和弦取其中最后一个音的八度);下一个音级按 §7.5.1 消解;`*`/`.` 在消解结果上 ±1 个八度组;绝对音(A~G/吉他弦)直接定音并成为新锚点。
- **纯四度(含)以内默认连续**:候选音距锚点 ≤ 纯四度(5 个半音)时取同向连续音;超出时取最近八度;两侧均在四度内时取上行(连续进行优先)。(依据 rule_0_2「纯四度(含)以内,默认连续,无需升降八度」。)

### 7.6 调号消解

- `|=C` → fifths=0;`|=G`→1;`|=D`→2;`|=A`→3;`|=E`→4;`|=B`→5;`|=#F`→6;`|=#C`→7;`|=F`→-1;`|=bB`→-2;`|=bE`→-3;`|=bA`→-4;`|=bD`→-5;`|=bG`→-6;`|=bC`→-7;小调(m)mode=minor。
- `|-7-3-6` 型:按变音集合与标准调号比对,能匹配则等价;不能匹配(如只降 7 度)则 fifths=0 并以逐音 `<accidental>` 表达。
- 简谱音级→音名:1=根音;2/3/4/5/6/7 按大(2 全 3 全 4 半…)或小调音阶步进;`+`/`-`/`～` 在结果上叠加变音。

### 7.7 变音与 `<accidental>` 输出

- `pitch.alter` = 音级消解 + 后缀变音 + 调外变音的合成结果;
- `<alter>` 仅 alter≠0 时输出;
- `<accidental>`:当 (step,alter) 非当前调号调内音时输出 `sharp`(alter=1)/`flat`(-1)/`natural`(0,如 G 大调中的 F♮);
- 调内自然音不输出 accidental(musicxmlEg.xml 同)。

### 7.8 休止与整小节休止

- `0` 起始的休止持续到下一事件;相邻休止自然合并;
- 某小节所有事件恰为一个占满全小节的休止 → `<rest measure="yes"/>` + duration=小节 tick 数 + type=whole(与 musicxmlEg.xml 一致);
- 其他休止:duration=实际 tick,type 由 §3.3 计算;
- 装饰格(前缀标记所在列之外的静默)**不**产生休止元素(标记列已计入相邻事件时值)。

---

## 八、特殊表记展开细则(rule_0_2 §1.2-d)

| 类 | 定义形态 | 展开规则 |
|----|----------|----------|
| ① 瞬时 | `x~{135}` / `x～{一三五}` / `x～{CEG}` | 展开为一个一拍事件,内部 n 个同刻音:数字=简谱音级(按当前调);中文数字=同简谱;字母=绝对音。事件时值 = 一拍 |
| ② 多瞬时 | `x&"{358}/7/3-6-"` | 按 `/` 分拍;`{…}` = 该拍柱式和弦(音级按调内);单个数字 = 该拍单音;`3-` 等 = 带变音音级;`_` = 空格替代。总时值 = 拍数 |
| ③ 吉他节奏型 | `x&"a/c/e/a"` | 同 ② 的分拍,但 `a~f` = 琴弦:该拍有活跃和弦 → 按把位表(④/`{D/200232}`/默认常用把位表)取弦上音;无和弦 → 按 `&gt:` 调弦表空弦音 |
| ④ 特殊和弦定义 | `{Cmy/300353}` | 不产生事件;登记 和弦名→把位,供 ③ 查询 |
| ⑤ 和弦节奏型 | `x&r"{358}7654345"` | 数字为**和弦相对音**:1,3,5,7,8,9… = 和弦内音(根音/三音/五音/七音/八度根音/九音…,按和弦符号品质:大/小/七等);和弦外音(2,4,6…) = 调内音;穿插 A~G = 绝对音;`(7/8)` = 7 为和弦内音则奏七音,否则奏八度根音;`[56]` = 该拍平分两音;`@[6-7-3-]` = 对绝对音名施加固定变音;使用处 `x@|3-6-7-4+` = 临时绝对变音;`+`/`-`/`～` 可跟在数字或字母后。**同一节奏组内和弦切换 → 节奏组重新开始**(仅 ⑤ 类)。和弦根音八度:以当前锚点最近原则 |
| 通用 | 谱面写 `x` 或 `x@|…` | 标记名占位一事件;时值 = ①类一拍,②③⑤类 = 声明拍数;标记名不可重复、不含 ABCDEFG、首尾非数字 |

---

## 九、错误处理与容错

- 两级诊断:`警告`(可恢复,继续解析)与`错误`(中止)。全部写入 stderr,格式 `行号: 类别: 信息`。
- 典型警告:未识别记号(按字面跳过)、特殊表记名未定义、`=` 无对应机动行段、总占用格数超过拍子数(截断)、延音线两端音高不同(丢弃)、无音高打击声部(整行跳过)、拍号缺省(沿用)、组头缺失(沿用上一组)。
- 典型错误:文件不存在/空、乐器行缺失、`{`/`}` 不配对、曲首无拍号、内存分配失败。
- 编码:输入输出均为 UTF-8;非法 UTF-8 字节按单字节容错处理。

---

## 十、测试用例(覆盖全部功能)

> 设计意图:一次覆盖 —— 文件头与 instruments 行;part-list 三种来源(旋律/和声/伴奏乐器+无音高跳过);4/4 与 6/8;|=C 与 |=G;单音、八分、附点、休止、整小节休止、全音符;`{13}` 柱式和弦与特殊表记展开的柱式和弦(isChord);`%234` 一拍三连音、`%12345` 五连音、`[123]` 一拍三等分、`%654`(机动行 `=` 替换);`*`/`.`/`+`/`～` 四种后缀;`(5)` 括号单音;`#` 和声行;歌词与歌词 `-` 延音线;**跨小节音符(一个跨 1 条小节线、一个跨 2 条小节线,验证 tie 的 start / start+stop / stop 三种形态)**;特殊表记 ①②③⑤ 四类定义与使用;和弦行(含小节中途换和弦、⑤ 类重启规则);组末力度(解析不输出);第二组拍号/调号变化与 attributes 重输出;无音高打击行跳过。
>
> 格式说明:各行 `||` 前为元数据区,之后为乐曲区;乐曲区列号从 0 起,4/4 每小节 16 格、6/8 每小节 24 格;`||` 列严格对齐(此处统一在第 84 字节处)。

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

> 说明:
> - 第 1 组(4/4,|=C):旋律 m1=`1`(我) `2`(`-` 延音线) `%234`(爱) `5`(你,跨小节起点);m2=`5` 的延续(tie stop) `4+`(晚,升F) `1`(风) `3` `ch`(特殊表记① → C4,E4,G4 柱式和弦)。和声行(带 `#`):`1*`(升八度 C5) `.6`(降八度 A3) ×2,行末 m2 的 `0` 使 A3 停在小节线、m2 成为整小节休止(rest measure="yes")。和声2行:`1`(C4) `1*`(C5) `5`(G4,行内无后续事件 → 持至行末,即跨 2 条小节线:第 1 组 m2 全小节为中间片段 start+stop)。伴奏行 m1=`ar`(②类:{135}→C4,E4,G4 柱式和弦 + 1→C4) `pa`(⑤类,Am 和弦相对音:1,3,5→A3,C4,E4 + 1→A3);m2=`{13}`(C4+E4) `%12345`(五连音 C4~G4) `0`(四分休止)。组末 `|s5` 只解析不输出。SD1 为无音高打击 → 整行跳过并告警。
> - 第 2 组(6/8,|=G):旋律 `1 2 3`(G4,A4,B4 八分) `=`(机动行 `|%654` 替换为 16 分三连音 E5,D5,C5) `5`(D5) `7~`(还原 F♮5 → `<accidental>natural</accidental>`)。和声行 `(5)` → D4 附点二分(整小节,isDot)。和声2行 `0` 终止跨组延音线(G4 的 stop 片段)+ 二分休止。伴奏行 `gt`(③类,空弦 a/c/e/a → E2,D3,B3,E2) `0`(八分休止) `[123]`(一拍三等分 G2,A2,B2)。
> - 期待行为:tie 三形态齐全(G4-旋律:start/stop;G4-和声2:start / start+stop / stop);歌词 `-` 生成 D4→D4 延音线;attributes 在第 3 小节因 |=G、6/8 重输出;divisions=4×4×lcm(3,5)=240。

---

## 十一、预期输出(musicxml)

> 由 §10 的测试用例按 §4 规格生成。逐音符时值(divisions=240):4/4 下 格=60、四分=240、八分=120、附点八分=180、三连八分=80、五连八分=96、全音符=960;6/8 下 格=30、八分(一拍)=120、16 分三连=40、附点二分=720。

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

1. 每小节各声部 duration 之和 = 小节 tick 数(4/4=960、6/8=720);
2. 每个 `<tie type="start">` 必有后配 `<tie type="stop">`(同音高);G4 和声2 的中间小节同时含 start 与 stop;
3. 每个 tuplet start 有同 number 的 stop;number 按声部内出现顺序递增;
4. 调号变化、拍号变化的小节重新输出 `<attributes>`;
5. `<chord/>` 出现于 `<pitch>` 之前;仅和弦第 2 音起;
6. `divisions=240`(全曲统一),所有 duration 为整数。

---

## 十二、实现里程碑

| 里程碑 | 内容 | 验收 |
|--------|------|------|
| M1 | tymp_internal.h + tympUtils.c 基础(字符串/有理数/调号/时值表示/divisions) | 单测通过 |
| M2 | 文件读入、组/行识别、组头行头元数据解析 | 能打印结构树 |
| M3 | 数字行主扫描器 + 机动行合并 + 特殊表记①~⑤展开 | 事件流正确 |
| M4 | 事件 → 小节切分 + 跨小节拆分 + 延音线 + 歌词附着 | 事件→note_tymp 正确 |
| M5 | ev2mxml + 输出层(part-list/attributes/note) | 输出格式与 musicxmlEg.xml 一致 |
| M6 | 全管线 + main + 错误处理 | 通过 §10/§11 测试用例 |
| M7 | 以 part-IV.txt 模板手工填入的真实乐段回归 | 与预期乐理一致 |

---

## 十三、待确认决策清单

1. **组 n 与八度的对应**:本方案约定 `*n` → 该行调根音 1 = MIDI 八度 n(C 大调下 `*4` → 1=C4 中央 C)。若约定不同,只改 `resolve_degree_pitch` 一处。
2. **`#` 的语义**:本方案按「和声行初始锚点 = 主旋律首音实际音高」处理;若实际语义为「首音与主旋律首音同音」,需在 §7.5 调整。
3. **力度/速度/渐强渐弱/延音号**:按 C1 约束解析不输出;若日后 tymp.h 增加 direction/dynamics 字段,输出位置与 musicxmlEg.xml 一致(`<direction>` 置于小节内对应位置)。
4. **和弦行**:不输出 `<harmony>`;仅参与 ⑤ 类消解。
5. **无音高打击/架子鼓**:跳过并告警;若日后 tymp.h 增加 unpitched 字段,按 instruments.h 的 `unpitched` 键位输出 `<unpitched>`。
6. **`<beam>`**:musicxmlEg.xml 给三连音加 beam 但 tymp.h 无字段;作为可选扩展(对 16th/eighth 短音符按组推断 begin/continue/end)。
7. **歌词 `<lyric>`**:tymp.h 有 lyric 字段故输出(示例中无);syllabic 全部 single。
8. **`[~123]` 等多拍括号**:按 rule_0_2 括号描述允许出现在数字行;机动行的自由分拍(2 拍三连音等)经 `|段` 处理。
9. **`(n)` 时值**:约定 = 一拍(至下一事件或小节末,不补空格)。
10. **标题/作者**:不输出 `<work>`(扩展点)。

---

## 附录:rule_0_3.txt 更新补充(v0.3 与 v0.3.1)

> 本节依据**最新版 rule_0_3.txt** 追加。上文正文为 rule_0_2 时代的首版内容,未回改;权威规则以 `rule.md` 为准(其已全部吸收本节内容)。

### 附一、v0.3 的 12 条改动(摘要)

1. 机动行归属:机动行一般仅在数字行上方紧邻一格;两个数字行可紧邻——只有"单独符号"与 `|` 开头段落算机动行,其余数字归其声部行。
2. `\` 下指:属"在下的机动行"的内容必须在 `\` 之后;单独符号须夹于 `\…\` 之间;反斜杠不可省略。方向语义(作者确认):`|` = 上方指、服务其下面一行;`\` = 下方指、服务其上面一行。
3. 固定变音改版 `|@[7-3-6-]`(C 调 `|@[0]`),`|` 可由 `\` 代;可在数字行内增减。旧 `|-7-3-6` 兼容。
4. 伴奏排列灵活化:行数不定、按内容动态判定;可借邻行作机动行(最多同时 2 个);行首无乐器定义视为钢琴;简洁定义 `*#1:Inst#1<Instrument1$1`(各部分均可省略,`$1` 缺省先搜 Inst#1 再填钢琴;谱内就地 `*Vln|` 接正文);ID 独一化责任移交解析器。
5. 和声/主旋律行数不再限制为 3;歌词在数字行正下方即可识别、动态;歌词分隔符 `*l""`/`*l|""`(上)/`*l\""`(下),引号内文字即歌词。
6. 参数化定义 `X(V)&r"V_{35}_"`:调用点 V 的绝对唱名文本代入。
7. 特殊表记自动重复:>1 单位自动重复;r 和弦源每换和弦循环重头;=1 单位一次性。
8. 数据结构实装歌名作者、力度、速度、变速、和弦行、无音高打击乐器;鼓组→鼓谱,非鼓组任何数字=一次发音。
9. 特殊表记定义可在元数据行任何位置,定义域整行。
10. 新记号 `@l9`:该声部从此存在并复制文档总第 9 行内容,直到被覆盖。
11. 音区前缀 `*` → `>`,允许出现在正文内部:`>4|`、`|>4|`、`\>4|`。
12. 和弦 `_` 标注根音位置(`G_6`、`F_46`…);七和弦"3/5/7 音"=根向上第 2/3/4 音;三和弦"2/4/6/7 音"=紧邻定义。

### 附二、v0.3.1 的 11 条调整(全文)

1. `@l13` 复制调整行,但是**不复制其歌词**。
2. 符号 `@l13^+3`:在第 13 行的音轨的**上方三度**置一个平行和声。`lN^M` 中 N 即源行;M 是整数(如 3、+3、-3):符号代表在音轨的什么方向;数字代表在原音轨上方/下方几度的位置;此数字的绝对值必须为 7n+{3,4,5,6,8} 之一(n 为非负整数)。取音规则:
   - a. 如果某一个时刻,该行的音是此刻的和弦内音,那么,去寻找这一和弦以这个音为根音的那个形式,按照 v0.3 第 12 条("和弦诸音的定义")去寻找那一个音。
   - b. 否则,寻找那个对于该行的调号而言的调内音,使它在该音的上方或下方 |M| 度。
   - c. 这样的行为持续,直到下一个有效符号被检出。
3. 写法 `H_6`:H 是一个在第一行中按照和弦节奏型定义声明的特殊标记符号。代表"使用**此处的活动和弦**和 **6 所代表的和弦根音位置**替换**此行**解析得到的和弦";这不影响其他的行。例如,和弦是 `G_46`,该行中 `H_6`,则此行内部按照 `G_6` 分配。`_3`、`_5` 代表原位三和弦;`_7` 则是原位七和弦。
4. 特殊表记的声明内部,引号内可以出现空格。
5. 歌词不一定和谱数字位于同一列,但一定位于相同的行。在判断的时候,应当以"逐字句完成了对应"作为标准,也就是依次一一对应。
6. 在进行生成 musicxml 的时候,先将那些特殊表记和 `@l13`、`@l13^3` 之类的简化写作进行补全,生成一个副本,副本的扩展名改为 `.tymp`。补全的时候统一将调整行放于上方,如果那一行的那一个地方是空的;否则,如果存在非空的话,补全之前,应当先在其上方生成一个新的空行以存放其调整行。
7. 在上的机动行也可以有它的在上机动行。规范完全相同。
8. 全部和弦的吉他把位应当在数据库中被提供,以使得我们可以通过吉他把位标注来转换成音位。顺便地添加吉他把位表注的声明:`X&g"xxxxxx"`,x=1,2,3,4,5,6,代表哪一根弦(1 为低)。
9. 添加了几个对已有的文件的调整小工具——
   - a. 增添一个空的乐谱行;
   - b. 把某一个乐谱行的拍号进行改变,但不改变精度;
   - c. 对某一行乐谱:①增加一个空的伴奏行,需要指明是哪一行;②增添一个空的和声/旋律行。这二个增添行数的操作需要对其下方的所有 `@lxx` 格式的数字进行相应修改。
10. 支持两个乐谱行具有不同的「歌名 by 作者」,生成两个 musicxml 文件。(原文"革命"系"歌名"之误)
11. 做出一个定义:**maj 三和弦是增三和弦,而不是大三和弦**。

> 附二第 10/11 两条在原文中编号均为"10",此处顺次编号为 10、11。
