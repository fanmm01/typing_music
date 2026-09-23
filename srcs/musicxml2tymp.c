/* ============================================================================
 * musicxml2tymp.c —— MusicXML → .tymp(本仓库的文本乐谱格式)
 *
 * 目标(最小可用):保证**音符**与**时值**正确。和弦只保留最高音。
 * 歌词 / 力度 / 速度变化 / 特殊表记 一律不输出。
 *
 * 列模型(.tymp 的网格,与 tymp2musicXML.c 的 measure_cols 一致):
 *   1 列 = 1 个十六分音符;小节列数 = beats × 16 / beat_type。
 *   音符时值 = 该行内「本音符列 → 下一事件列」的列数差。
 *
 * 音区与变音修正优先写成**数字后缀**(' + - . 紧随数字,不另占列位——
 * 解析器把后缀计入音高且 anchor 随之更新),后缀放不下(列余量不足)时,
 * 余下的符号落到数字行上方的**机动行**(apply_aux_symbol 对同列音符
 * octave±1 / alter±1,不改 anchor)。每音最多 2 个机动符号(两行,每行同列
 * 至多 1 个),因此列位与时值严格对应。
 *
 * 用法:musicxml2tymp <输入.musicxml> [输出.tymp] [-p 1,2,...] [-v]
 *   -p  选择声部(按 <part-list> 顺序,1 基);缺省 = 全部
 * ==========================================================================*/
#include "tympImpl.h"
#include <ctype.h>
#include <stdarg.h>

/* ============================================================ 小工具 */

static char *read_whole(const char *path, size_t *out_n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *p = (char *)malloc((size_t)sz + 1);
    if (!p) { fclose(f); return NULL; }
    size_t rd = fread(p, 1, (size_t)sz, f);
    p[rd] = '\0';
    fclose(f);
    if (out_n) *out_n = rd;
    return p;
}

/* ============================================================ XML 扫描
 * MusicXML 结构规整,不需要通用解析器。全部按**字节**处理
 * (仓库里的 .musicxml 标题是 GBK 字节流,故不做编码假设)。
 */

/* p 指向 '<' 之后。取标签名(去掉 '/' 前缀),并判断是否自闭合;
 * 返回开标签 '>' 之后的位置。 */
static const char *read_tag(const char *p, char *name, int nsz, int *self) {
    const char *q = p;
    if (*q == '/') q++;
    int i = 0;
    while (q[i] && !isspace((unsigned char)q[i]) && q[i] != '>' && q[i] != '/' && i < nsz - 1) {
        name[i] = q[i]; i++;
    }
    name[i] = '\0';
    const char *gt = strchr(q, '>');
    if (self) *self = (gt && gt > q && gt[-1] == '/');
    return gt ? gt + 1 : q + strlen(q);
}

/* 找 from 之后第一个 <name …>…</name> 的内容区间;找不到返回 0 */
static int find_elem(const char *from, const char *lim, const char *name,
                     const char **bs, const char **be) {
    char open[64], close[64];
    snprintf(open, sizeof(open), "<%s", name);
    snprintf(close, sizeof(close), "</%s>", name);
    const char *p = from;
    while ((p = strstr(p, open)) != NULL && (!lim || p < lim)) {
        char c = p[strlen(open)];
        if (c == '>' || c == '/' || isspace((unsigned char)c)) {
            const char *gt = strchr(p, '>');
            if (!gt || (lim && gt > lim)) return 0;
            if (gt[-1] == '/') { *bs = gt + 1; *be = gt + 1; return 1; }
            const char *e = strstr(gt + 1, close);
            if (!e || (lim && e > lim)) return 0;
            *bs = gt + 1; *be = e;
            return 1;
        }
        p += strlen(open);
    }
    return 0;
}

static int elem_text(const char *blk, const char *lim, const char *name,
                     char *out, int osz) {
    const char *bs, *be;
    if (!find_elem(blk, lim, name, &bs, &be)) return 0;
    int n = (int)(be - bs);
    if (n > osz - 1) n = osz - 1;
    memcpy(out, bs, n); out[n] = '\0';
    return 1;
}

static int elem_int(const char *blk, const char *lim, const char *name, int *out) {
    char t[64];
    if (!elem_text(blk, lim, name, t, sizeof(t))) return 0;
    *out = atoi(t);
    return 1;
}

/* 遍历一层的直接子元素 */
typedef struct { const char *p, *lim; } Walk;

static int walk_next(Walk *w, char *name, int nsz, const char **bs, const char **be) {
    while (w->p < w->lim) {
        const char *lt = (const char *)memchr(w->p, '<', (size_t)(w->lim - w->p));
        if (!lt) return 0;
        if (lt[1] == '!') {                       /* 注释 / DOCTYPE */
            if (!strncmp(lt, "<!--", 4)) {
                const char *e = strstr(lt + 4, "-->");
                w->p = (e && e < w->lim) ? e + 3 : w->lim;
            } else {
                const char *e = (const char *)memchr(lt, '>', (size_t)(w->lim - lt));
                w->p = e ? e + 1 : w->lim;
            }
            continue;
        }
        if (lt[1] == '?') {
            const char *e = (const char *)memchr(lt, '>', (size_t)(w->lim - lt));
            w->p = e ? e + 1 : w->lim;
            continue;
        }
        if (lt[1] == '/') { w->p = lt; return 0; }    /* 本层结束 */

        int self = 0;
        const char *gt = read_tag(lt + 1, name, nsz, &self);
        if (self) { *bs = gt; *be = gt; w->p = gt; return 1; }

        char close[64];
        snprintf(close, sizeof(close), "</%s>", name);
        const char *e = strstr(gt, close);
        if (!e || e > w->lim) { *bs = gt; *be = w->lim; w->p = w->lim; return 1; }
        *bs = gt; *be = e;
        w->p = e + strlen(close);
        return 1;
    }
    return 0;
}

/* ============================================================ 数据模型 */

typedef struct {                 /* 一个发声事件(和弦已取最高音;tie 切分原样保留) */
    int  onset;                  /* 起点 tick(声部内绝对) */
    int  dur;                    /* 时值 tick */
    int  is_rest;
    char step;                   /* 'A'..'G' */
    int  alter, octave;
} RawEv;

typedef struct {
    char   id[48], name[128];
    int    beats, beat_type, fifths, is_minor;
    int    divisions;
    double tempo;
    RawEv *ev; int n, cap;
} MxPart;

static void part_push(MxPart *pt, const RawEv *e) {
    if (pt->n >= pt->cap) {
        pt->cap = pt->cap ? pt->cap * 2 : 256;
        pt->ev = (RawEv *)realloc(pt->ev, pt->cap * sizeof(RawEv));
        if (!pt->ev) { fprintf(stderr, "内存不足\n"); exit(1); }
    }
    pt->ev[pt->n++] = *e;
}


/* 音高比较用的 MIDI 值 */
static int ev_midi(const char *step, int alter, int octave) {
    int s;
    switch (toupper((unsigned char)*step)) {
        case 'C': s = 0;  break; case 'D': s = 2;  break; case 'E': s = 4;  break;
        case 'F': s = 5;  break; case 'G': s = 7;  break; case 'A': s = 9;  break;
        case 'B': s = 11; break; default: s = 0;   break;
    }
    return (octave + 1) * 12 + s + alter;
}

/* ============================================================ 解析 */

typedef struct {
    char   title[256], author[256];
    MxPart  *parts; int nparts, cap;
} MxScore;

static MxPart *score_part(MxScore *sc, const char *id) {
    for (int i = 0; i < sc->nparts; i++)
        if (!strcmp(sc->parts[i].id, id)) return &sc->parts[i];
    return NULL;
}

static MxPart *score_add(MxScore *sc, const char *id) {
    MxPart *p = score_part(sc, id);
    if (p) return p;
    if (sc->nparts >= sc->cap) {
        sc->cap = sc->cap ? sc->cap * 2 : 16;
        sc->parts = (MxPart *)realloc(sc->parts, sc->cap * sizeof(MxPart));
        if (!sc->parts) { fprintf(stderr, "内存不足\n"); exit(1); }
    }
    p = &sc->parts[sc->nparts++];
    memset(p, 0, sizeof(*p));
    snprintf(p->id, sizeof(p->id), "%s", id);
    p->beats = 4; p->beat_type = 4; p->divisions = 1; p->tempo = 120;
    return p;
}

static int pitch_of(const char *blk, const char *lim, char *step, int *alter, int *oct) {
    const char *bs, *be;
    char t[16];
    if (!find_elem(blk, lim, "pitch", &bs, &be)) return 0;
    if (!elem_text(bs, be, "step", t, sizeof(t))) return 0;
    *step = t[0];
    *alter = 0; elem_int(bs, be, "alter", alter);
    *oct = 4;   elem_int(bs, be, "octave", oct);
    return 1;
}

/* 读一个 <measure>:推进 cursor,产出事件 */
static void parse_measure(MxPart *pt, const char *body, const char *lim, int *cursor) {
    Walk w = { body, lim };
    char name[64];
    const char *bs, *be;

    int   pend = 0;              /* 当前和弦组:是否有待入列的「首音」 */
    RawEv p;                     /* 组内最高音 */
    int   gp_dur = 0;            /* 组内最大时值 */

    while (walk_next(&w, name, sizeof(name), &bs, &be)) {
        if (!strcmp(name, "attributes")) {
            elem_int(bs, be, "divisions", &pt->divisions);
            const char *tbs, *tbe;
            if (find_elem(bs, be, "time", &tbs, &tbe)) {
                int b = pt->beats, bt = pt->beat_type;
                if (elem_int(tbs, tbe, "beats", &b) && elem_int(tbs, tbe, "beat-type", &bt)) {
                    pt->beats = b; pt->beat_type = bt;
                }
            }
            const char *kbs, *kbe;
            if (find_elem(bs, be, "key", &kbs, &kbe)) {
                char m[16] = "";
                elem_int(kbs, kbe, "fifths", &pt->fifths);
                elem_text(kbs, kbe, "mode", m, sizeof(m));
                pt->is_minor = (m[0] && !strncmp(m, "minor", 5));
            }
        } else if (!strcmp(name, "note")) {
            int dur = 0;
            elem_int(bs, be, "duration", &dur);
            if (dur < 0) dur = 0;
            const char *rp = strstr(bs, "<rest");
            const char *cp = strstr(bs, "<chord/>");
            int is_rest  = (rp && rp < be);
            int is_chord = (cp && cp < be);
            char step = 'C'; int alter = 0, oct = 4;
            int has_pitch = pitch_of(bs, be, &step, &alter, &oct);

            if (is_rest) {
                if (pend) { p.dur = gp_dur; part_push(pt, &p); pend = 0; }
                RawEv e; memset(&e, 0, sizeof(e));
                e.onset = *cursor; e.dur = dur; e.is_rest = 1;
                part_push(pt, &e);
                *cursor += dur;
            } else if (is_chord && pend) {
                /* 和弦:只留最高音 */
                if (has_pitch &&
                    ev_midi(&step, alter, oct) > ev_midi(&p.step, p.alter, p.octave)) {
                    p.step = step; p.alter = alter; p.octave = oct;
                }
                if (dur > gp_dur) gp_dur = dur;
            } else {
                if (pend) { p.dur = gp_dur; part_push(pt, &p); pend = 0; }
                if (has_pitch) {
                    memset(&p, 0, sizeof(p));
                    p.onset = *cursor; p.dur = dur;
                    p.step = step; p.alter = alter; p.octave = oct;
                    gp_dur = dur; pend = 1;
                }
                *cursor += dur;
            }
        } else if (!strcmp(name, "backup")) {
            int d = 0; elem_int(bs, be, "duration", &d);
            *cursor -= d; if (*cursor < 0) *cursor = 0;
        } else if (!strcmp(name, "forward")) {
            int d = 0; elem_int(bs, be, "duration", &d);
            *cursor += d;
        } else if (!strcmp(name, "direction")) {
            const char *s = strstr(bs, "<sound");
            if (s && s < be) {
                const char *t = strstr(s, "tempo=\"");
                if (t && t < be) {
                    double v = atof(t + 7);
                    if (v > 0) pt->tempo = v;
                }
            }
        }
    }
    if (pend) { p.dur = gp_dur; part_push(pt, &p); }
}

static int parse_score(const char *xml, MxScore *sc) {
    const char *bs, *be;
    if (find_elem(xml, NULL, "work-title", &bs, &be)) {
        int n = (int)(be - bs); if (n > 255) n = 255;
        memcpy(sc->title, bs, n); sc->title[n] = '\0';
    }
    if (find_elem(xml, NULL, "creator", &bs, &be)) {
        int n = (int)(be - bs); if (n > 255) n = 255;
        memcpy(sc->author, bs, n); sc->author[n] = '\0';
    }
    if (find_elem(xml, NULL, "part-list", &bs, &be)) {
        /* 手工扫 <score-part id="…">:id 在开标签上,不能从元素内容里找 */
        const char *p = bs;
        while ((p = strstr(p, "<score-part")) != NULL && p < be) {
            char c = p[11];
            if (c != '>' && c != '/' && !isspace((unsigned char)c)) { p += 11; continue; }
            const char *idp = strstr(p, "id=\"");
            const char *gt = strchr(p, '>');
            if (!idp || !gt || gt > be) { p += 11; continue; }
            char id[48]; int k = 0;
            idp += 4;
            while (*idp && *idp != '"' && k < 47) id[k++] = *idp++;
            id[k] = '\0';
            MxPart *pt = score_add(sc, id);
            const char *pe = strstr(gt, "</score-part>");
            if (pe && pe < be && elem_text(gt + 1, pe, "part-name", pt->name, sizeof(pt->name)))
                strip_trailing_spaces(pt->name);
            p = gt + 1;
        }
    }
    /* <part id="…"> … </part> */
    const char *p = xml;
    while ((p = strstr(p, "<part ")) != NULL) {
        const char *idp = strstr(p, "id=\"");
        if (!idp) break;
        char id[48]; int k = 0;
        idp += 4;
        while (*idp && *idp != '"' && k < 47) id[k++] = *idp++;
        id[k] = '\0';
        const char *gt = strchr(p, '>');
        if (!gt) break;
        const char *pend = strstr(gt, "</part>");
        if (!pend) break;

        MxPart *pt = score_add(sc, id);
        int cursor = 0;
        Walk w = { gt + 1, pend };
        char name[64];
        const char *m1, *m2;
        while (walk_next(&w, name, sizeof(name), &m1, &m2)) {
            if (strcmp(name, "measure")) continue;
            parse_measure(pt, m1, m2, &cursor);
        }
        p = pend + 7;
    }
    return sc->nparts;
}

/* ============================================================ 调号 */

static int fifths_to_key(int fifths, char *root, int *alter) {
    static const struct { int f; char r; int a; } tab[] = {
        {  0,'C', 0}, {  1,'G', 0}, {  2,'D', 0}, {  3,'A', 0}, {  4,'E', 0},
        {  5,'B', 0}, {  6,'F', 1}, {  7,'C', 1},
        { -1,'F', 0}, { -2,'B',-1}, { -3,'E',-1}, { -4,'A',-1}, { -5,'D',-1},
        { -6,'G',-1}, { -7,'C',-1},
    };
    for (unsigned i = 0; i < sizeof(tab) / sizeof(tab[0]); i++)
        if (tab[i].f == fifths) { *root = tab[i].r; *alter = tab[i].a; return 1; }
    *root = 'C'; *alter = 0;
    return 0;
}

/* ============================================================ 一行(列网格) */

typedef struct {
    int   n;
    char *cells;              /* 数字行(数字 + 后缀) */
    char *aux[2];             /* 机动行(八度/变音符号,每行每列至多 1 个) */
    int   has_aux[2];
    int   first_oct;
} MxRow;

static void row_init(MxRow *r, int n) {
    r->n = n;
    r->cells = (char *)malloc((size_t)n + 1);
    for (int i = 0; i < 2; i++) r->aux[i] = (char *)malloc((size_t)n + 1);
    if (!r->cells || !r->aux[0] || !r->aux[1]) { fprintf(stderr, "内存不足\n"); exit(1); }
    memset(r->cells, ' ', (size_t)n);
    for (int i = 0; i < 2; i++) {
        memset(r->aux[i], ' ', (size_t)n);
        r->aux[i][n] = '\0';
        r->has_aux[i] = 0;
    }
    r->cells[n] = '\0';
    r->first_oct = 4;
}

static void row_free(MxRow *r) { free(r->cells); free(r->aux[0]); free(r->aux[1]); }

static int col_of(int tick, int divisions) {          /* tick → 列(1/16 音符) */
    if (divisions <= 0) divisions = 1;
    return (int)((long long)tick * 4 / divisions);
}

static int part_to_row(const MxPart *pt, const GroupState *gs, MxRow *r, int verbose, int part_no) {
    if (pt->n <= 0) return 0;
    int dv = pt->divisions > 0 ? pt->divisions : 1;

    int last_col = 0;
    for (int i = 0; i < pt->n; i++) {
        int c = col_of(pt->ev[i].onset + pt->ev[i].dur, dv);
        if (c > last_col) last_col = c;
    }
    int mcols = 16 * pt->beats / (pt->beat_type > 0 ? pt->beat_type : 4);
    if (mcols <= 0) mcols = 16;
    int ncol = last_col;
    ncol = ((ncol + mcols - 1) / mcols) * mcols;      /* 补齐到整小节;行尾事件
                                                         由转换器延续到小节末 */
    if (ncol < mcols) ncol = mcols;
    row_init(r, ncol);

    int  has_anchor = 0, anchor_oct = 4;
    char anchor_step = 'C';
    int  warned = 0;

    for (int i = 0; i < pt->n; i++) {
        const RawEv *e = &pt->ev[i];
        if ((long long)e->onset * 4 % dv != 0) {
            if (!warned) {
                fprintf(stderr, "  ! 声部%d 有事件不在 1/16 网格上(已向后取整)\n", part_no);
                warned = 1;
            }
        }
        int col = col_of(e->onset, dv);
        if (col < 0) col = 0;
        if (col >= r->n) continue;

        if (e->is_rest) { r->cells[col] = '0'; continue; }

        /* 音级:在该调内找到与目标音名同字母的级数(8 音不用,以 1 级 + 后缀
         * 表达,效果与解析器的 og8 完全相同) */
        char st = 'C'; int alo = 0, og8 = 0;
        int deg = 1, base_alter = 0;
        for (int d = 1; d <= 7; d++) {
            degree_to_step_oct(gs, d, 0, &st, &alo, &og8);
            if (toupper((unsigned char)st) == toupper((unsigned char)e->step)) {
                deg = d; base_alter = alo; break;
            }
        }

        /* 与解析器同序复算八度(后缀 sh 计入 anchor;机动符号不入 anchor) */
        int oct_raw = anchor_oct;
        if (has_anchor) resolve_octave(anchor_step, anchor_oct, st, &oct_raw, 0);

        int k8 = 0;                     /* 需要的八度总修正(后缀 + 机动) */
        if (!has_anchor) {
            /* 首音:行头 >n 给出实际八度;解析器首音忽略后缀,故首音无八度符号 */
            r->first_oct = (e->octave < 0) ? 0 : (e->octave > 9 ? 9 : e->octave);
            oct_raw = r->first_oct;
        } else {
            k8 = e->octave - oct_raw;
        }

        /* 需要的变音修正(后缀 + 机动);每音后缀链独立,al 从 0 起算 */
        int dal = e->alter - base_alter;

        /* 符号序列:先八度(影响 anchor 的放前面),后变音 */
        char seq[8]; int nseq = 0;
        {
            char sym = (k8 > 0) ? '\'' : '.';
            for (int t = 0; t < (k8 > 0 ? k8 : -k8) && nseq < 6; t++) seq[nseq++] = sym;
        }
        {
            char sym = (dal > 0) ? '+' : '-';
            for (int t = 0; t < (dal > 0 ? dal : -dal) && nseq < 6; t++) seq[nseq++] = sym;
        }

        /* 先填数字后缀(占列 = 个数,须 ≤ dur-1),余下逐行放机动行 */
        r->cells[col] = (char)('0' + deg);
        int avail = e->dur - 1; if (avail < 0) avail = 0;
        int nsfx = nseq < avail ? nseq : avail;         /* 进后缀的符号数 */
        int naux = nseq - nsfx;                         /* 进机动行的符号数 */
        if (naux > 2) {
            if (!warned) {
                fprintf(stderr, "  ! 声部%d 有音符修正超出表达容量(八度/变音将失真)\n", part_no);
                warned = 1;
            }
            naux = 2;
        }
        int sh = 0;                                     /* 后缀净八度 */
        for (int t = 0; t < nsfx; t++) {
            if (col + 1 + t < r->n) r->cells[col + 1 + t] = seq[t];
            if (seq[t] == '\'') sh++;
            else if (seq[t] == '.') sh--;
        }
        for (int t = 0; t < naux; t++) {
            r->aux[t][col] = seq[nsfx + t];
            r->has_aux[t] = 1;
        }

        anchor_step = st; anchor_oct = oct_raw + sh; has_anchor = 1;
        if (verbose && i < 8)
            fprintf(stderr, "    声部%d ev%d 列=%d 级=%d 音=%c%+d%d k8=%+d dal=%+d 后缀=%d 机动=%d\n",
                    part_no, i, col, deg, e->step, e->alter, e->octave, k8, dal, nsfx, naux);
    }

    return 1;
}

/* ============================================================ 输出 */

static void emit_line(FILE *fp, const char *meta, int mw, const char *zone) {
    int ml = (int)strlen(meta);
    for (int i = ml; i < mw; i++) fputc(' ', fp);
    fputs(meta, fp);
    fputs("||", fp);
    if (zone) {
        int n = (int)strlen(zone);
        while (n > 0 && zone[n - 1] == ' ') n--;      /* 去掉行尾空格 */
        fwrite(zone, 1, (size_t)n, fp);
    }
    fputc('\n', fp);
}

static void usage(const char *a0) {
    fprintf(stderr,
        "用法: %s <输入.musicxml> [输出.tymp] [-p 1,2,...] [-v]\n"
        "  -p  选择声部(按 <part-list> 顺序,1 基;可重复);缺省 = 全部声部\n"
        "  -v  打印逐音解析信息\n", a0);
}

int main(int argc, char **argv) {
    const char *in = NULL, *out = NULL;
    int sel[512] = {0}, nsel = 0, verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            const char *s = argv[++i];
            while (*s) {
                int v = atoi(s);
                if (v >= 1 && v < 512) sel[v] = 1;
                while (*s && *s != ',') s++;
                if (*s == ',') s++;
            }
        } else if (!in) in = argv[i];
        else if (!out) out = argv[i];
        else { usage(argv[0]); return 1; }
    }
    if (!in) { usage(argv[0]); return 1; }
    for (int i = 1; i < 512; i++) if (sel[i]) nsel++;

    size_t xn = 0;
    char *xml = read_whole(in, &xn);
    if (!xml) { fprintf(stderr, "无法读取: %s\n", in); return 1; }

    MxScore sc; memset(&sc, 0, sizeof(sc));
    if (parse_score(xml, &sc) <= 0) { fprintf(stderr, "未找到 <part>\n"); return 1; }

    int *picked = (int *)calloc((size_t)sc.nparts + 1, sizeof(int));
    int npick = 0;
    for (int i = 0; i < sc.nparts; i++) {
        int no = i + 1;
        int want = (nsel == 0) ? 1 : (no < 512 && sel[no]);
        if (!want) continue;
        if (sc.parts[i].n <= 0) {
            fprintf(stderr, "声部%d(%s):无音符,跳过\n", no, sc.parts[i].id);
            continue;
        }
        picked[npick++] = i;
    }
    if (npick == 0) { fprintf(stderr, "没有可转换的声部\n"); return 1; }

    const MxPart *p0 = &sc.parts[picked[0]];

    /* 组状态:用解析器同一份 parse_keyspec / parse_timesig 建立 */
    GroupState gs; memset(&gs, 0, sizeof(gs));
    char keytok[32];
    {
        char root; int alt;
        int fifth = p0->fifths;
        if (p0->is_minor) fifth += 3;                 /* parse_keyspec 对 minor 会 -3 */
        if (!fifths_to_key(fifth, &root, &alt)) {
            fprintf(stderr, "  ! 调号 fifths=%d 超出常用范围,按 C 处理\n", p0->fifths);
            root = 'C'; alt = 0;
        }
        int n = snprintf(keytok, sizeof(keytok), "|=");
        if (alt > 0) n += snprintf(keytok + n, sizeof(keytok) - n, "#");
        else if (alt < 0) n += snprintf(keytok + n, sizeof(keytok) - n, "b");
        n += snprintf(keytok + n, sizeof(keytok) - n, "%c", root);
        if (p0->is_minor) snprintf(keytok + n, sizeof(keytok) - n, "m");
        parse_keyspec(keytok, &gs);
    }
    char timetok[32];
    snprintf(timetok, sizeof(timetok), "%d/%d", p0->beats, p0->beat_type);
    parse_timesig(timetok, &gs);

    MxRow *rows = (MxRow *)calloc((size_t)npick, sizeof(MxRow));
    for (int i = 0; i < npick; i++) {
        int no = picked[i] + 1;
        if (verbose)
            fprintf(stderr, "声部%d(%s) 名字=%s 事件=%d divisions=%d %d/%d\n",
                    no, sc.parts[picked[i]].id, sc.parts[picked[i]].name,
                    sc.parts[picked[i]].n, sc.parts[picked[i]].divisions,
                    sc.parts[picked[i]].beats, sc.parts[picked[i]].beat_type);
        part_to_row(&sc.parts[picked[i]], &gs, &rows[i], verbose, no);
    }

    /* 元数据区宽度 */
    char meta0[192];
    snprintf(meta0, sizeof(meta0), "/*LV.pitch range of 1st note:*/ >%d", rows[0].first_oct);
    int mw = (int)strlen(meta0);
    for (int i = 1; i < npick; i++) {
        char t[192];
        snprintf(t, sizeof(t), "[/*Harmony*/ >%d", rows[i].first_oct);
        if ((int)strlen(t) > mw) mw = (int)strlen(t);
    }
    if ((int)strlen("{ |00/00 |=C |vb000.000 }") > mw) mw = (int)strlen("{ |00/00 |=C |vb000.000 }");
    if (mw < 56) mw = 56;
    mw = ((mw + 3) / 4) * 4;

    FILE *fp = out ? fopen(out, "wb") : stdout;
    if (!fp) { fprintf(stderr, "无法写出: %s\n", out); return 1; }

    fprintf(fp, "%s by %s\n", sc.title[0] ? sc.title : "song",
            sc.author[0] ? sc.author : "author");
    fputs("instruments:", fp);
    for (int i = 0; i < npick; i++) {
        const char *nm = sc.parts[picked[i]].name;
        fprintf(fp, " %s", nm[0] ? nm : sc.parts[picked[i]].id);
    }
    fputc('\n', fp);

    /* 刻度尺(解析器跳过 #timestamp 行,仅供人眼对齐) */
    {
        int accu = (p0->beat_type > 0 && 16 % p0->beat_type == 0) ? 16 / p0->beat_type : 4;
        if (accu < 1) accu = 1;
        int mcols = accu * p0->beats;
        int width = rows[0].n + accu;
        char *zone = (char *)malloc((size_t)width + 1);
        memset(zone, ' ', (size_t)width);
        for (int c = 0; c < width; c += accu) zone[c] = (c % mcols == 0) ? '|' : '.';
        zone[width] = '\0';
        emit_line(fp, "#timestamp:", mw, zone);
        free(zone);
    }

    char ghead[192];
    snprintf(ghead, sizeof(ghead), "{ |%s %s |vb%.3f }", timetok, keytok, p0->tempo);
    emit_line(fp, ghead, mw, "");

    for (int i = 0; i < npick; i++) {
        char meta[192];
        if (i == 0) snprintf(meta, sizeof(meta), "/*LV.pitch range of 1st note:*/ >%d", rows[i].first_oct);
        else        snprintf(meta, sizeof(meta), "[/*Harmony*/ >%d", rows[i].first_oct);
        if (rows[i].has_aux[0]) emit_line(fp, "/*aux*/", mw, rows[i].aux[0]);
        if (rows[i].has_aux[1]) emit_line(fp, "/*aux*/", mw, rows[i].aux[1]);
        emit_line(fp, meta, mw, rows[i].cells);
    }
    emit_line(fp, "}", mw, NULL);
    if (out) fclose(fp);

    if (verbose) fprintf(stderr, "已写出 %d 个声部 → %s\n", npick, out ? out : "(stdout)");

    for (int i = 0; i < npick; i++) row_free(&rows[i]);
    free(rows); free(picked); free(xml);
    for (int i = 0; i < sc.nparts; i++) free(sc.parts[i].ev);
    free(sc.parts);
    return 0;
}
