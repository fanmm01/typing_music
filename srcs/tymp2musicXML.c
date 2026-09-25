/* ============================================================================
 * tymp2musicXML.c —— 解析管线 + MusicXML 输出 + main
 * 依据 tymp2musicxml_实现方案.md(§6.2 / §6.3 / §7)
 *
 * 列模型:1 列 = 16 分音符 = 1/4 拍;每小节列数 = 16 × 分子 ÷ 分母。
 * 时值:事件起点列 → 下一事件起点列;标记调用按 R7 循环填充。
 * ==========================================================================*/
#include "tympImpl.h"
#include <ctype.h>

/* ========================== 全局输出上下文 ========================== */
static FILE *g_fp;
static int   g_divisions = 60;     /* 每四分音符 tick 数;列 tick = 15(整除 3、5 等分) */
static int   g_verbose = 0;
static int   g_fifths = 0;         /* 当前调号(供 <accidental> 判定) */
static int   g_unpitched_part = 0;  /* 当前 part 为无音高打击 */
static int   g_up_cnt = 0, g_dn_cnt = 0;   /* 诊断:八度标记计数 */

/* 新建 .musicxml 文件并写入 xmlHead(与 tymp.h 的 xmlHead 一致) */
FILE *initNewMusicXML(char *filename) {
    if (!filename) return NULL;
    size_t len = strlen(filename);
    char *full = (char *)malloc(len + 16);
    if (!full) return NULL;
    if (!(ifStrEndwith(filename, ".musicxml") || ifStrEndwith(filename, ".xml")))
        sprintf(full, "%s.musicxml", filename);
    else strcpy(full, filename);
    FILE *fp = fopen(full, "w");
    free(full);
    if (!fp) return NULL;
    fputs(xmlHead, fp);
    return fp;
}

/* ========================== 小工具 ========================== */
static int is_event_digit(char c) { return c >= '1' && c <= '8'; }
/* `*`、`'`、`.` 均为「作用于前一音」的后缀标记 */
static int is_oct_suffix(char c) { return c == '*' || c == '\'' || c == '.'; }
static int is_acc_suffix(char c) { return c == '+' || c == '-' || c == '~'; }

/* 音型表:列数 → type 名与附点 */
typedef struct { int cols; const char *name; int dot; } DurType;
static const DurType kDurTable[] = {
    {  1, "16th",    0 }, {  2, "eighth",  0 }, {  3, "eighth",  1 },
    {  4, "quarter", 0 }, {  6, "quarter", 1 }, {  8, "half",    0 },
    { 12, "half",    1 }, { 16, "whole",   0 }, { 24, "whole",   1 },
    { 32, "breve",   0 }, {  0, NULL,      0 }
};

/* 排序:按列升序;同列的「和弦首音」排在其续音之前;同列同型按推入顺序(稳定) */
static int note_cmp(const void *a, const void *b) {
    const Note *x = (const Note *)a, *y = (const Note *)b;
    if (x->col != y->col) return x->col - y->col;
    if (x->is_chord != y->is_chord)
        return (x->is_chord ? 1 : 0) - (y->is_chord ? 1 : 0);
    return x->seq - y->seq;
}

static int note_tie_key(const Note *nt) {
    if (nt->is_rest) return -1;
    int s = 0;
    switch (nt->step) {
    case 'C': s = 0; break; case 'D': s = 1; break; case 'E': s = 2; break;
    case 'F': s = 3; break; case 'G': s = 4; break; case 'A': s = 5; break;
    case 'B': s = 6; break; default: return -1;
    }
    return ((nt->octave * 7 + s) * 13) + nt->alter + 128;
}

/* 延音端点校验：每个声部按音高维护一个活动延音状态。
 * 这只清理孤立 stop 和重叠 start，不凭总数相等而删除合法跨小节链。 */
static void normalize_note_ties(Note *notes, int n) {
    int *keys = NULL, *active = NULL, nk = 0, cap = 0;
    for (int i = 0; i < n; i++) {
        Note *nt = &notes[i];
        if (nt->is_rest) { nt->tie_start = nt->tie_stop = 0; continue; }
        int key = note_tie_key(nt);
        if (key < 0) { nt->tie_start = nt->tie_stop = 0; continue; }
        int pos = -1;
        for (int k = 0; k < nk; k++) if (keys[k] == key) { pos = k; break; }
        if (pos < 0) {
            if (nk >= cap) {
                cap = cap ? cap * 2 : 16;
                keys = (int *)realloc(keys, (size_t)cap * sizeof(*keys));
                active = (int *)realloc(active, (size_t)cap * sizeof(*active));
            }
            pos = nk++; keys[pos] = key; active[pos] = 0;
        }
        if (nt->tie_stop && !active[pos]) nt->tie_stop = 0;
        if (nt->tie_start && active[pos]) nt->tie_start = 0;
        if (nt->tie_stop) active[pos] = 0;
        if (nt->tie_start) active[pos] = 1;
    }
    free(keys); free(active);
}

static void dur_to_type(int cols, const char **name, int *dot) {
    for (int i = 0; kDurTable[i].name; i++) {
        if (kDurTable[i].cols == cols) { *name = kDurTable[i].name; *dot = kDurTable[i].dot; return; }
    }
    /* 就近取不超过者 */
    int best = 0;
    for (int i = 0; kDurTable[i].name; i++) if (kDurTable[i].cols <= cols) best = i;
    *name = kDurTable[best].name; *dot = 0;
}

/* 精确音型匹配(含附点);不匹配返回 0 */
static int dur_exact(int cols, const char **name, int *dot) {
    for (int i = 0; kDurTable[i].name; i++)
        if (kDurTable[i].cols == cols) { *name = kDurTable[i].name; *dot = kDurTable[i].dot; return 1; }
    return 0;
}

static int gcd_int(int a, int b) {
    while (b) { int t = a % b; a = b; b = t; }
    return a;
}

/* 输出用户提供的 XML 文本。谱面中的歌词、标题和方向文字可以含有
 * &, <, > 以及引号；统一在唯一出口转义，避免生成不可解析的 XML。 */
static void write_xml_escaped(FILE *fp, const char *s, int attribute) {
    if (!s) return;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '&': fputs("&amp;", fp); break;
        case '<': fputs("&lt;", fp); break;
        case '>': fputs("&gt;", fp); break;
        case '"': if (attribute) fputs("&quot;", fp); else fputc('"', fp); break;
        case '\'' : if (attribute) fputs("&apos;", fp); else fputc('\'', fp); break;
        default: fputc(*p, fp); break;
        }
    }
}

/* ========================== 行扫描 ========================== */

enum { IT_NOTE, IT_REST, IT_MARK, IT_EQ, IT_SEG_PIPE, IT_SEG_BACK, IT_COPY, IT_IGNORE };

typedef struct {
    int  kind;
    int  col, end;        /* 起止列(乐曲区坐标) */
    int  degree, alter, shift;
    int  mark_idx;
    char mark_arg[8];
    char *text;           /* 段原文(段类) */
    int  seg_subdiv;      /* | 后下划线数 */
    int  chord_override;  /* H_6:标记调用带 _n → 覆盖本行活动和弦的低音(0 = 无) */
} Item;

typedef struct {
    Item *it; int n, cap;
} ItemList;

static void il_push(ItemList *l, Item v) {
    if (l->n >= l->cap) { l->cap = l->cap ? l->cap * 2 : 64;
                          l->it = (Item *)realloc(l->it, l->cap * sizeof(Item)); }
    l->it[l->n++] = v;
}

/* 判断标记名:字母序列(不含 A~G),长度 1~4 */
static int match_mark(const GroupState *gs, const char *s, int *len_out, int *idx_out, char *arg_out) {
    int i = 0;
    while (i < 4 && is_mark_letter(s[i])) i++;
    if (i == 0) return 0;
    int idx = find_mark_def(gs, s, i, s[i] == '(');
    if (idx < 0) return 0;
    int used = i;
    char arg[8] = {0};
    if (gs->defs[idx].param && s[i] == '(') {      /* 参数化调用 X(3) */
        int j = i + 1, k = 0;
        while (s[j] && s[j] != ')' && k < 7) arg[k++] = s[j++];
        arg[k] = '\0';
        if (s[j] == ')') used = j + 1;
    }
    *len_out = used; *idx_out = idx;
    if (arg_out) strcpy(arg_out, arg);
    return 1;
}

/* H_6:标记调用后跟 _n → 返回 n(覆盖本行活动和弦的低音位置) */
static int match_mark_chord_override(const char *s, int used, int *end_out) {
    if (s[used] != '_') return 0;
    int j = used + 1, v = 0;
    while (isdigit((unsigned char)s[j])) { v = v * 10 + (s[j] - '0'); j++; }
    if (v <= 0) return 0;
    *end_out = j;
    return v;
}

/* 扫描乐曲区为条目序列 */
static void scan_items(const char *m, int len, const GroupState *gs, ItemList *out) {
    int i = 0;
    while (i < len) {
        char c = m[i];
        if (c == ' ' || c == '\t') { i++; continue; }

        /* ---- 段:| 与 \ ---- */
        if (c == '|') {
            int j = i + 1;
            if (m[j] == ' ' || j >= len) { i++; continue; }      /* 空段 */
            while (j < len && m[j] != ' ' && m[j] != '|' && m[j] != '\\') j++;
            Item it; memset(&it, 0, sizeof(it));
            it.kind = IT_SEG_PIPE; it.col = i; it.end = j;
            it.text = (char *)malloc(j - i + 1);
            memcpy(it.text, m + i + 1, j - i - 1); it.text[j - i - 1] = '\0';
            it.seg_subdiv = 0;
            il_push(out, it);
            i = j; continue;
        }
        if (c == '\\') {
            int j = i + 1;
            if (j >= len) { i++; continue; }
            if (m[j] == ' ' || m[j] == '\\') {                   /* \ … \ 单独符号区间 */
                int k = j;
                while (k < len && m[k] != '\\') k++;
                Item it; memset(&it, 0, sizeof(it));
                it.kind = IT_SEG_BACK; it.col = i; it.end = (k < len) ? k + 1 : len;
                it.text = (char *)malloc(k - j + 1);
                memcpy(it.text, m + j, k - j); it.text[k - j] = '\0';
                il_push(out, it);
                i = (k < len) ? k + 1 : len;
                continue;
            }
            while (j < len && m[j] != ' ' && m[j] != '\\') j++;
            Item it; memset(&it, 0, sizeof(it));
            it.kind = IT_SEG_BACK; it.col = i; it.end = j;
            it.text = (char *)malloc(j - i + 1);
            memcpy(it.text, m + i + 1, j - i - 1); it.text[j - i - 1] = '\0';
            il_push(out, it);
            i = j; continue;
        }

        /* ---- @lN 行复制 / @lN^±M 平行和声 ---- */
        if (c == '@' && m[i + 1] == 'l') {
            int j = i + 2, num = 0;
            while (isdigit((unsigned char)m[j])) { num = num * 10 + (m[j] - '0'); j++; }
            Item it; memset(&it, 0, sizeof(it));
            it.kind = IT_COPY; it.col = i; it.end = j; it.degree = num;
            if (m[j] == '^') {                       /* 平行和声:^+3 / ^-3 */
                int sgn = 1, v = 0;
                j++;
                if (m[j] == '+') { sgn = 1; j++; }
                else if (m[j] == '-') { sgn = -1; j++; }
                while (isdigit((unsigned char)m[j])) { v = v * 10 + (m[j] - '0'); j++; }
                it.shift = sgn * v;                  /* shift = 方向×度数 */
                it.end = j;
            }
            il_push(out, it);
            i = j; continue;
        }

        /* ---- 事件 ---- */
        Item it; memset(&it, 0, sizeof(it)); it.col = i; it.mark_idx = -1;

        if (is_event_digit(c)) {
            it.kind = IT_NOTE; it.degree = c - '0';
            i++;
            while (i < len && (is_acc_suffix(m[i]) || is_oct_suffix(m[i]))) {
                char s = m[i];
                if (s == '+') it.alter++;
                else if (s == '-') it.alter--;
                else if (s == '~') it.alter = 0;
                else if (s == '*' || s == '\'') it.shift++;
                else if (s == '.') it.shift--;          /* '.' = 后缀:降一个八度组 */
                i++;
            }
            il_push(out, it); continue;
        }
        if (c == '0') {
            it.kind = IT_REST; i++;
            il_push(out, it); continue;
        }
        if (c == '(') {                                   /* (n) / (n+) / (n.) / (7/8) */
            int j = i + 1, dg = 0, al = 0, sh = 0;
            if (isdigit((unsigned char)m[j])) dg = m[j] - '0';
            j++;
            while (j < len && m[j] != ')') {
                char s = m[j];
                if (s == '+') al++;
                else if (s == '-') al--;
                else if (s == '*') sh++;
                j++;
            }
            int end = (j < len) ? j + 1 : len;
            /* 标记调用之后的括号单音 = 同标记的再次调用:
             * X(3)  (5+)  (7) → X(3)  X(5+)  X(7) */
            if (dg > 0 && out->n > 0 && out->it[out->n - 1].kind == IT_MARK) {
                Item it2 = out->it[out->n - 1];
                it2.col = i; it2.end = end; it2.chord_override = 0;
                int k2 = 0;
                for (int t = i + 1; t < end - 1 && k2 < 7; t++) it2.mark_arg[k2++] = m[t];
                it2.mark_arg[k2] = '\0';
                il_push(out, it2);
                i = end; continue;
            }
            if (dg > 0) {
                it.kind = IT_NOTE; it.degree = dg; it.alter = al; it.shift = sh;
                il_push(out, it);
            }
            i = end;
            continue;
        }
        if (c == '{') {                                   /* 柱式和弦:{135} / {13-} */
            int j = i + 1;
            while (j < len && m[j] != '}') j++;
            it.kind = IT_NOTE; it.degree = 100;           /* 特殊标:和弦体在 text */
            it.text = (char *)malloc(j - i);
            memcpy(it.text, m + i + 1, j - i - 1); it.text[j - i - 1] = '\0';
            it.end = (j < len) ? j + 1 : len;
            il_push(out, it);
            i = it.end; continue;
        }
        if (c == '[') {                                   /* [123] / [~123] 等分 */
            int j = i + 1, nt = 0;
            while (j < len && m[j] == '~') { nt++; j++; }
            char buf[16]; int cnt = 0;
            while (j < len && m[j] != ']' && cnt < 15) {
                if (is_event_digit(m[j])) buf[cnt++] = m[j];
                j++;
            }
            buf[cnt] = '\0';
            if (cnt > 0) {
                it.kind = IT_NOTE; it.degree = 400;       /* 等分:alter=音数,shift=~ 数 */
                it.alter = cnt; it.shift = nt;
                it.text = dup_str(buf);
                it.end = (j < len) ? j + 1 : len;
                il_push(out, it);
            }
            i = (j < len) ? j + 1 : len;
            continue;
        }
        if (c == '=') {                                   /* = 段 */
            int j = i;
            while (j < len && m[j] == '=') j++;
            it.kind = IT_EQ; it.end = j; it.degree = j - i;    /* degree = 等号个数 */
            il_push(out, it);
            i = j; continue;
        }
        if (c == '%') {                                   /* %345 / %% 连音 */
            int j = i + 1, dbl = 0;
            if (j < len && m[j] == '%') { dbl = 1; j++; }   /* %% = 8 列三连音 */
            if (dbl) {
                /* %% 一律按间隔书写形式:紧凑 %%345678 也拆为逐列音素,
                 * 由行转换在 8 列窗口内收集组成员(%%3 4 5 / %%345678 / %%R R R) */
                it.kind = IT_NOTE; it.degree = 201; it.end = j;
                il_push(out, it);
                i = j;
            } else {
                int n = 0;
                while (j < len && is_event_digit(m[j])) { n++; j++; }
                if (n > 0) {
                    it.kind = IT_NOTE; it.degree = 200; it.end = j; it.alter = n;
                    it.text = (char *)malloc(n + 1);
                    memcpy(it.text, m + i + 1, n); it.text[n] = '\0';
                    il_push(out, it);
                }
                i = j;
            }
            continue;
        }
        if (c == '\'' || c == '*' || c == '.' || c == '+' || c == '-' || c == '~') {
            /* 行内孤立符号:作用于相邻事件 */
            it.kind = IT_IGNORE; it.alter = (c == '+') ? 1 : (c == '-') ? -1 : 0;
            it.degree = (int)(unsigned char)c;             /* 保存原字符 */
            il_push(out, it);
            i++; continue;
        }
        if (c == '>' && isdigit((unsigned char)m[i + 1])) {  /* 谱内音区前缀 >n(须与渐强 >   > 区分) */
            int j = i + 1, v = 0;
            while (isdigit((unsigned char)m[j])) { v = v * 10 + (m[j] - '0'); j++; }
            it.kind = IT_IGNORE; it.degree = (int)'>'; it.alter = v;
            il_push(out, it);
            i = j; continue;
        }
        if (c == '_' && isdigit((unsigned char)m[i + 1])) {
            /* 标记调用之后的 _n / _46 = 同标记再次调用 + 根音位置覆盖
             * (X _6 _46 → X X_6 X_46);非标记之后 → 等价空格 */
            int j = i + 1, v = 0;
            while (isdigit((unsigned char)m[j])) { v = v * 10 + (m[j] - '0'); j++; }
            if (out->n > 0 && out->it[out->n - 1].kind == IT_MARK) {
                Item it2 = out->it[out->n - 1];
                it2.col = i; it2.end = j; it2.chord_override = v;
                il_push(out, it2);
            }
            i = j; continue;
        }
        if (isalpha((unsigned char)c)) {
            int ml = 0, mi = -1; char arg[8] = {0};
            if (match_mark(gs, m + i, &ml, &mi, arg)) {
                it.kind = IT_MARK; it.mark_idx = mi; strcpy(it.mark_arg, arg);
                it.end = i + ml;
                int e2 = 0, cov = match_mark_chord_override(m + i, ml, &e2);
                if (cov > 0) { it.chord_override = cov; it.end = i + e2; }
                il_push(out, it);
                i = it.end; continue;
            }
            if (toupper((unsigned char)c) >= 'A' && toupper((unsigned char)c) <= 'G') {
                it.kind = IT_NOTE; it.degree = 300;        /* 绝对音 */
                it.text = (char *)malloc(2); it.text[0] = (char)toupper((unsigned char)c); it.text[1] = 0;
                i++;
                il_push(out, it); continue;
            }
        }
        i++;   /* 未识别字符:跳过 */
    }
}

/* %% 连音的 8 列窗口收集成员:把成员原始位置保留为同组列,输出时由
 * 组首的 8 列 dur 占位;组内每个音的 tick 按 n 等分。 */

static Voice *new_voice(Ctx *ctx) {
    if (ctx->nvoices >= ctx->vcap) {
        ctx->vcap = ctx->vcap ? ctx->vcap * 2 : 16;
        ctx->voices = (Voice *)realloc(ctx->voices, ctx->vcap * sizeof(Voice));
    }
    Voice *v = &ctx->voices[ctx->nvoices++];
    memset(v, 0, sizeof(*v));
    return v;
}

/* 把某行的条目解析为该声部的音符(时值 = 到下一事件列距;标记循环填充) */
static void row_to_voice(Ctx *ctx, const ItemList *items, Voice *v, int start_col, int skip_existing) {
    const GroupState *gs = &ctx->gs;
    for (int i = 0; i < items->n; i++) {
        const Item *it = &items->it[i];
        if (it->kind == IT_SEG_PIPE || it->kind == IT_SEG_BACK || it->kind == IT_COPY) continue;
        if (it->kind == IT_IGNORE) {
            if (it->degree == (int)'>' && it->alter >= 1 && it->alter <= 9) {
                /* 谱内音区前缀 >n:重置本声部音区锚点 */
                v->octave_base = it->alter;
                v->anchor_oct = it->alter;
            } else if (it->degree == (int)'@' && it->text) {
                /* 固定变音段 |@[...] / \@[...]:按列更新声部固定变音,影响后续音符 */
                GroupState tg = *gs;
                parse_fixed_acc(it->text, &tg);
                for (int f2 = 0; f2 < 8; f2++) v->fixed_alter[f2] = tg.fixed_alter[f2];
            }
            continue;
        }

        /* 机动段并入时:同列已有音符则跳过(本行自身事件优先) */
        if (skip_existing && it->kind != IT_IGNORE &&
            it->kind != IT_SEG_PIPE && it->kind != IT_SEG_BACK && it->kind != IT_COPY) {
            int dup = 0;
            for (int q = 0; q < v->n; q++)
                if (v->notes[q].col == it->col) { dup = 1; break; }
            if (dup) continue;
        }

        int has_next = 0;
        int next_col = start_col;                 /* 下一事件列(决定时值) */
        for (int j = i + 1; j < items->n; j++) {
            Item *nx = &items->it[j];
            if (nx->kind == IT_SEG_PIPE || nx->kind == IT_SEG_BACK || nx->kind == IT_IGNORE) continue;
            if (nx->kind == IT_COPY) continue;
            /* %% 组的成员事件属于同一组,不应截断组首的 8 单位时值 */
            if (it->kind == IT_NOTE && it->degree == 201) continue;
            next_col = nx->col; has_next = 1; break;
        }
        int span = next_col - it->col;
        if (span <= 0) span = 1;
        else if (!has_next) {
            /* 行尾事件:延续到小节末(musicxml2tymp 生成的行不含收尾占位;
             * 小节内时值总和守恒,不跨小节,不影响既有 .tymp 输入) */
            int mcols = 16 * gs->beats / gs->beat_type;
            if (mcols <= 0) mcols = 16;
            span = mcols - it->col % mcols;
        }

        switch (it->kind) {
        case IT_REST: {
            Note nt; memset(&nt, 0, sizeof(nt));
            nt.col = it->col; nt.dur = span; nt.is_rest = 1;
            voice_push(v, nt);
            break;
        }
        case IT_MARK: {
            const MarkDef *d = &gs->defs[it->mark_idx];
            /* 参数化:文本代入 */
            MarkDef dd = *d;
            if (d->param && it->mark_arg[0]) {
                char *sub = (char *)malloc(strlen(d->content) + 64);
                int k = 0;
                for (const char *p = d->content; *p; p++) {
                    if (*p == d->param) { for (const char *q = it->mark_arg; *q; q++) sub[k++] = *q; }
                    else sub[k++] = *p;
                }
                sub[k] = '\0';
                dd.content = sub;
            } else {
                dd.content = dup_str(d->content);
            }
            const Chord *ch = NULL;
            for (int c = gs->nchords - 1; c >= 0; c--)
                if (gs->chords[c].col <= it->col) { ch = &gs->chords[c]; break; }
            Chord dummy; memset(&dummy, 0, sizeof(dummy)); dummy.root = 'C';
            /* H_6:以「此处活动和弦 + 覆盖的低音位置」替换本行消解和弦(不影响他行) */
            Chord ovr;
            if (it->chord_override > 0 && ch) {
                ovr = *ch;
                ovr.bass[0] = it->chord_override; ovr.bass_n = 1;
                ch = &ovr;
            }
            /* 标记调用以本声部音区为基准起算,避免长段落音区累积漂移 */
            v->anchor_oct = v->octave_base; v->has_anchor = 0;
            expand_mark(&dd, ch ? ch : &dummy, gs, it->col, span, v, 1);
            free(dd.content);
            break;
        }
        case IT_EQ: {
            /* = 段:由同列 | 段(或 \ 段)提供内容铺满;无配对则记为休止(告警) */
            if (it->text && it->text[0] && it->text[0] != '[') {
                int subdiv = 0;
                const char *body = it->text;
                while (*body == '_') { subdiv++; body++; }   /* |__…:格内细分 1/2^k */
                MarkDef tmp; memset(&tmp, 0, sizeof(tmp));
                tmp.content = (char *)body; tmp.kind = 2;
                const Chord *ch = NULL;
                for (int c = gs->nchords - 1; c >= 0; c--)
                    if (gs->chords[c].col <= it->col) { ch = &gs->chords[c]; break; }
                Chord dummy; memset(&dummy, 0, sizeof(dummy)); dummy.root = 'C';
                int before2 = v->n;
                expand_mark(&tmp, ch ? ch : &dummy, gs, it->col, span, v, 0);
                if (subdiv > 0) {
                    /* |__…:每列拆为 2^subdiv 格——同列格共位、逐格 tick 取整
                     * (15/2^subdiv 的累计取整差),整段 tick 总和恰为 span×15 */
                    int ci2 = 0;
                    for (int k2 = before2; k2 < v->n; k2++) {
                        if (v->notes[k2].is_chord) continue;
                        int base = g_divisions / 4;
                        v->notes[k2].dur_tick = ((base * (ci2 + 1)) >> subdiv)
                                              - ((base * ci2) >> subdiv);
                        v->notes[k2].col = it->col + (ci2 >> subdiv);
                        v->notes[k2].dur = 1;
                        ci2++;
                    }
                }
            } else if (it->text && it->text[0] == '[') {
                /* |[...] 管道括号等分:内容按 _ 分格,n 格在段内等分 */
                const char *p = strchr(it->text, '[');
                const char *q = strchr(it->text, ']');
                if (p && q && q > p) {
                    int ncell = 1;
                    for (const char *t = p + 1; t < q; t++) if (*t == '_') ncell++;
                    char *body = (char *)malloc(q - p);
                    memcpy(body, p + 1, q - p - 1); body[q - p - 1] = '\0';
                    MarkDef tmp; memset(&tmp, 0, sizeof(tmp));
                    tmp.content = body; tmp.kind = 2;
                    const Chord *ch = NULL;
                    for (int c = gs->nchords - 1; c >= 0; c--)
                        if (gs->chords[c].col <= it->col) { ch = &gs->chords[c]; break; }
                    Chord dummy; memset(&dummy, 0, sizeof(dummy)); dummy.root = 'C';
                    /* 每格时值 = span/ncell 列(tick 粒度);整除时保持原状(逐列 15 tick),
                     * 否则余数分给前 rem 格并给 tm */
                    int per_tick = span * (g_divisions / 4) / ncell;
                    int rem = span * (g_divisions / 4) % ncell;
                    int before = v->n;
                    expand_mark(&tmp, ch ? ch : &dummy, gs, it->col, span, v, 0);
                    if (rem != 0) {
                        int T3 = 16;
                        while (T3 > 1 && 4.0 / T3 < span / 4.0 / ncell - 1e-9) T3 /= 2;
                        int tcols3 = 16 / T3;
                        int g3 = gcd_int(per_tick, tcols3 * (g_divisions / 4));
                        int tm_act3 = (g3 > 0) ? tcols3 * (g_divisions / 4) / g3 : ncell;
                        int tm_nrm3 = (g3 > 0) ? per_tick / g3 : tcols3;
                        int ci = 0;
                        for (int k = before; k < v->n; k++) {
                            if (!v->notes[k].is_chord) {
                                v->notes[k].dur_tick = per_tick + (ci < rem ? 1 : 0);
                                v->notes[k].type_cols = tcols3;
                                v->notes[k].tm_actual = tm_act3;
                                v->notes[k].tm_normal = tm_nrm3;
                                ci++;
                            }
                        }
                    }
                    free(body);
                } else {
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = it->col; nt.dur = span; nt.is_rest = 1;
                    voice_push(v, nt);
                }
            } else {
                Note nt; memset(&nt, 0, sizeof(nt));
                nt.col = it->col; nt.dur = span; nt.is_rest = 1;
                voice_push(v, nt);
            }
            break;
        }
        case IT_NOTE:
        default: {
            if (it->degree == 100) {              /* 柱式和弦 {..} */
                int first = 1;
                for (const char *p = it->text; *p; ) {
                    if (isdigit((unsigned char)*p)) {
                        int dg = *p - '0'; p++;
                        int al = 0, sh = 0;
                        while (*p && (is_acc_suffix(*p) || is_oct_suffix(*p))) {
                            if (*p == '+') al++; else if (*p == '-') al--;
                            else if (*p == '~') al = 0;
                            else if (*p == '*' || *p == '\'') sh++; else if (*p == '.') sh--;
                            p++;
                        }
                        char st; int alo; int og8 = 0;
                        degree_to_step_oct(gs, dg, al + v->fixed_alter[dg == 8 ? 1 : dg], &st, &alo, &og8);
                        Note nt; memset(&nt, 0, sizeof(nt));
                        nt.col = it->col; nt.dur = span; nt.is_chord = !first; first = 0;
                        nt.step = st; nt.alter = alo;
                        int oct = v->anchor_oct;
                        if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, sh + og8);
                        nt.octave = oct + v->shift_oct;
                        voice_push(v, nt);
                        v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                    } else p++;
                }
            } else if (it->degree == 200) {       /* % 连音(%234=4 列;%%234/5-7 连音=8 列) */
                int n = it->alter > 0 ? it->alter : 1;
                int span_t = (n >= 4 || it->shift) ? 8 : 4;
                if (n >= span_t) {
                    /* 音数不亚于声明宽度(实践中的长 % 跑句):按普通音逐列铺开 */
                    for (int k = 0; k < n; k++) {
                        int dg = it->text[k] - '0';
                        char st; int alo; int og8 = 0;
                        degree_to_step_oct(gs, dg, v->fixed_alter[dg == 8 ? 1 : dg], &st, &alo, &og8);
                        Note nt; memset(&nt, 0, sizeof(nt));
                        nt.col = it->col + k; nt.dur = 1;
                        nt.step = st; nt.alter = alo;
                        int oct = v->anchor_oct;
                        if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, og8);
                        nt.octave = oct + v->shift_oct;
                        voice_push(v, nt);
                        v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                    }
                    break;
                }
                int total = span_t * (g_divisions / 4);
                int dt = total / n, rem = total % n;
                /* §3.3:取最大 T 使 4/T ≥ 每音时值(拍);tm = dt:type 约分 */
                int T = 16;
                while (T > 1 && 4.0 / T < span_t / 4.0 / n - 1e-9) T /= 2;
                int tcols = 16 / T;
                int g = gcd_int(dt, tcols * (g_divisions / 4));
                int tm_act = (g > 0) ? tcols * (g_divisions / 4) / g : n;
                int tm_nrm = (g > 0) ? dt / g : tcols;
                if (tm_act == tm_nrm) tm_act = 0;          /* 实际并非连音 → 不写 tm */
                for (int k = 0; k < n; k++) {
                    int dg = it->text[k] - '0';
                    char st; int alo; int og8 = 0;
                    degree_to_step_oct(gs, dg, v->fixed_alter[dg == 8 ? 1 : dg], &st, &alo, &og8);
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = it->col; nt.dur = span_t;
                    nt.dur_tick = dt + (k < rem ? 1 : 0);
                    nt.type_cols = tcols;
                    nt.tm_actual = tm_act;
                    nt.tm_normal = tm_nrm;
                    nt.step = st; nt.alter = alo;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, og8);
                    nt.octave = oct + v->shift_oct;
                    voice_push(v, nt);
                    v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                }
            } else if (it->degree == 201) {       /* %% 连音(间隔书写):后 8 列内的音/标记为组成员 */
                int c0 = it->col;
                int idxs[64]; int nid = 0;
                int j = i + 1;
                while (j < items->n && items->it[j].col < c0 + 8) {
                    Item *nx = &items->it[j];
                    if ((nx->kind == IT_NOTE && nx->degree >= 1 && nx->degree <= 8)
                        || nx->kind == IT_MARK) idxs[nid++] = j;
                    else if (nx->kind != IT_IGNORE) break;
                    j++;
                }
                int n2 = nid > 0 ? nid : 1;
                int total = 8 * (g_divisions / 4);
                int dt2 = total / n2, rem2 = total % n2;
                int T2 = 16;
                while (T2 > 1 && 4.0 / T2 < 8.0 / 4.0 / n2 - 1e-9) T2 /= 2;
                int tcols2 = 16 / T2;
                int gg = gcd_int(dt2, tcols2 * (g_divisions / 4));
                int tm_a2 = (gg > 0) ? tcols2 * (g_divisions / 4) / gg : n2;
                int tm_n2 = (gg > 0) ? dt2 / gg : tcols2;
                if (tm_a2 == tm_n2) tm_a2 = 0;
                for (int q = 0; q < nid; q++) {
                    Item *nx = &items->it[idxs[q]];
                    int before2 = v->n;
                    if (nx->kind == IT_MARK) {
                        const MarkDef *d2 = &gs->defs[nx->mark_idx];
                        MarkDef dd = *d2;
                        if (d2->param && nx->mark_arg[0]) {
                            char *sub = (char *)malloc(strlen(d2->content) + 64);
                            int k2 = 0;
                            for (const char *p = d2->content; *p; p++) {
                                if (*p == d2->param) { for (const char *q2 = nx->mark_arg; *q2; q2++) sub[k2++] = *q2; }
                                else sub[k2++] = *p;
                            }
                            sub[k2] = '\0';
                            dd.content = sub;
                        } else dd.content = dup_str(d2->content);
                        const Chord *ch = NULL;
                        for (int c2 = gs->nchords - 1; c2 >= 0; c2--)
                            if (gs->chords[c2].col <= c0) { ch = &gs->chords[c2]; break; }
                        Chord dummy; memset(&dummy, 0, sizeof(dummy)); dummy.root = 'C';
                        expand_mark(&dd, ch ? ch : &dummy, gs, c0, 1, v, 1);
                        free(dd.content);
                    } else {
                        int dg = nx->degree;
                        char st; int alo; int og8 = 0;
                        int al3 = nx->alter + ((dg >= 1 && dg <= 7) ? v->fixed_alter[dg] : 0);
                        degree_to_step_oct(gs, dg, al3, &st, &alo, &og8);
                        Note nt; memset(&nt, 0, sizeof(nt));
                        nt.col = c0; nt.dur = 1;
                        nt.step = st; nt.alter = alo;
                        int oct = v->anchor_oct;
                        if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, og8);
                        nt.octave = oct + v->shift_oct;
                        voice_push(v, nt);
                        v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                    }
                    for (int k2 = before2; k2 < v->n; k2++) {
                        v->notes[k2].dur_tick = dt2 + (q < rem2 ? 1 : 0);
                        v->notes[k2].type_cols = tcols2;
                        v->notes[k2].tm_actual = tm_a2;
                        v->notes[k2].tm_normal = tm_n2;
                    }
                    if (q == 0 && v->n > before2) v->notes[before2].dur = 8;   /* 组宽推进 cursor */
                }
                i = (j > i + 1) ? j - 1 : i;
            } else if (it->degree == 400) {       /* [..] 等分(1 拍 / 每 ~ 多 1 拍,n 等分) */
                int n = it->alter > 0 ? it->alter : 1;
                int beats = 1 + it->shift;
                int per_beat_cols = (gs->beat_type > 0 && 16 % gs->beat_type == 0)
                                    ? 16 / gs->beat_type : 4;
                int cols = beats * per_beat_cols;
                int per_tick = cols * (g_divisions / 4) / n;
                int rem_t = cols * (g_divisions / 4) % n;
                /* normal-type:取最大 T 使 4/T ≥ 每音时值(拍) */
                int T = 16;
                double d = cols / 4.0 / n;
                for (int cand = 16; cand >= 1; cand /= 2) {
                    if (4.0 / cand >= d - 1e-9) { T = cand; break; }
                }
                int tcols = 16 / T;
                int g2 = gcd_int(per_tick, tcols * (g_divisions / 4));
                int tm_act2 = (g2 > 0) ? tcols * (g_divisions / 4) / g2 : n;
                int tm_nrm2 = (g2 > 0) ? per_tick / g2 : tcols;
                for (int k = 0; k < n; k++) {
                    int dg = it->text[k] - '0';
                    char st; int alo; int og8 = 0;
                    degree_to_step_oct(gs, dg, v->fixed_alter[dg == 8 ? 1 : dg], &st, &alo, &og8);
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = it->col; nt.dur = cols;
                    nt.dur_tick = per_tick + (k < rem_t ? 1 : 0);
                    nt.type_cols = tcols;
                    nt.tm_actual = tm_act2;
                    nt.tm_normal = tm_nrm2;
                    nt.step = st; nt.alter = alo;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, og8);
                    nt.octave = oct + v->shift_oct;
                    voice_push(v, nt);
                    v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                }
            } else {
                char st; int alo; int og8 = 0;
                int dg = it->degree;
                if (it->degree == 300) { st = it->text[0]; alo = 0; }
                else {
                    int fa = (dg >= 1 && dg <= 7) ? v->fixed_alter[dg] : 0;
                    degree_to_step_oct(gs, dg, it->alter + fa, &st, &alo, &og8);
                }
                Note nt; memset(&nt, 0, sizeof(nt));
                nt.col = it->col; nt.dur = span;
                nt.step = st; nt.alter = alo;
                /* 音区标记 = 方向:该音取该方向上离锚点最近的八度音;
                 * 只作用于带标记的这一音,其后音区由锚点自然延续。 */
                int bias = 0;
                if (it->shift > 0) { bias = 1;  g_up_cnt++; }
                else if (it->shift < 0) { bias = -1; g_dn_cnt++; }
                int oct = v->anchor_oct;
                if (v->has_anchor) resolve_octave_bias(v->anchor_step, v->anchor_oct, st, &oct, bias);
                else oct = v->octave_base;
                nt.octave = oct + og8;
                voice_push(v, nt);
                v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
            }
            break;
        }
        }
    }
}

/* ========================== direction / harmony ========================== */

typedef struct { int col; int kind; char text[24]; double tempo; int level; } DirEv;
/* kind: 1=tempo 2=words 3=dynamics 4=wedge 5=fermata */
static DirEv g_dirs[512]; static int g_ndirs = 0;
static int  g_first_tempo = 0; static double g_first_bpm = 120;

/* 该列是否有延音号(|。)→ 挂音符 <fermata/> */
static int fermata_at(int col) {
    for (int i = 0; i < g_ndirs; i++)
        if (g_dirs[i].kind == 5 && g_dirs[i].col == col) return 1;
    return 0;
}

static void record_direction(int col, const char *tok, const GroupState *gs) {
    /* 组尾可能没有当前组状态，方向文字仍应安全地记录。 */
    GroupState fallback; memset(&fallback, 0, sizeof(fallback)); fallback.tempo = 120;
    if (!gs) gs = &fallback;
    /* |rit.|vb.|accel.|rall.|vi-|vi+ → words;曲首 |vbN → metronome */
    const char *t = tok;
    while (*t == '|' || *t == '\\') t++;
    char w[24]; int n = 0;
    while (t[n] && t[n] != ' ' && n < 23) { w[n] = t[n]; n++; }
    w[n] = '\0';
    int dl = parse_dynamic_level(tok);
    if (dl > 0)                                     { if (g_ndirs<512){g_dirs[g_ndirs++]=(DirEv){col,3,"",0,dl};} return; }
    if (!strcmp(w, "cresc") || !strcmp(w, "si-") || !strcmp(w, "dim") || !strcmp(w, "si+")) {
        const char *wn = (!strcmp(w,"dim")||!strcmp(w,"si+")) ? "diminuendo" : "crescendo";
        DirEv de; memset(&de, 0, sizeof(de)); de.col = col; de.kind = 4;
        strncpy(de.text, wn, 23);
        if (g_ndirs<512){g_dirs[g_ndirs++] = de;} return;
    }
    if (!strcmp(w, "se")) {
        if (g_ndirs < 512) g_dirs[g_ndirs++] = (DirEv){col, 4, "stop", 0, 0};
        return;
    }
    if (tok[0] == '|' && (unsigned char)tok[1] == 0xE3) {
        if (g_ndirs < 512) g_dirs[g_ndirs++] = (DirEv){col, 5, "", 0, 0};
        return;
    }
    if (!strcmp(w, "rit.") || !strcmp(w, "rit")) {
        if (g_ndirs < 512) g_dirs[g_ndirs++] = (DirEv){col, 2, "rit.", 0, 0};
    } else if (!strcmp(w, "rall")) {
        if (g_ndirs < 512) g_dirs[g_ndirs++] = (DirEv){col, 2, "rall.", 0, 0};
    } else if (!strcmp(w, "accel.") || !strcmp(w, "accel")) {
        if (g_ndirs < 512) g_dirs[g_ndirs++] = (DirEv){col, 2, "accel.", 0, 0};
    } else if (!strcmp(w, "vi-")) {
        if (g_ndirs < 512) g_dirs[g_ndirs++] = (DirEv){col, 2, "rit.", 0, 0};
    } else if (!strcmp(w, "vi+")) {
        if (g_ndirs < 512) g_dirs[g_ndirs++] = (DirEv){col, 2, "accel.", 0, 0};
    } else if (!strcmp(w, "vb.") || !strcmp(w, "vb")) {
        if (g_ndirs < 512) g_dirs[g_ndirs++] = (DirEv){col, 2, "A tempo", gs->tempo, 0};
    } else if (!strncmp(w, "vb", 2) && isdigit((unsigned char)w[2])) {
        double b = atof(w + 2);
        if (b > 0 && !g_first_tempo) { g_first_tempo = 1; g_first_bpm = b; }
    }
    (void)gs;
}

static void write_direction_words(FILE *fp, int m, int measure_cols) {    static const char *dyn_names[10] = { "pppp", "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff", "ffff" };
    for (int i = 0; i < g_ndirs; i++) {
        if (g_dirs[i].col / measure_cols != m) continue;
        if (g_dirs[i].kind == 2) {
            fprintf(fp, "      <direction>\n        <direction-type><words>");
            write_xml_escaped(fp, g_dirs[i].text, 0);
            fprintf(fp, "</words></direction-type>\n");
            if (g_dirs[i].tempo > 0)
                fprintf(fp, "        <sound tempo=\"%.0f\"/>\n", g_dirs[i].tempo);
            fprintf(fp, "      </direction>\n");
        } else if (g_dirs[i].kind == 3 && g_dirs[i].level >= 1 && g_dirs[i].level <= 10) {
            fprintf(fp, "      <direction>\n        <direction-type><dynamics><%s/></dynamics></direction-type>\n      </direction>\n",
                    dyn_names[g_dirs[i].level - 1]);
        } else if (g_dirs[i].kind == 4) {
            fprintf(fp, "      <direction>\n        <direction-type><wedge type=\"%s\"/></direction-type>\n      </direction>\n",
                    g_dirs[i].text);
        } else if (g_dirs[i].kind == 5) {
            /* 延音号:挂当前列音符的 <fermata/>(见 write_note) */
        }
    }
}

/* 和弦行 → <harmony>(按列归入小节;同一小节多个和弦按列位 <offset> 码放;
 * kind 映射含 maj=增三和弦;cs/cn 为本作品的和弦区间) */
static void write_harmonies(FILE *fp, const Chord *cs, int cn, int m, int measure_cols, int is_first_part) {
    if (!is_first_part || !cs) return;
    int tpc = g_divisions / 4;      /* 列 tick(divisions=60 → 15) */
    int cursor = 0;                 /* 小节内已放到的列 */
    for (int i = 0; i < cn; i++) {
        const Chord *c = &cs[i];
        if (c->col / measure_cols != m) continue;
        if (c->is_none) continue;
        const char *kind = "major";
        if (c->is_maj7) kind = "major-seventh";
        else if (c->is_minor && c->has_seventh) kind = "minor-seventh";
        else if (c->is_minor) kind = "minor";
        else if (c->is_aug) kind = "augmented";
        else if (c->is_dim && c->has_seventh) kind = "diminished-seventh";
        else if (c->is_dim) kind = "diminished";
        else if (c->has_seventh) kind = "dominant";
        int off = c->col % measure_cols;
        fprintf(fp, "      <harmony>\n");
        if (off > cursor)
            fprintf(fp, "        <offset>%d</offset>\n", (off - cursor) * tpc);
        cursor = off;
        fprintf(fp, "        <root><root-step>%c</root-step>", c->root);
        if (c->root_alter) fprintf(fp, "<root-alter>%d</root-alter>", c->root_alter);
        fprintf(fp, "</root>\n        <kind>%s</kind>\n", kind);
        if (c->bass_n > 0) {
            fprintf(fp, "        <bass><bass-step>%c</bass-step>", c->bass_step);
            if (c->bass_alter) fprintf(fp, "<bass-alter>%d</bass-alter>", c->bass_alter);
            fprintf(fp, "</bass>\n");
        }
        fprintf(fp, "      </harmony>\n");
    }
}

/* ========================== XML 输出 ========================== */

static void write_pitch(FILE *fp, const Note *nt) {
    int oct = nt->octave;
    if (oct < 0 || oct > 9) {                  /* 保护:八度越界时归入 4/5 八度 */
        if (g_verbose) fprintf(stderr, "  !八度越界 %d(step %c),已修正\n", oct, nt->step);
        oct = (oct < 0) ? 4 : 9;
    }
    fprintf(fp, "        <pitch><step>%c</step>", nt->step);
    if (nt->alter != 0) fprintf(fp, "<alter>%d</alter>", nt->alter);
    fprintf(fp, "<octave>%d</octave></pitch>\n", oct);
}

static void unpitched_display(int key, char *step, int *octave) {
    static const struct { int key; char step; int octave; } map[] = {
        {36,'F',4},{38,'C',5},{40,'B',4},{42,'G',5},{45,'F',5},{46,'A',5},
        {49,'B',4},{50,'A',5},{51,'F',5},{54,'F',5},{56,'G',5},{64,'C',5},
        {70,'B',4},{76,'E',5}
    };
    *step = 'F'; *octave = 4;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (map[i].key == key) { *step = map[i].step; *octave = map[i].octave; return; }
}

static void write_note(FILE *fp, const Note *nt, int dur_ticks, int type_cols,
                       const char *lyric, int tie_start, int tie_stop) {
    const char *tname; int dot;
    dur_to_type(type_cols, &tname, &dot);

    /* MusicXML 不允许休止带延音端点；调用方的拆分逻辑仍可传入
     * 原始标记，但在最终输出边界统一清零。 */
    if (nt->is_rest) { tie_start = 0; tie_stop = 0; }

    fprintf(fp, "      <note>\n");
    if (nt->is_chord) fprintf(fp, "        <chord/>\n");
    if (nt->is_rest) {
        fprintf(fp, "        <rest/>\n");
    } else if (g_unpitched_part) {
        /* 无音高声部的 display 键位在写出阶段由 part 级默认值提供；
         * 当前 Note 未携带单件鼓键位时使用标准底鼓显示音。 */
        fprintf(fp, "        <unpitched><display-step>F</display-step><display-octave>4</display-octave></unpitched>\n");
    } else {
        write_pitch(fp, nt);
    }
    fprintf(fp, "        <duration>%d</duration>\n", dur_ticks);
    if (tie_start) fprintf(fp, "        <tie type=\"start\"/>\n");
    if (tie_stop)  fprintf(fp, "        <tie type=\"stop\"/>\n");
    fprintf(fp, "        <voice>1</voice>\n");
    fprintf(fp, "        <type>%s</type>\n", tname);
    if (dot) fprintf(fp, "        <dot/>\n");
    if (nt->tm_actual > 0) {
        fprintf(fp, "        <time-modification><actual-notes>%d</actual-notes>"
                    "<normal-notes>%d</normal-notes></time-modification>\n",
                nt->tm_actual, nt->tm_normal);
    }
    if (!nt->is_rest && !g_unpitched_part && nt->alter != 0 && !step_is_diatonic(g_fifths, nt->step, nt->alter)) {
        fprintf(fp, "        <accidental>%s</accidental>\n",
                nt->alter > 0 ? "sharp" : "flat");
    }
    if (tie_start || tie_stop || fermata_at(nt->col)) {
        fprintf(fp, "        <notations>\n");
        if (tie_start) fprintf(fp, "          <tied type=\"start\"/>\n");
        if (tie_stop)  fprintf(fp, "          <tied type=\"stop\"/>\n");
        if (fermata_at(nt->col)) fprintf(fp, "          <fermata/>\n");
        fprintf(fp, "        </notations>\n");
    }
    if (lyric && *lyric && !nt->is_rest) {
        fprintf(fp, "        <lyric><syllabic>single</syllabic><text>");
        write_xml_escaped(fp, lyric, 0);
        fprintf(fp, "</text></lyric>\n");
    }
    fprintf(fp, "      </note>\n");
}

/* 写一个音:无法用单一音值表达的时值拆为延音线相连的音型系列
 * (如 6/8 的 5 拍 = 10 列 → half 8 + eighth 2);休止拆为连续休止;
 * 连音(tm)/细分 tick 音不拆。tick 按列数比例分配,末片吸收余数。 */
static void write_note_series(FILE *fp, const Note *nt, int dur_ticks, int type_cols,
                              const char *lyric, int tie_start, int tie_stop) {
    const char *tn; int dot;
    if (type_cols <= 0 || nt->tm_actual > 0 || nt->dur_tick > 0 ||
        dur_exact(type_cols, &tn, &dot)) {
        write_note(fp, nt, dur_ticks, type_cols, lyric, tie_start, tie_stop);
        return;
    }
    int rem_c = type_cols, rem_t = dur_ticks, k = 0;
    while (rem_c > 0) {
        int c2 = 0;
        for (int i = 0; kDurTable[i].name; i++)
            if (kDurTable[i].cols <= rem_c) c2 = kDurTable[i].cols;
        if (c2 <= 0) {                       /* 拆不出(不会发生):原样单音 */
            write_note(fp, nt, rem_t, rem_c, lyric, tie_start, tie_stop);
            return;
        }
        int last = (rem_c == c2);
        int t2 = last ? rem_t : c2 * rem_t / rem_c;
        int ts = 0, tstop = 0;
        if (!nt->is_rest) {
            /* 拆分延音:首片起新 start、保留原 stop;末片保留原 start、收新 stop */
            if (k == 0) { ts = 1; tstop = tie_stop; }
            else if (last) { ts = tie_start; tstop = 1; }
            else { ts = 1; tstop = 1; }
        }
        write_note(fp, nt, t2, c2, (k == 0 ? lyric : NULL), ts, tstop);
        rem_c -= c2; rem_t -= t2; k++;
    }
}

static void write_attributes_kind(FILE *fp, const GroupState *gs, int kind) {
    fprintf(fp, "      <attributes>\n");
    fprintf(fp, "        <divisions>%d</divisions>\n", g_divisions);
    fprintf(fp, "        <key><fifths>%d</fifths><mode>%s</mode></key>\n",
            gs->fifths, gs->is_minor ? "minor" : "major");
    fprintf(fp, "        <time><beats>%d</beats><beat-type>%d</beat-type></time>\n",
            gs->beats, gs->beat_type);
    if (kind != INS_PITCHED)
        fprintf(fp, "        <clef><sign>percussion</sign><line>2</line></clef>\n");
    else
        fprintf(fp, "        <clef><sign>G</sign><line>2</line></clef>\n");
    fprintf(fp, "      </attributes>\n");
}

/* ========================== 组级辅助 ========================== */

typedef struct { int vidx; int group_idx; int work; } VRec;

/* 把结构 id 清洗为合法 XML NCName(折叠连字符;空则回退) */
static void make_ncname(const char *raw, int idx, char *out, int sz) {
    int k = 0;
    for (const char *p = raw; p && *p && k < sz - 1; p++) {
        char c = *p;
        if (isalnum((unsigned char)c) || c == '_') out[k++] = c;
        else if (k > 0 && out[k - 1] != '-') out[k++] = '-';
    }
    while (k > 0 && out[k - 1] == '-') k--;
    if (k == 0) { snprintf(out, sz, "P%d", idx); return; }
    out[k] = '\0';
}

/* 在组内各行的条目中查找与 col 同列的 | 段内容(供 = 段替换) */
/* 在组内查找能服务 target_idx 的方向段。上方 `|` 服务下方，
 * 下方 `\` 服务上方；同一列取离目标最近的一段。 */
static const char *find_aux_for_eq(ItemList *its, int n, int target_idx, int col) {
    const char *best = NULL;
    int best_dist = 1 << 30;
    for (int i = 0; i < n; i++) {
        if (i == target_idx) continue;
        int serves = (i < target_idx) ? 1 : 0;
        for (int k = 0; k < its[i].n; k++) {
            Item *it = &its[i].it[k];
            if (it->col != col || !it->text) continue;
            if ((serves && it->kind != IT_SEG_PIPE) || (!serves && it->kind != IT_SEG_BACK)) continue;
            int dist = i < target_idx ? target_idx - i : i - target_idx;
            if (dist < best_dist) { best = it->text; best_dist = dist; }
        }
    }
    return best;
}

static int row_has_eq_at(const ItemList *its, int col) {
    for (int k = 0; k < its->n; k++)
        if (its->it[k].kind == IT_EQ && its->it[k].col == col) return 1;
    return 0;
}

/* 对一个音符施加机动符号 */
static void apply_aux_symbol(Voice *v, int col, char sym) {
    if (!v) return;
    int best = -1;
    for (int k = 0; k < v->n; k++) {
        if (v->notes[k].is_rest || v->notes[k].is_chord) continue;
        if (v->notes[k].col == col) { best = k; break; }
        if (v->notes[k].col > col && best < 0) best = k;
    }
    if (best < 0) return;
    Note *nt = &v->notes[best];
    switch (sym) {
        case '+': nt->alter += 1; break;
        case '-': nt->alter -= 1; break;
        case '~': nt->alter = 0;  break;
        case '\'':
        case '*': nt->octave += 1; break;
        case '.': nt->octave -= 1; break;
        default: break;
    }
}

/* 一行的条目(带列偏移)应用到声部:text 为段内容 */
static void apply_segment(Ctx *ctx, Voice *v, const char *text, int base_col) {
    if (!text || !v) return;
    /* |@[...] / \@[...] 固定变音段：更新声部状态，并把增减同步到
     * 该列之后已经解析出的音符。这样辅助行在数字行之后处理时，
     * 仍然能影响同组后续事件。 */
    if (text[0] == '@') {
        GroupState tmpg = ctx->gs;
        int old_fixed[8];
        for (int f2 = 0; f2 < 8; f2++) old_fixed[f2] = v->fixed_alter[f2];
        parse_fixed_acc(text, &tmpg);
        for (int k = 0; k < v->n; k++) {
            Note *nt = &v->notes[k];
            if (nt->is_rest || nt->col < base_col) continue;
            for (int degree = 1; degree <= 7; degree++) {
                char st; int al, oct;
                degree_to_step_oct(&tmpg, degree, 0, &st, &al, &oct);
                if (st != nt->step) continue;
                nt->alter += tmpg.fixed_alter[degree] - old_fixed[degree];
                break;
            }
        }
        for (int f2 = 0; f2 < 8; f2++) v->fixed_alter[f2] = tmpg.fixed_alter[f2];
        if (g_verbose) fprintf(stderr, "  固定变音段(列%d)→ 声部固定变音已更新\n", base_col);
        return;
    }
    char *tmp = dup_str(text);
    for (char *p = tmp; *p; p++) if (*p == '/') *p = ' ';    /* 机动行空格替代 */
    ItemList its; memset(&its, 0, sizeof(its));
    scan_items(tmp, (int)strlen(tmp), &ctx->gs, &its);
    for (int k = 0; k < its.n; k++) {
        Item *it = &its.it[k];
        it->col += base_col;
        if (it->kind == IT_IGNORE) apply_aux_symbol(v, it->col, (char)it->degree);
    }
    row_to_voice(ctx, &its, v, base_col, 1);
    for (int k = 0; k < its.n; k++) free(its.it[k].text);
    free(its.it);
    free(tmp);
}

/* @lN 行复制:把文档第 src 行的内容按列复制到声部;M != 0 时生成平行和声
 * (lN^±M:在该音上方/下方 M 度置平行音;M 度 = 调内平移 M-1 个音级) */
static void apply_line_copy(Ctx *ctx, Row *rows, int nlines, int src, int from_col, Voice *v, int M) {
    if (src < 1 || src > nlines) return;
    Row *r = &rows[src - 1];
    ItemList its; memset(&its, 0, sizeof(its));
    scan_items(r->music, (int)strlen(r->music), &ctx->gs, &its);
    int k = 0;
    while (k < its.n && its.it[k].col < from_col) k++;
    if (k > 0) { memmove(its.it, its.it + k, (its.n - k) * sizeof(Item)); its.n -= k; }
    int before = v->n;
    row_to_voice(ctx, &its, v, from_col, 1);
    if (M != 0) {
        /* 平行音:对每个新复制音生成上方/下方 M 度的同调音 */
        static const char snames[7] = { 'C', 'D', 'E', 'F', 'G', 'A', 'B' };
        int end = v->n;                 /* 固化上界:voice_push 会在循环中增长 v->n */
        for (int q = before; q < end; q++) {
            Note *srcn = &v->notes[q];
            if (srcn->is_rest) continue;
            int si = -1;
            for (int w = 0; w < 7; w++) if (snames[w] == srcn->step) si = w;
            if (si < 0) continue;
            int steps = M > 0 ? (M - 1) : -((-M) - 1);
            int idx = (si + steps + 700) % 7;
            Note nt = *srcn;
            nt.is_chord = 1;
            nt.step = snames[idx];
            int o8 = (si + steps) / 7 - si / 7;
            if (M > 0 && idx < si) o8++;       /* 字母回绕 */
            if (M < 0 && idx > si) o8--;
            nt.octave += o8;
            nt.tm_actual = nt.tm_normal = 0;
            nt.dur_tick = 0; nt.type_cols = 0;
            voice_push(v, nt);
        }
    }
    for (int q = 0; q < its.n; q++) free(its.it[q].text);
    free(its.it);
}

/* 提取 *l"..." / *l|"..." / *l\"..." 的歌词正文。
 * 返回值由调用者释放；below=1 表示歌词服务下方数字行，默认服务上方行。 */
static char *extract_lyric_text(const char *music, int *below) {
    if (below) *below = 0;
    if (!music) return NULL;
    const char *p = strstr(music, "*l");
    if (!p) return NULL;
    p += 2;
    if (*p == '|') p++;
    else if (*p == '\\') { if (below) *below = 1; p++; }
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') p++;
    const char *q = strchr(p, '"');
    if (!q) {
        q = p + strlen(p);
        fprintf(stderr, "警告:歌词引号未闭合，按行尾截断\n");
    }
    size_t n = (size_t)(q - p);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n); out[n] = '\0';
    return out;
}

/* 从歌词正文中取下一个句元。空格只用于分隔句元，标点和其它字符原样保留。 */
static const char *next_lyric_token(const char *p, char *out, int outsz, int *is_hold) {
    if (is_hold) *is_hold = 0;
    if (!p || !out || outsz <= 1) return p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (!*p) { out[0] = '\0'; return p; }
    if (*p == '-') {
        if (is_hold) *is_hold = 1;
        out[0] = '\0';
        return p + 1;
    }
    int ul = utf8_len_of(p);
    if (ul >= 3) {
        int n = ul < outsz - 1 ? ul : outsz - 1;
        memcpy(out, p, n); out[n] = '\0';
        return p + ul;
    }
    int n = 0;
    while (p[n] && p[n] != ' ' && p[n] != '\t' && n < outsz - 1) n++;
    memcpy(out, p, n); out[n] = '\0';
    return p + n;
}


static void parse_chords_row(const char *music, int len, GroupState *gs) {
    int i = 0;
    while (i < len) {
        while (i < len && music[i] == ' ') i++;
        if (i >= len) break;
        int j = i;
        while (j < len && music[j] != ' ') j++;
        char buf[32]; int n = j - i; if (n > 31) n = 31;
        memcpy(buf, music + i, n); buf[n] = '\0';
        if (buf[0] == '|' || buf[0] == '\\') {   /* 方向标记(孤立 | 不构成和弦) */
            record_direction(i, buf, gs);
            i = j;
            continue;
        }
        Chord c; parse_chord_token(buf, i, &c);
        if (!c.is_none) {
            int ri = 0;
            for (int k = 0; k < 7; k++)
                if ("CDEFGAB"[k] == toupper((unsigned char)c.root)) ri = k;
            /* 方法 1(|=K 视同简谱):和弦行跟随谱面一道转调——和弦字母即调内
             * 级数名,按目标调号逐级映射(|=C 恒等;后缀 ~ 时 chord_follow=0) */
            if (gs->chord_follow) {
                char st; int al;
                degree_to_step_oct(gs, ri + 1, c.root_alter, &st, &al, NULL);
                c.root = st; c.root_alter = al;
            }
            if (c.bass_n > 0)   /* 根音位置标注按 §7.7 消解为低音(解析时固化) */
                chord_bass_resolve(&c, gs, &c.bass_step, &c.bass_alter);
        }
        if (gs->nchords >= gs->chordcap) {
            gs->chordcap = gs->chordcap ? gs->chordcap * 2 : 16;
            gs->chords = (Chord *)realloc(gs->chords, gs->chordcap * sizeof(Chord));
        }
        gs->chords[gs->nchords++] = c;
        i = j;
    }
}

/* 组头行:解析 { ... } 内的元数据 + 之后的标记定义 */
static void parse_group_head_row(const char *head, const char *music, GroupState *gs) {
    const char *p = head;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '{') p++;
    while (*p) {
        while (*p == ' ') p++;
        if (*p == '}') break;
        if (*p == '|') {
            if (p[1] == '=' || p[1] == '@' || p[1] == '-') parse_keyspec(p, gs);
            else if (isdigit((unsigned char)p[1])) parse_timesig(p + 1, gs);
            else if (p[1] == 'v' || p[1] == 'r' || p[1] == 'a' ||
                     p[1] == 's' || p[1] == 'c' || p[1] == 'd') parse_tempo_mark(p, gs);
            while (*p && *p != ' ') p++;
        } else if (!strncmp(p, "&gt", 3)) {
            parse_tuning(p, gs);
            while (*p && *p != ' ') p++;
        } else {
            p++;
        }
    }
    /* 标记定义:整行扫(元数据区之后的乐曲区) */
    if (music) {
        int i = 0, len = (int)strlen(music);
        while (i < len) {
            while (i < len && music[i] == ' ') i++;
            if (i >= len) break;
            if (music[i] == '|') {                  /* |rit. |vb. 等方向记号:记录列 */
                record_direction(i, music + i, gs);
                while (i < len && music[i] != ' ') i++;
                continue;
            }
            if (music[i] == '{') {                  /* ④类把位定义 {Cmy/300353} / 组头 {D/200232} */
                int j = i + 1;
                while (j < len && music[j] != '}' && music[j] != ' ') j++;
                char tok[48]; int tn = j - i;
                if (tn > 47) tn = 47;
                memcpy(tok, music + i, tn); tok[tn] = '\0';
                const char *slash = strchr(tok, '/');
                if (slash) {
                    char nm[16]; int nl = (int)(slash - tok) - 1;   /* 去掉 '{' */
                    if (nl > 15) nl = 15;
                    memcpy(nm, tok + 1, nl); nm[nl] = '\0';
                    const char *fr = slash + 1;
                    int fl = 0;
                    while (fr[fl] && fr[fl] != '}' && fr[fl] != ' ' && fl < 7) fl++;
                    char fbuf[8];
                    memcpy(fbuf, fr, fl); fbuf[fl] = '\0';
                    add_voicing(gs, nm, fbuf);
                }
                while (i < len && music[i] != ' ') i++;
                continue;
            }
            MarkDef d; int used = 0;
            if (parse_mark_def(music + i, &used, &d) && used > 0) {
                add_mark_def(gs, &d);
                i += used;
            } else {
                while (i < len && music[i] != ' ') i++;
            }
        }
    }
}

/* 从组头行的 head 提取乐器(伴奏行头六形态) */
static void parse_instrument_head(const char *head, char *id, int idsz,
                                  char *name, int namesz) {
    id[0] = name[0] = '\0';
    const char *p = head;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '(') {
        p++;
        if (!strncmp(p, "Accomponiment", 13)) {
            p += 13;
            if (*p == '#') p++;
            while (isdigit((unsigned char)*p)) p++;      /* 伴奏序号(非结构 id) */
            if (*p == ':') p++;
            const char *lt = strchr(p, '<');
            const char *semi = strchr(p, ';');
            if (lt) {
                int k = 0;
                while (p < lt && k < idsz - 1) id[k++] = *p++;
                id[k] = '\0';
                const char *e = semi ? semi : lt + 1 + strlen(lt + 1);
                p = lt + 1; k = 0;
                while (p < e && k < namesz - 1) name[k++] = *p++;
                name[k] = '\0';
            } else {
                int k = 0;
                while (*p && *p != ';' && k < idsz - 1) id[k++] = *p++;
                id[k] = '\0';
            }
        } else if (*p == '-') {                 /* (-Inst.#0- */
            p++;
            int k = 0;
            while (*p && *p != '-' && k < idsz - 1) id[k++] = *p++;
            id[k] = '\0';
        }
    }
}

int tymp2musicxml(const char *src, const char *dst, int nolyrics) {
    int nlines = 0, *linenos = NULL;
    char **lines = read_all_lines(src, &nlines, &linenos);
    if (!lines || nlines < 3) { fprintf(stderr, "无法读取文件: %s\n", src); return 1; }

    Ctx ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.gs.beats = 4; ctx.gs.beat_type = 4; ctx.gs.accu = 4;
    ctx.gs.key_root = 'C'; ctx.gs.fifths = 0; ctx.gs.tempo = 120; ctx.gs.chord_follow = 1;
    ctx.opt_nolyrics = nolyrics;
    ctx.opt_verbose = g_verbose;

    /* ---- 标题 / 作者 / 乐器行 ---- */
    char title[256] = {0}, author[256] = {0};
    char *by = strstr(lines[0], " by ");
    if (by) {
        int n = (int)(by - lines[0]); if (n > 255) n = 255;
        memcpy(title, lines[0], n); title[n] = '\0';
        strncpy(author, by + 4, 255);
    } else {
        strncpy(title, lines[0], 255);
    }

    /* 收集行 */
    Row *rows = (Row *)calloc(nlines, sizeof(Row));
    for (int i = 0; i < nlines; i++) {
        strip_trailing_spaces(lines[i]);
        int me = find_metadata_end(lines[i]);
        rows[i].lineno = i + 1;
        if (me < 0) { rows[i].head = dup_str(lines[i]); rows[i].music = dup_str(""); }
        else {
            char *h = (char *)malloc(me + 1);
            memcpy(h, lines[i], me); h[me] = '\0';
            rows[i].head = h;
            rows[i].music = dup_str(lines[i] + me);
        }
    }

    FILE *fp = initNewMusicXML((char *)dst);
    if (!fp) { fprintf(stderr, "无法写出: %s\n", dst); return 1; }
    g_fp = fp;

    /* ---- 组织结构:每组 = 组头行 + 若干内容行 ---- */
    typedef struct { Row *head; Row *body[512]; int nbody; int idx; int work; } Group;
    Group *groups = (Group *)calloc(nlines + 1, sizeof(Group));
    int ngroups = 0;
    Group *cg = NULL;
    int cur_work = 0;
    char work_titles[8][256]; int work_titles_n = 0;
    char work_authors[8][256];
    for (int i = 0; i < nlines; i++) {
        Row *r = &rows[i];
        const char *h = r->head;
        if (strstr(h, "#timestamp")) continue;
        if (r->lineno > 2 && h[0] != ' ' && h[0] != '\t' && strstr(h, " by ")) {
            /* 新作品标题行 */
            cur_work++;
            if (cur_work < 8) {
                const char *by = strstr(h, " by ");
                int n = by ? (int)(by - h) : (int)strlen(h);
                if (n > 255) n = 255;
                memcpy(work_titles[cur_work], h, n); work_titles[cur_work][n] = '\0';
                strncpy(work_authors[cur_work], by ? by + 4 : "", 255);
                work_authors[cur_work][255] = '\0';
                if (cur_work + 1 > work_titles_n) work_titles_n = cur_work + 1;
            }
            continue;
        }
        if (!strncmp(h, "instruments:", 12)) continue;
        int ghead = 0, gtail = 0;
        for (const char *p = h; *p; p++) {
            if (*p == ' ' || *p == '\t') continue;
            if (*p == '{') ghead = 1;
            if (*p == '}') gtail = 1;
            break;
        }
        if (ghead) { cg = &groups[ngroups]; cg->head = r; cg->nbody = 0; cg->idx = ngroups; cg->work = cur_work; ngroups++; continue; }
        if (gtail) {
            /* 组末元数据:力度/速度等方向记号(全局) */
            for (const char *p2 = r->music; *p2; ) {
                while (*p2 == ' ') p2++;
                if (!*p2) break;
                if (*p2 == '|') {
                    record_direction((int)(p2 - r->music), p2, &ctx.gs);
                    while (*p2 && *p2 != ' ') p2++;
                } else break;
            }
            cg = NULL; continue;
        }
        if (cg && cg->nbody < 512) cg->body[cg->nbody++] = r;
    }

    VRec *vrecs = NULL; int nvrec = 0, vreccap = 0;
    int melody_seen = 0, harm_no = 0;

    /* ---- 逐组处理 ---- */
    /* 和弦行归各自作品:记录每个作品的和弦下标区间(全局表累积,写入时按区间取) */
    int work_ch_start[8], work_ch_end[8];
    for (int k = 0; k < 8; k++) { work_ch_start[k] = work_ch_end[k] = 0; }
    int cur_wch = -1;
    for (int gi = 0; gi < ngroups; gi++) {
        Group *g = &groups[gi];
        if (!g->head) continue;
        if (g->work != cur_wch) {
            if (cur_wch >= 0 && cur_wch < 8) work_ch_end[cur_wch] = ctx.gs.nchords;
            cur_wch = g->work > 7 ? 7 : g->work;
            work_ch_start[cur_wch] = ctx.gs.nchords;
        }
        parse_group_head_row(g->head->head, g->head->music, &ctx.gs);

        int nb = g->nbody;
        if (nb == 0) continue;
        ItemList *its = (ItemList *)calloc(nb, sizeof(ItemList));
        int *isnum = (int *)calloc(nb, sizeof(int));
        Voice **vof = (Voice **)calloc(nb, sizeof(Voice *));

        int lyric_rows[512]; int lyric_below[512]; char *lyric_texts[512];
        int nlyric = 0;      /* 歌词行(附着用) */
        for (int i = 0; i < nb; i++) {
            Row *r = g->body[i];
            const char *h = r->head;
            int lbelow = 0;
            char *ltext = extract_lyric_text(r->music, &lbelow);
            if (ltext || strstr(h, "liric") || strstr(h, "lyric")) {
                if (nlyric < 512) {
                    lyric_rows[nlyric] = i;
                    lyric_below[nlyric] = lbelow;
                    lyric_texts[nlyric] = ltext;
                    nlyric++;
                } else free(ltext);
                continue;
            }
            if (strstr(h, "chords")) { parse_chords_row(r->music, (int)strlen(r->music), &ctx.gs); continue; }
            scan_items(r->music, (int)strlen(r->music), &ctx.gs, &its[i]);
        }

        /* 行角色:含事件者为数字行(段/单独符号 不算事件) */
        for (int i = 0; i < nb; i++) {
            int he = 0;
            for (int k = 0; k < its[i].n; k++) {
                int kk = its[i].it[k].kind;
                if (kk == IT_NOTE || kk == IT_REST || kk == IT_MARK || kk == IT_EQ || kk == IT_COPY) { he = 1; break; }
            }
            isnum[i] = he;
        }

        /* | 段 → 同列 = 段的替换内容 */
        for (int i = 0; i < nb; i++) {
            if (!isnum[i]) continue;
            for (int k = 0; k < its[i].n; k++) {
                if (its[i].it[k].kind != IT_EQ) continue;
                const char *pc = find_aux_for_eq(its, nb, i, its[i].it[k].col);
                if (pc) its[i].it[k].text = dup_str(pc);
                else if (g_verbose)
                    fprintf(stderr, "警告:数字行%d列%d的 = 没有配对机动段\n",
                            i, its[i].it[k].col);
            }
        }

        /* 数字行 → 声部 */
        for (int i = 0; i < nb; i++) {
            if (!isnum[i]) continue;
            Row *r = g->body[i];
            const char *h = r->head;
            char iid[64] = {0}, iname[128] = {0};
            const char *hp = h; while (*hp == ' ' || *hp == '\t') hp++;
            if (*hp == '(') parse_instrument_head(h, iid, sizeof(iid), iname, sizeof(iname));

            if (g_verbose) {
                fprintf(stderr, "L%-4d 条目=%d:", r->lineno, its[i].n);
                for (int q = 0; q < its[i].n && q < 12; q++)
                    fprintf(stderr, " %d@%d", its[i].it[q].kind, its[i].it[q].col);
                fprintf(stderr, "\n");
            }

            Voice *v = new_voice(&ctx);
            v->row_no = r->lineno;
            if (iid[0]) {
                char cid[64];
                make_ncname(iid, ctx.nvoices, cid, sizeof(cid));
                v->id = dup_str(cid);
                v->name_en = dup_str(iname[0] ? iname : iid);
                v->program = 1; v->kind = INS_PITCHED;
                /* 查乐器原型:无音高/鼓组识别(方案 §7.10) */
                for (int pi2 = 0; pi2 < NUM_OF_INSTRUMENT_PROTS; pi2++) {
                    const InstrumentPrototype *pr = &InstrumentPrototypeList[pi2];
                    if (!pr->base_id) continue;
                    if (strstr(iid, pr->base_id)) {
                        v->kind = pr->kind;
                        v->unpitched = pr->unpitched;
                        v->program = pr->gm_program > 0 ? pr->gm_program : 1;
                        if (!iname[0]) { free(v->name_en); v->name_en = dup_str(pr->base_name); }
                        break;
                    }
                }
            } else if (melody_seen == 0) {
                v->name_en = dup_str("Melody"); v->id = dup_str("P0"); melody_seen = 1;
            } else {
                char nm[32], cid[16];
                snprintf(nm, sizeof(nm), "Harmony %d", ++harm_no);
                snprintf(cid, sizeof(cid), "H%d", harm_no);
                v->name_en = dup_str(nm); v->id = dup_str(cid);
            }
            v->octave_base = 4;
            const char *pr = strchr(h, '>');
            const char *ps = strchr(h, '*');
            if (pr) v->octave_base = atoi(pr + 1);
            else if (ps && isdigit((unsigned char)ps[1])) v->octave_base = atoi(ps + 1);
            v->anchor_oct = v->octave_base;
            for (int f2 = 0; f2 < 8; f2++) v->fixed_alter[f2] = ctx.gs.fixed_alter[f2];
            vof[i] = v;

            row_to_voice(&ctx, &its[i], v, 0, 0);

            /* @lN 行复制 / @lN^±M 平行和声 */
            for (int k = 0; k < its[i].n; k++)
                if (its[i].it[k].kind == IT_COPY)
                    apply_line_copy(&ctx, rows, nlines, its[i].it[k].degree,
                                    its[i].it[k].col, v, its[i].it[k].shift);

            if (nvrec >= vreccap) {
                vreccap = vreccap ? vreccap * 2 : 32;
                vrecs = (VRec *)realloc(vrecs, vreccap * sizeof(VRec));
            }
            vrecs[nvrec].vidx = ctx.nvoices - 1;
            vrecs[nvrec].group_idx = gi;
            vrecs[nvrec].work = g->work;
            nvrec++;
        }

        /* 机动行绑定:-- \ 段 → 上方最近数字行;单独符号 → 下方最近数字行 */
        for (int a = 0; a < nb; a++) {
            int above = -1, below = -1;
            for (int j = a - 1; j >= 0; j--) if (isnum[j] && vof[j]) { above = j; break; }
            for (int j = a + 1; j < nb; j++) if (isnum[j] && vof[j]) { below = j; break; }
            for (int k = 0; k < its[a].n; k++) {
                Item *it = &its[a].it[k];
                if ((it->kind == IT_SEG_BACK || it->kind == IT_SEG_PIPE) && it->text && it->text[0] == '@') {
                    /* 固定变音段直接作用于目标声部；数字行已经先完成基础
                     * 解析，因此 apply_segment 会按列修正后续音符并更新状态。 */
                    int tgt = (it->kind == IT_SEG_BACK) ? above : below;
                    if (tgt >= 0) apply_segment(&ctx, vof[tgt], it->text, it->col);
                    continue;
                }
                if (it->kind == IT_SEG_BACK) {
                    if (above >= 0 && it->text) {
                        if (g_verbose)
                            fprintf(stderr, "  段: L%d 的 \\段(列%d)→ 应用至 L%d: [%.40s]\n",
                                    g->body[a]->lineno, it->col, g->body[above]->lineno, it->text);
                        apply_segment(&ctx, vof[above], it->text, it->col);
                    }
                }
                if (it->kind == IT_SEG_PIPE) {
                    /* 上指管道段服务下方数字行；除 = 替换外，
                     * 数字行自身携带的段也要注入其下方目标行。 */
                    if (below >= 0 && it->text && !row_has_eq_at(&its[below], it->col))
                        apply_segment(&ctx, vof[below], it->text, it->col);
                } else if (it->kind == IT_IGNORE) {
                    if (below >= 0) apply_aux_symbol(vof[below], it->col, (char)it->degree);
                }
            }
        }

        /* 目标行条目按列重排(固定变音段插入后) */
        for (int i2 = 0; i2 < nb; i2++) {
            if (!isnum[i2]) continue;
            for (int a2 = its[i2].n - 1; a2 > 0; a2--) {
                for (int b2 = 0; b2 < a2; b2++) {
                    if (its[i2].it[b2].col > its[i2].it[a2].col) {
                        Item t2 = its[i2].it[a2]; its[i2].it[a2] = its[i2].it[b2]; its[i2].it[b2] = t2;
                    }
                }
            }
        }

        /* 歌词附着(逐字句一一对应:按序附着到下方数字行的各音) */
        if (!nolyrics) {
            for (int li = 0; li < nlyric; li++) {
                int lr = lyric_rows[li];
                int below = lyric_below[li] ? -1 : -1;
                int target = lyric_below[li] ? -1 : -1;
                if (lyric_below[li]) {
                    for (int j = lr + 1; j < nb; j++) if (isnum[j] && vof[j]) { target = j; break; }
                } else {
                    for (int j = lr - 1; j >= 0; j--) if (isnum[j] && vof[j]) { target = j; break; }
                    if (target < 0)
                        for (int j = lr + 1; j < nb; j++) if (isnum[j] && vof[j]) { target = j; break; }
                }
                below = target;
                if (below < 0) continue;
                const char *txt = lyric_texts[li] ? lyric_texts[li] : g->body[lr]->music;
                char token[128]; int hold = 0;
                for (int k = 0; k < vof[below]->n; k++) {
                    Note *nt = &vof[below]->notes[k];
                    if (nt->is_rest || nt->is_chord) continue;
                    const char *next = next_lyric_token(txt, token, sizeof(token), &hold);
                    if (!*token && !hold && next == txt) break;
                    if (hold) {
                        nt->tie_lyric = 1;
                        txt = next;
                        continue;
                    }
                    if (!*token) break;
                    free(nt->lyric);
                    nt->lyric = dup_str(token);
                    txt = next;
                }
            }
        }

        for (int i = 0; i < nlyric; i++) free(lyric_texts[i]);

        for (int i = 0; i < nb; i++) {
            for (int k = 0; k < its[i].n; k++) free(its[i].it[k].text);
            free(its[i].it);
        }
        free(its); free(isnum); free(vof);
    }
    if (cur_wch >= 0 && cur_wch < 8) work_ch_end[cur_wch] = ctx.gs.nchords;
    free(groups);

    /* ---- 输出(按作品分组;每作品一个 .musicxml) ---- */
    Score sc; memset(&sc, 0, sizeof(sc));
    sc.work_title = title; sc.composer = author;
    sc.parts = NULL; sc.nparts = nvrec;

    int nworks = work_titles_n > 0 ? work_titles_n : 1;

    int measure_cols = 16 * ctx.gs.beats / ctx.gs.beat_type;
    if (measure_cols <= 0) measure_cols = 16;
    int ticks_per_col = g_divisions / 4;
    g_fifths = ctx.gs.fifths;

    /* --- 预展开:按小节边界拆分跨小节音符(带 tie) --- */
    Note **obuf = (Note **)calloc(nvrec ? nvrec : 1, sizeof(Note *));
    int    *on   = (int *)calloc(nvrec ? nvrec : 1, sizeof(int));
    int    *ocap = (int *)calloc(nvrec ? nvrec : 1, sizeof(int));
    int max_measures = 0;
    for (int i = 0; i < nvrec; i++) {
        Voice *v = &ctx.voices[vrecs[i].vidx];
        for (int k = 0; k < v->n; k++) {
            Note *nt = &v->notes[k];
            int col = nt->col, dur = nt->dur > 0 ? nt->dur : 1;
            int off0 = col % measure_cols;
            int avail0 = measure_cols - off0;
            if (nt->dur_tick > 0 && dur > avail0) {
                /* 同列连音组跨小节:组整体按比例拆分,组内逐音分配取整余数 */
                int gn = 1, T = nt->dur_tick;
                while (k + gn < v->n && v->notes[k + gn].col == col
                       && v->notes[k + gn].dur_tick > 0) { T += v->notes[k + gn].dur_tick; gn++; }
                int d1 = avail0, d2 = dur - d1;
                int T1 = T * d1 / dur;
                int fl_sum = 0;
                for (int q = 0; q < gn; q++)
                    fl_sum += v->notes[k + q].dur_tick * d1 / dur;
                int deficit = T1 - fl_sum;               /* ≤ gn,分给组内前 deficit 音 */
                for (int q = 0; q < gn; q++) {
                    Note *nn = &v->notes[k + q];
                    int t1 = nn->dur_tick * d1 / dur + (q < deficit ? 1 : 0);
                    for (int piece = 0; piece < 2; piece++) {
                        if (on[i] >= ocap[i]) {
                            ocap[i] = ocap[i] ? ocap[i] * 2 : 128;
                            obuf[i] = (Note *)realloc(obuf[i], ocap[i] * sizeof(Note));
                        }
                        Note tt = *nn;
                        tt.col = piece ? col + d1 : col;
                        tt.dur = piece ? d2 : d1;
                        tt.dur_tick = piece ? nn->dur_tick - t1 : t1;
                        tt.tie_start = piece ? nn->tie_start : 1;   /* 首片起拆分音;末片保留原 start */
                        tt.tie_stop  = piece ? 1 : nn->tie_stop;    /* 首片保留原 stop;末片收拆分音 */
                        if (tt.is_rest) { tt.tie_start = 0; tt.tie_stop = 0; }
                        obuf[i][on[i]++] = tt;
                    }
                }
                k += gn - 1;
                continue;
            }
            int first = 1, tick_done = 0;
            while (dur > 0) {
                int off = col % measure_cols;
                int avail = measure_cols - off;
                int d = dur > avail ? avail : dur;
                if (on[i] >= ocap[i]) {
                    ocap[i] = ocap[i] ? ocap[i] * 2 : 128;
                    obuf[i] = (Note *)realloc(obuf[i], ocap[i] * sizeof(Note));
                }
                Note tt = *nt;
                tt.col = col; tt.dur = d;
                /* 拆分延音:首片保留原 stop、起新 start;末片保留原 start、收新 stop */
                tt.tie_start = (dur > avail) ? 1 : nt->tie_start;
                tt.tie_stop  = first ? nt->tie_stop : 1;
                if (tt.is_rest) { tt.tie_start = 0; tt.tie_stop = 0; }
                if (nt->dur_tick > 0) {                  /* 连音音按列数比例分 tick(向下取整,末片吸收余数) */
                    tt.dur_tick = (dur > avail) ? (nt->dur_tick * d) / nt->dur
                                                : nt->dur_tick - tick_done;
                    tick_done += tt.dur_tick;
                }
                obuf[i][on[i]++] = tt;
                col += d; dur -= d; first = 0;
            }
        }
        qsort(obuf[i], on[i], sizeof(Note), note_cmp);
        normalize_note_ties(obuf[i], on[i]);
        for (int k = 0; k < on[i]; k++) {
            int m = obuf[i][k].col / measure_cols + 1;
            if (m > max_measures) max_measures = m;
        }
    }

    for (int w = 0; w < nworks; w++) {
        FILE *wfp;
        if (w == 0) {
            wfp = fp;
        } else {
            char wname[1024];
            snprintf(wname, sizeof(wname), "%s", dst);
            if (ifStrEndwith(wname, ".musicxml") || ifStrEndwith(wname, ".xml")) {
                char *dot = strrchr(wname, '.');
                if (dot) *dot = '\0';
            }
            int nl = (int)strlen(wname);
            snprintf(wname + nl, sizeof(wname) - nl, "-%d.musicxml", w + 1);
            wfp = fopen(wname, "w");
            if (!wfp) continue;
            fputs(xmlHead, wfp);
        }

        const char *wt = (w > 0 && w < work_titles_n) ? work_titles[w] : title;
        const char *wa = (w > 0 && w < work_titles_n) ? work_authors[w] : author;

        fprintf(wfp, "  <work><work-title>"); write_xml_escaped(wfp, wt, 0); fprintf(wfp, "</work-title></work>\n");
        fprintf(wfp, "  <identification><creator type=\"composer\">"); write_xml_escaped(wfp, wa, 0); fprintf(wfp, "</creator></identification>\n");
        fprintf(wfp, "  <part-list>\n");
        int chan = 0;
        char used_ids[64][64]; int nused_ids = 0;
        char output_ids[512][64]; memset(output_ids, 0, sizeof(output_ids));
        for (int i = 0; i < nvrec; i++) {
            if (vrecs[i].work != w) continue;
            Voice *v = &ctx.voices[vrecs[i].vidx];
            char pid[64];
            snprintf(pid, sizeof(pid), "%s", v->id ? v->id : "V");
            for (char *q = pid; *q; q++) if (*q == '#' || *q == '.' || *q == '(' || *q == ')') *q = '-';
            {   /* ID 独一化:与本作品内已用 id 去重 */
                int dup = 1;
                while (dup) {
                    dup = 0;
                    for (int u2 = 0; u2 < nused_ids; u2++)
                        if (!strcmp(used_ids[u2], pid)) { dup = 1; break; }
                    if (dup) {
                        int pl = (int)strlen(pid);
                        if (pl < 58) {
                            snprintf(pid + pl, sizeof(pid) - (size_t)pl, "-%d", nused_ids + 1);
                        } else {
                            pid[57] = '\0';
                        }
                    }
                }
                if (nused_ids < 64) {
                    strncpy(used_ids[nused_ids], pid, 63);
                    used_ids[nused_ids][63] = '\0';
                    nused_ids++;
                }
            }
            free(v->xml_id);
            v->xml_id = dup_str(pid);
            fprintf(wfp, "    <score-part id=\"%s\">\n", pid);
            fprintf(wfp, "      <part-name>"); write_xml_escaped(wfp, v->name_en ? v->name_en : "Part", 0); fprintf(wfp, "</part-name>\n");
            fprintf(wfp, "      <score-instrument id=\"%s-I1\">\n        <instrument-name>", pid);
            write_xml_escaped(wfp, v->name_en ? v->name_en : "Part", 0);
            fprintf(wfp, "</instrument-name>\n      </score-instrument>\n");
            if (v->kind == INS_UNPITCHED) {
                fprintf(wfp, "      <midi-instrument id=\"%s-I1\">\n        <midi-channel>%d</midi-channel>\n        <midi-unpitched>%d</midi-unpitched>\n      </midi-instrument>\n",
                        pid, (chan++ % 15) + 1, v->unpitched > 0 ? v->unpitched : 38);
            } else {
                fprintf(wfp, "      <midi-instrument id=\"%s-I1\">\n        <midi-channel>%d</midi-channel>\n        <midi-program>%d</midi-program>\n      </midi-instrument>\n",
                        pid, (chan++ % 15) + 1, v->program > 0 ? v->program : 1);
            }
            fprintf(wfp, "    </score-part>\n");
        }
        fprintf(wfp, "  </part-list>\n");

    /* parts:每声部按其音符列切分小节(使用预展开缓冲 obuf) */
    int pi = 0;
    for (int i = 0; i < nvrec; i++) {
        if (vrecs[i].work != w) continue;
        int is_first_part = (pi == 0);
        pi++;
        Voice *v = &ctx.voices[vrecs[i].vidx];
        g_unpitched_part = (v->kind != INS_PITCHED);
        char pid[64];
        snprintf(pid, sizeof(pid), "%s", v->xml_id ? v->xml_id : (v->id ? v->id : "V"));
        for (char *q = pid; *q; q++) if (*q == '#' || *q == '.' || *q == '(' || *q == ')') *q = '-';
        fprintf(wfp, "  <part id=\"%s\">\n", pid);

        for (int m = 0; m < max_measures; m++) {
            fprintf(wfp, "    <measure number=\"%d\">\n", m + 1);
            if (m == 0) write_attributes_kind(wfp, &ctx.gs, v->kind);
            if (m == 0 && (g_first_tempo || ctx.gs.tempo_valid)) {
                double bpm = g_first_tempo ? g_first_bpm : ctx.gs.tempo;
                fprintf(wfp, "      <direction>\n        <direction-type>"
                            "<metronome><beat-unit>quarter</beat-unit><per-minute>%.0f</per-minute></metronome>"
                            "</direction-type>\n        <sound tempo=\"%.0f\"/>\n      </direction>\n",
                        bpm, bpm);
            }
            write_direction_words(wfp, m, measure_cols);
            write_harmonies(wfp, ctx.gs.chords + work_ch_start[w], work_ch_end[w] - work_ch_start[w],
                            m, measure_cols, is_first_part);

            int wrote = 0, cursor = 0, last_off = -1, last_col = -1, last_tick = 0;
            for (int k = 0; k < on[i]; k++) {
                Note *nt = &obuf[i][k];
                int nm = nt->col / measure_cols;
                if (nm != m) continue;
                int off = nt->col % measure_cols;
                /* 同一声部的和弦续音随首音一起输出,但不应继承跨事件的 tie 标志:
             * tie 只属于对应 pitch 的连续音符;复制声部中同刻新柱式和弦不能伪造 stop。 */
            if (nt->is_chord) {
                if (off != last_off) continue;
                int dtick2 = nt->dur_tick > 0 ? nt->dur_tick : nt->dur * ticks_per_col;
                int tcols2 = nt->type_cols > 0 ? nt->type_cols : nt->dur;
                int ts2 = nt->tie_start, tp2 = nt->tie_stop;
                if (tp2 && !ts2) tp2 = 0;
                write_note_series(wfp, nt, dtick2, tcols2,
                           (!nolyrics ? nt->lyric : NULL), ts2, tp2);
                continue;
            }
                if (off < cursor) {
                    /* 同列连音组的后续音:紧跟在组首音之后写出(须前音同为连音) */
                    if (nt->dur_tick > 0 && nt->col == last_col && last_tick > 0) {
                        int dtick2 = nt->dur_tick;
                        int tcols2 = nt->type_cols > 0 ? nt->type_cols : nt->dur;
                        write_note_series(wfp, nt, dtick2, tcols2,
                                   (!nolyrics ? nt->lyric : NULL), nt->tie_start, nt->tie_stop);
                    }
                    continue;
                }
                int dur = nt->dur;
                if (off + dur > measure_cols) dur = measure_cols - off;
                if (dur <= 0) continue;
                if (off > cursor) {
                    Note r2; memset(&r2, 0, sizeof(r2));
                    r2.is_rest = 1;
                    write_note_series(wfp, &r2, (off - cursor) * ticks_per_col, off - cursor, NULL, 0, 0);
                    wrote = 1;
                }
                int dtick = nt->dur_tick > 0 ? nt->dur_tick : dur * ticks_per_col;
                int tcols = nt->type_cols > 0 ? nt->type_cols : dur;
                write_note_series(wfp, nt, dtick, tcols,
                           (!nolyrics ? nt->lyric : NULL), nt->tie_start, nt->tie_stop);
                cursor = off + dur;
                last_off = off;
                last_col = nt->col;
                last_tick = nt->dur_tick;
                wrote = 1;
            }
            if (!wrote) {
                const char *tn; int dot;
                if (dur_exact(measure_cols, &tn, &dot)) {
                    /* 整小节休止:音型按小节列数(6/8 → 附点半音符,不是全音符) */
                    fprintf(wfp, "      <note>\n        <rest measure=\"yes\"/>\n        <duration>%d</duration>\n        <voice>1</voice>\n        <type>%s</type>\n",
                            measure_cols * ticks_per_col, tn);
                    if (dot) fprintf(wfp, "        <dot/>\n");
                    fprintf(wfp, "      </note>\n");
                } else {
                    Note r2; memset(&r2, 0, sizeof(r2));
                    r2.is_rest = 1;
                    write_note_series(wfp, &r2, measure_cols * ticks_per_col, measure_cols, NULL, 0, 0);
                }
            } else if (cursor < measure_cols) {
                Note r2; memset(&r2, 0, sizeof(r2));
                r2.is_rest = 1;
                write_note_series(wfp, &r2, (measure_cols - cursor) * ticks_per_col, measure_cols - cursor, NULL, 0, 0);
            }
            fprintf(wfp, "    </measure>\n");
        }
        fprintf(wfp, "  </part>\n");
    }

    fprintf(wfp, "</score-partwise>\n");
    if (w > 0) fclose(wfp);
    }
    for (int i = 0; i < nvrec; i++) free(obuf[i]);
    free(obuf); free(on); free(ocap);
    fclose(fp);

        for (int i = 0; i < nlines; i++) {
            free(rows[i].head); free(rows[i].music);
        }
        for (int i = 0; i < ctx.nvoices; i++) {
            free(ctx.voices[i].xml_id);
            free(ctx.voices[i].id);
            free(ctx.voices[i].name_en);
            free(ctx.voices[i].name_zh);
            free(ctx.voices[i].notes);
        }
        free(ctx.voices);
        free(ctx.gs.defs);
        free(ctx.gs.chords);
        free(linenos);
        free(lines);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "用法: tymp2musicxml <输入.tymp> [输出.musicxml] [-nolyrics]\n");
        return 1;
    }
    const char *src = argv[1];
    const char *dst = NULL;
    int nolyrics = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-nolyrics")) nolyrics = 1;
        else if (!strcmp(argv[i], "-v")) g_verbose = 1;
        else if (!dst) dst = argv[i];
    }
    char buf[1024];
    if (!dst) {
        strncpy(buf, src, sizeof(buf) - 1);
        char *dot = strrchr(buf, '.');
        if (dot) *dot = '\0';
        strncat(buf, ".musicxml", sizeof(buf) - strlen(buf) - 1);
        dst = buf;
    }
    return tymp2musicxml(src, dst, nolyrics);
}
