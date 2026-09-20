#ifndef _TYMP_INTERNAL_H_
#define _TYMP_INTERNAL_H_

#include "tymp.h"

typedef struct { 
    int num; 
    int den; 
} Fraction;            /* 通用有理数(以拍为单位,1 拍 = 四分音符) */

typedef struct {                                      /* 调号状态 */
    char root; 
    int alter; 
    bool is_minor; 
    bool chord_no_follow;
    int  fifths;                                      /* 等价标准调号五度圈数 */
    int  fixed_alter[8];                              /* 【新增】度 1~7 的固定变音 -1/0/+1(|@[…]) */
} KeyState;

typedef struct { 
    int num;
    int den; 
} TimeState;           /* 拍号 */

typedef struct { 
    char str[6][4]; 
    bool valid; 
} GuitarTuning;   /* &gt: {a=E2,…} */

typedef struct SpecialMarkDef {                       /* ①~⑤ 特殊表记定义 */
    char name[8];        /* 标记名(1~4 字母) */
    int  kind;           /* 1..5 */
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

#endif //TYMP_INTERNAL_H