#ifndef TYMP_IMPL_H
#define TYMP_IMPL_H
/* ============================================================================
 * tymp → MusicXML 转换器:实现内部类型
 * 依据 tymp2musicxml_实现方案.md(§5.2 的落地版)
 * ==========================================================================*/
#include "tymp.h"

/* ---------------- 行 ---------------- */
typedef struct {
    char *head;      /* 元数据区(|| 之前) */
    char *music;     /* 乐曲区(|| 之后) */
    int   lineno;    /* 文档总行号(1 基) */
    int   head_len;  /* head 的显示长度(用于对齐诊断) */
} Row;

/* ---------------- 特殊表记定义 ---------------- */
typedef struct {
    char  name[8];        /* 标记名(不含参数部分) */
    char  param;          /* 参数名(如 'V');0 = 非参数化 */
    int   kind;           /* 1..6 */
    int   chord_relative; /* ⑤ r */
    int   voicing_g;      /* ⑥ g */
    char *content;        /* 引号 / 括号内原文 */
    int   n_units;        /* 按 _ 切分后的单位数 */
} MarkDef;

/* ---------------- 和弦(和弦行条目) ---------------- */
typedef struct {
    int  col;
    char root;
    int  root_alter;
    char quality[16];
    int  bass_n;
    int  bass[8];
    char bass_step;       /* 低音级数按调号消解后的字母(解析时固化) */
    int  bass_alter;
    int  is_none;         /* "-" */
    int  is_minor, has_seventh, is_maj7, is_dim, is_aug;
} Chord;

/* ---------------- 音符 ---------------- */
typedef struct {
    int  col;             /* 乐曲区起点列 */
    int  dur;             /* 时值(列) */
    int  dur_tick;        /* 时值(tick,>0 时优先使用;用于连音/等分非整列时值) */
    int  type_cols;       /* 音符型列宽(>0 时优先;连音音为 normal-type 对应的列数) */
    int  is_rest;
    int  seq;             /* 推入顺序(同列排序的稳定键) */
    char step;            /* A~G */
    int  alter;           /* -2..2 */
    int  octave;          /* 科学音高八度(C4 = 中央 C) */
    int  is_chord;        /* 与前一音同刻 */
    int  tie_start, tie_stop;
    int  dot;
    int  tm_actual, tm_normal;   /* 连音 */
    char *lyric;
    int  tie_lyric;
} Note;

/* ---------------- 声部 ---------------- */
typedef struct {
    char *id, *name_en, *name_zh;
    int   program;
    int   kind;           /* InsKind */
    int   unpitched;
    Note *notes; int n, cap;
    int   row_no;         /* 声部内的数字行序(1 起) */
    int   is_melody, is_harmony;
    int   octave_base;    /* 音区锚点:调根音 1 所在八度 */
    int   shift_oct;      /* 持续音区偏移(八度):由 . * ' 标记改变,到反向标记为止 */
    int   anchor_oct;     /* 逐音推进的八度锚点 */
    char  anchor_step;
    int   has_anchor;
    int   fixed_alter[8]; /* 声部级固定变音 */
    int   tie_pending;    /* 跨小节/跨组未完成延音线 */
    int   emit_floor;     /* 当前标记展开可回写的最早 Note 下标 */
} Voice;

/* ---------------- 组状态 ---------------- */
typedef struct {
    int   beats, beat_type;
    int   fifths;
    int   is_minor;
    char  key_root;
    int   key_alter;
    int   fixed_alter[8];
    int   chord_follow;
    double tempo;
    int   tempo_valid;
    char  tuning[6][4];
    int   tuning_valid;
    MarkDef *defs; int ndefs, defcap;
    Chord   *chords; int nchords, chordcap;
    char  voic_name[32][16];
    char  voic_fret[32][8];
    int   nvoic;          /* 和弦把位表(④类/{D/200232}) */
    int   accu;           /* 每拍列数 */
} GroupState;

/* ---------------- 一行扫描出的段 ---------------- */
typedef enum {
    SEG_EVENT,     /* 音符 / 休止 / 标记调用 / = 段 */
    SEG_PIPE,      /* | 起始段 */
    SEG_BACK,      /* \ 起始段 */
    SEG_COPY       /* @lN */
} SegKind;

typedef struct {
    SegKind kind;
    int  col;          /* 段起始列 */
    int  end;          /* 段结束列(不含) */
    char *text;        /* 段原文 */
    /* 事件额外信息 */
    int  ev_kind;      /* 见 EV_* */
    int  degree, alter, rest;
    int  mark_idx;     /* 标记调用:定义下标;-1 非标记 */
    char *mark_arg;    /* 参数化调用的实参 */
    int  chord_body;   /* {..} 的起始 */
} Seg;

enum { EV_NOTE = 0, EV_REST, EV_MARK, EV_EQ, EV_CHORD, EV_TUPLET, EV_NOTE_PAREN };

/* ---------------- 上下文 ---------------- */
typedef struct {
    GroupState gs;                 /* 当前组状态(跨组携带) */
    Voice *voices; int nvoices, vcap;   /* 全曲声部 */
    int   accu;
    const char *src_path;
    int   opt_nolyrics;
    int   opt_verbose;
} Ctx;

/* ============================ tympUtils.c ============================ */
char  *dup_str(const char *s);
void   strip_crlf(char *line);
void   strip_trailing_spaces(char *line);
char **read_all_lines(const char *path, int *n, int **linenos);
int    find_metadata_end(const char *line);      /* 返回 || 之后的起始列(-1 = 无) */
int    col_is_cjk(const char *s);                /* UTF-8 首字符是否 CJK */
int    utf8_len_of(const char *s);

int    key_to_fifths(char root, int alter);
void   degree_to_step(const GroupState *g, int degree, int alter,
                      char *step, int *alter_out);
void   degree_to_step_oct(const GroupState *g, int degree, int alter,
                         char *step, int *alter_out, int *oct_out);
int    step_is_diatonic(int fifths, char step, int alter);
int    resolve_octave(char prev_step, int prev_oct, char step, int *oct, int shift);
int    resolve_octave_bias(char prev_step, int prev_oct, char step, int *oct, int bias);

void   parse_timesig(const char *tok, GroupState *g);
void   parse_keyspec(const char *tok, GroupState *g);
void   parse_tempo_mark(const char *tok, GroupState *g);
void   parse_tuning(const char *tok, GroupState *g);
void   parse_fixed_acc(const char *tok, GroupState *g);
int    parse_dynamic_level(const char *tok);     /* |sN / |ppp → 级别;0 = 非力度 */

int    parse_mark_def(const char *s, int *end_col, MarkDef *out);
void   add_mark_def(GroupState *gs, const MarkDef *d);
int    find_mark_def(const GroupState *gs, const char *name, int namelen, int want_param);

void   parse_chord_token(const char *s, int col, Chord *out);
int    chord_bass_resolve(const Chord *c, const GroupState *gs, char *step, int *alter);
const char *voicing_lookup(const GroupState *gs, const Chord *c);
void   add_voicing(GroupState *gs, const char *name, const char *frets);

/* 把一个标记定义的内容展开为音符序列(追加到 voice) */
void   expand_mark(const MarkDef *d, const Chord *chord, const GroupState *gs,
                   int start_col, int span_cols, Voice *v, int us_cell);

/* 追加音符到声部 */
void   voice_push(Voice *v, Note nt);
/* 标记名字符判定(字母且不含 A~G) */
int    is_mark_letter(char c);

/* ============================ tymp2musicXML.c ============================ */
int    tymp2musicxml(const char *src, const char *dst, int nolyrics);

#endif /* TYMP_IMPL_H */
