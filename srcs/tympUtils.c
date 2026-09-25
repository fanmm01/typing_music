/* ============================================================================
 * tympUtils.c —— 工具层(纯函数)
 * 依据 tymp2musicxml_实现方案.md §6.1:字符串 / 乐理 / 调号 / 时值 / 标记定义
 * ==========================================================================*/
#include "tympImpl.h"
#include <ctype.h>

/* ---------------------------------------------------------------- 字符串 */

char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

void strip_crlf(char *line) {
    if (!line) return;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
}

/* 去掉行尾空格;.tymp 每行有约 10000 个填充空格 */
void strip_trailing_spaces(char *line) {
    if (!line) return;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t')) line[--n] = '\0';
}

int utf8_len_of(const char *s) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

int col_is_cjk(const char *s) {
    int l = utf8_len_of(s);
    if (l < 3) return 0;
    unsigned cp = decode_utf8((const unsigned char *)s, l);
    return cp_is_cjk(cp) ? 1 : 0;
}

/* 整文件读入为行数组;同时返回每行的文档行号 */
char **read_all_lines(const char *path, int *n, int **linenos) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    int cap = 256, cnt = 0;
    char **lines = (char **)malloc(cap * sizeof(char *));
    int  *nums  = (int *)malloc(cap * sizeof(int));
    if (!lines || !nums) { fclose(fp); free(lines); free(nums); return NULL; }

    int bufsz = 1 << 16;
    char *buf = (char *)malloc(bufsz);
    if (!buf) { fclose(fp); free(lines); free(nums); return NULL; }

    char chunk[8192];
    size_t used = 0;
    while (!feof(fp)) {
        size_t got = fread(chunk, 1, sizeof(chunk), fp);
        if (got == 0) break;
        if (used + got + 1 > (size_t)bufsz) {
            while (used + got + 1 > (size_t)bufsz) bufsz *= 2;
            buf = (char *)realloc(buf, bufsz);
            if (!buf) { fclose(fp); free(lines); free(nums); return NULL; }
        }
        memcpy(buf + used, chunk, got);
        used += got;
    }
    buf[used] = '\0';
    fclose(fp);

    /* 按 \n 切分(保留内容,去掉行尾 \r) */
    size_t start = 0;
    for (size_t i = 0; i <= used; i++) {
        if (i == used || buf[i] == '\n') {
            size_t len = i - start;
            if (i < used) { /* 正常的换行结束 */ }
            char *ln = (char *)malloc(len + 1);
            if (!ln) break;
            memcpy(ln, buf + start, len);
            ln[len] = '\0';
            strip_crlf(ln);
            if (cnt >= cap) {
                cap *= 2;
                lines = (char **)realloc(lines, cap * sizeof(char *));
                nums  = (int *)realloc(nums, cap * sizeof(int));
            }
            lines[cnt] = ln;
            nums[cnt] = cnt + 1;
            cnt++;
            start = i + 1;
        }
    }
    free(buf);
    if (n) *n = cnt;
    if (linenos) *linenos = nums; else free(nums);
    return lines;
}

/* 找到第一个 "||",返回其**之后**的列号(0 基);无则返回 -1 */
int find_metadata_end(const char *line) {
    if (!line) return -1;
    for (int i = 0; line[i] && line[i + 1]; i++) {
        if (line[i] == '|' && line[i + 1] == '|') return i + 2;
    }

    /* 歌词行在实践文件中常省略 ||，例如歌词控制前缀后直接跟正文。
     * 从 *l 开始的部分仍须进入 music，不能被整行丢弃。 */
    const char *ly = strstr(line, "*l");
    if (ly) return (int)(ly - line);

    /* 兼容没有 || 的旧式注释歌词行：注释后的正文是歌词。 */
    const char *comment_end = strstr(line, "*/");
    if (comment_end) {
        const char *p = comment_end + 2;
        while (*p == ' ' || *p == '\t') p++;
        if (*p) return (int)(p - line);
    }
    return -1;
}

/* ---------------------------------------------------------------- 乐理 */

int key_to_fifths(char root, int alter) {
    int base;
    switch (toupper((unsigned char)root)) {
        case 'C': base = 0;  break;
        case 'D': base = 2;  break;
        case 'E': base = 4;  break;
        case 'F': base = -1; break;
        case 'G': base = 1;  break;
        case 'A': base = 3;  break;
        case 'B': base = 5;  break;
        default:  base = 0;  break;
    }
    return base + 7 * alter;
}

/* 大调音阶半音步进(0 基:1=0, 2=2, 3=4, 4=5, 5=7, 6=9, 7=11, 8=12) */
static const int kMajorSteps[9] = { 0, 0, 2, 4, 5, 7, 9, 11, 12 };
/* 自然小调 */
static const int kMinorSteps[9] = { 0, 0, 2, 3, 5, 7, 8, 10, 12 };
static const char kStepNames[7] = { 'C', 'D', 'E', 'F', 'G', 'A', 'B' };
static const int  kStepSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };

/* 音级 + 变音 → 音名与变音(相对调根音)
 * 简谱音级按其「级数」取对应音名字母:1 = 根音字母, 2 = 根音上方一个字母,
 * 依此类推(与调式音阶的半音数共同决定需补的升降号)。 */
void degree_to_step_oct(const GroupState *g, int degree, int alter,
                        char *step, int *alter_out, int *oct_out) {
    if (degree < 1) degree = 1;
    int d = degree;
    int oct_shift = 0;
    while (d > 7) { d -= 7; oct_shift++; }          /* 8 → 1 + 一个八度 */
    if (oct_out) *oct_out = oct_shift;

    int root_idx = 0;
    char r = g->key_root ? g->key_root : 'C';
    for (int i = 0; i < 7; i++) if (kStepNames[i] == toupper((unsigned char)r)) root_idx = i;

    int idx = (root_idx + d - 1) % 7;               /* 该音级对应的字母 */
    int natural = kStepSemis[idx];                  /* 该字母的自然半音 */
    int scale = (g->is_minor ? kMinorSteps[d] : kMajorSteps[d]);
    int target = kStepSemis[root_idx] + scale + g->key_alter + alter;

    int al = target - natural;
    while (al > 6) al -= 12;
    while (al < -6) al += 12;
    *step = kStepNames[idx];
    *alter_out = al;
}

/* 兼容旧签名 */
void degree_to_step(const GroupState *g, int degree, int alter,
                    char *step, int *alter_out) {
    degree_to_step_oct(g, degree, alter, step, alter_out, NULL);
}

/* (step,alter) 是否属于当前调号的自然音 */
int step_is_diatonic(int fifths, char step, int alter) {
    static const char sharp_order[7] = { 'F', 'C', 'G', 'D', 'A', 'E', 'B' };
    static const char flat_order[7]  = { 'B', 'E', 'A', 'D', 'G', 'C', 'F' };
    int base = 0;
    if (fifths > 0) {
        for (int i = 0; i < 7 && i < fifths; i++) if (toupper((unsigned char)step) == sharp_order[i]) base = 1;
    } else if (fifths < 0) {
        for (int i = 0; i < 7 && i < -fifths; i++) if (toupper((unsigned char)step) == flat_order[i]) base = -1;
    }
    return (base == alter);
}

/* 八度消解(见 resolve_octave_bias):无标记时四度/五度不改变音组;
 * bias 为方向偏好——在该方向上取离锚点最近的八度音。 */
int resolve_octave_bias(char prev_step, int prev_oct, char step, int *oct, int bias) {
    static const int semis[7] = { 0, 2, 4, 5, 7, 9, 11 };
    int ps = -1, cs = -1;
    for (int i = 0; i < 7; i++) {
        if (kStepNames[i] == toupper((unsigned char)prev_step)) ps = semis[i];
        if (kStepNames[i] == toupper((unsigned char)step))      cs = semis[i];
    }
    if (ps < 0 || cs < 0) { *oct = prev_oct; return 0; }

    /* 无标记:四度与五度(含)以内**不改变音组**;更大音程取其较近的八度。
     * bias = 方向偏好(标记):只在该方向上找「离锚点最近」的那个八度音,
     * 不无条件平移八度(1 5' 不动、4 3' 上移、4 3. 不动)。 */
    int prev_midi = (prev_oct + 1) * 12 + ps;
    int best = prev_midi, bestd = 1 << 30;
    for (int o = prev_oct - 2; o <= prev_oct + 2; o++) {
        int m = (o + 1) * 12 + cs;
        int d = m - prev_midi;
        if (bias > 0 && d < 0) continue;      /* 只要上方 */
        if (bias < 0 && d > 0) continue;      /* 只要下方 */
        int ad = d < 0 ? -d : d;
        if (ad < bestd || (ad == bestd && d > 0)) { bestd = ad; best = m; }
    }
    int o = best / 12 - 1;
    if (o < 0) o = 0;
    if (o > 9) o = 9;
    *oct = o;
    return 0;
}

/* 兼容旧调用:shift 为「临时八度升降」(定义内容内的格内标记) */
int resolve_octave(char prev_step, int prev_oct, char step, int *oct, int shift) {
    resolve_octave_bias(prev_step, prev_oct, step, oct, 0);
    *oct += shift;
    if (*oct < 0) *oct = 0;
    if (*oct > 9) *oct = 9;
    return 0;
}

/* ---------------------------------------------------------------- 组头元数据 */

void parse_timesig(const char *tok, GroupState *g) {
    int n = 0, d = 0;
    if (sscanf(tok, "%d/%d", &n, &d) == 2 && n > 0 && d > 0) {
        g->beats = n; g->beat_type = d;
        g->accu = (d > 0 && 16 % d == 0) ? 16 / d : 4;
        if (g->accu < 1) g->accu = 1;
    }
}

void parse_keyspec(const char *tok, GroupState *g) {
    /* |=C / |=#G / |=Dm / |=#G~ ; |-7-3-6(旧) ; |@[7-3-6-] / |@[0] */
    while (*tok == '|' || *tok == '\\') tok++;
    if (*tok == '@') { parse_fixed_acc(tok, g); return; }
    if (*tok == '=') tok++;
    if (*tok == '-') { /* 旧式 |-7-3-6 */
        for (int i = 0; i < 8; i++) g->fixed_alter[i] = 0;
        const char *p = tok;
        while (*p) {
            if (*p == '-') { int d = atoi(p + 1); if (d >= 1 && d <= 7) g->fixed_alter[d] = -1; }
            p++;
        }
        return;
    }
    int alter = 0;
    if (*tok == '#') { alter = 1; tok++; }
    else if (*tok == 'b') { alter = -1; tok++; }
    g->key_root = *tok ? *tok : 'C';
    g->key_alter = alter;
    tok++;
    if (*tok == 'm') g->is_minor = 1; else g->is_minor = 0;
    if (*tok == '~') g->chord_follow = 0; else g->chord_follow = 1;
    g->fifths = key_to_fifths(g->key_root, g->key_alter);
    if (g->is_minor) g->fifths -= 3;   /* 关系小调 */
    for (int i = 0; i < 8; i++) g->fixed_alter[i] = 0;
}

void parse_tempo_mark(const char *tok, GroupState *g) {
    while (*tok == '|' || *tok == '\\') tok++;
    if (tok[0] == 'v' && tok[1] == 'b') {
        double t = atof(tok + 2);
        if (t > 0) { g->tempo = t; g->tempo_valid = 1; }
    }
}

/* |sN / |ppp … → 力度级别(1..10);0 = 非力度记号 */
int parse_dynamic_level(const char *tok) {
    while (*tok == '|' || *tok == '\\') tok++;
    if (*tok == 's' && isdigit((unsigned char)tok[1])) {
        int n = atoi(tok + 1);
        if (n >= 1 && n <= 10) return n;
    }
    static const char *names[6] = { "ppppp", "pppp", "ppp", "pp", "p", "f" };
    (void)names;
    int p = 0; while (tok[p] == 'p') p++;
    if (p >= 1 && p <= 5 && tok[p] == '\0') return 5 - p;   /* pppp→1 … p→4 */
    int f = 0; while (tok[f] == 'f') f++;
    if (f >= 1 && f <= 5 && tok[f] == '\0') return 6 + (f - 1); /* f→6, ff→7 */
    return 0;
}

void parse_fixed_acc(const char *tok, GroupState *g) {
    while (*tok == '|' || *tok == '\\') tok++;
    if (*tok != '@') return;
    tok++;
    if (*tok != '[') return;
    tok++;
    if (*tok == '0') { for (int i = 0; i < 8; i++) g->fixed_alter[i] = 0; return; }
    for (int i = 0; i < 8; i++) g->fixed_alter[i] = 0;
    int cur = 0;
    while (*tok && *tok != ']') {
        if (isdigit((unsigned char)*tok)) { cur = atoi(tok); }
        else if (*tok == '+') { if (cur >= 1 && cur <= 7) g->fixed_alter[cur] = 1; }
        else if (*tok == '-') { if (cur >= 1 && cur <= 7) g->fixed_alter[cur] = -1; }
        else if (*tok == '~') { if (cur >= 1 && cur <= 7) g->fixed_alter[cur] = 0; }
        tok++;
    }
}

void parse_tuning(const char *tok, GroupState *g) {
    /* &gt: {a=E2,b=A2,c=D3,d=G3,e=B3,f=E3} */
    const char *p = strchr(tok, '{');
    if (!p) return;
    p++;
    for (int i = 0; i < 6; i++) {
        while (*p && (*p == ' ' || *p == ',')) p++;
        if (*p && (*p >= 'a' && *p <= 'f')) p++;     /* 弦名 */
        while (*p && *p == '=') p++;
        if (!*p) return;
        char step = *p++;
        int alter = 0, oct = 0;
        while (*p && (*p == '#' || *p == 'b')) { if (*p == '#') alter++; else alter--; p++; }
        if (isdigit((unsigned char)*p)) oct = atoi(p);
        g->tuning[i][0] = (char)toupper((unsigned char)step);
        g->tuning[i][1] = (char)('0' + (oct > 9 ? 9 : oct));
        g->tuning[i][2] = (char)('0' + (alter + 2));
        g->tuning[i][3] = '\0';
    }
    g->tuning_valid = 1;
}

/* ---------------------------------------------------------------- 吉他把位表 */

/* 内建常用开放和弦把位(1=低弦;x=不弹);被 ④类/{D/…} 定义覆盖 */
static const char *kDefaultVoicings[][2] = {
    {"Am7", "x02010"}, {"Am", "x02210"}, {"A", "x02220"}, {"A7", "x02020"},
    {"Bm", "x24432"},  {"C", "x32010"},  {"C7", "x32310"}, {"D", "xx0232"},
    {"D7", "xx0212"},  {"Dm", "xx0231"}, {"Dm7", "xx0211"}, {"E", "022100"},
    {"E7", "020100"},  {"Em", "022000"}, {"Em7", "020000"}, {"F", "133211"},
    {"G", "320003"},   {"G7", "320001"},
    {NULL, NULL}
};

void add_voicing(GroupState *gs, const char *name, const char *frets) {
    if (!gs || !name || !frets || gs->nvoic >= 32) return;
    for (int i = 0; i < gs->nvoic; i++) {
        if (!strcmp(gs->voic_name[i], name)) {          /* 后定义覆盖先定义 */
            strncpy(gs->voic_fret[i], frets, 7);
            gs->voic_fret[i][7] = '\0';
            return;
        }
    }
    strncpy(gs->voic_name[gs->nvoic], name, 15);
    gs->voic_name[gs->nvoic][15] = '\0';
    strncpy(gs->voic_fret[gs->nvoic], frets, 7);
    gs->voic_fret[gs->nvoic][7] = '\0';
    gs->nvoic++;
}

/* 构造和弦查询名:根音大写 + 品质(Am / C7 / Emaj7 / Ddim …) */
static void chord_voicing_name(const Chord *c, char *out, int sz) {
    int n = 0;
    out[n++] = (char)toupper((unsigned char)c->root);
    if (c->is_minor) { if (n < sz - 1) out[n++] = 'm'; }
    else if (c->is_dim) { if (n < sz - 3) { out[n++] = 'd'; out[n++] = 'i'; out[n++] = 'm'; } }
    else if (c->is_maj7) { if (n < sz - 4) { out[n++] = 'm'; out[n++] = 'a'; out[n++] = 'j'; out[n++] = '7'; } }
    else if (c->has_seventh) { if (n < sz - 1) out[n++] = '7'; }
    out[n] = '\0';
}

const char *voicing_lookup(const GroupState *gs, const Chord *c) {
    if (!gs || !c) return NULL;
    char name[16];
    chord_voicing_name(c, name, sizeof(name));
    for (int i = gs->nvoic - 1; i >= 0; i--)            /* 后定义优先 */
        if (!strcmp(gs->voic_name[i], name)) return gs->voic_fret[i];
    for (int i = 0; kDefaultVoicings[i][0]; i++)
        if (!strcmp(kDefaultVoicings[i][0], name)) return kDefaultVoicings[i][1];
    return NULL;
}

/* ---------------------------------------------------------------- 标记定义 */

int is_mark_letter(char c) {
    /* 标记名:字母,且不含 A~G */
    if (!isalpha((unsigned char)c)) return 0;
    char u = (char)toupper((unsigned char)c);
    if (u >= 'A' && u <= 'G') return 0;
    return 1;
}

void add_mark_def(GroupState *gs, const MarkDef *d) {
    if (gs->ndefs >= gs->defcap) {
        gs->defcap = gs->defcap ? gs->defcap * 2 : 16;
        gs->defs = (MarkDef *)realloc(gs->defs, gs->defcap * sizeof(MarkDef));
    }
    gs->defs[gs->ndefs++] = *d;
}

/* 查找标记定义。want_param:调用处是否带实参 (…)。
 * 同名可同时存在「非参数化」与「参数化」两份定义,按调用形态择一。 */
int find_mark_def(const GroupState *gs, const char *name, int namelen, int want_param) {
    if (namelen <= 0) return -1;
    int fallback = -1;
    for (int i = gs->ndefs - 1; i >= 0; i--) {   /* 后定义优先 */
        if ((int)strlen(gs->defs[i].name) != namelen ||
            strncmp(gs->defs[i].name, name, namelen)) continue;
        int has_param = (gs->defs[i].param != 0);
        if (has_param == (want_param ? 1 : 0)) return i;
        if (fallback < 0) fallback = i;
    }
    return fallback;
}

/* 统计 _ 分隔的单位数 */
static int count_units(const char *content, char sep) {
    int n = 1;
    for (const char *p = content; *p; p++) if (*p == sep) n++;
    return n;
}

/* 解析一个标记定义;成功返回 1,end_col 给出结束列 */
int parse_mark_def(const char *s, int *end_col, MarkDef *out) {
    int i = 0;
    memset(out, 0, sizeof(*out));

    /* 名:1~4 个字母(不含 A~G) */
    int nl = 0;
    while (nl < 4 && is_mark_letter(s[i])) { out->name[nl++] = s[i++]; }
    if (nl == 0) return 0;
    out->name[nl] = '\0';

    /* 可选参数 (X) */
    if (s[i] == '(') {
        int j = i + 1;
        if (isalpha((unsigned char)s[j]) && s[j + 1] == ')') {
            out->param = s[j];
            i = j + 2;
        }
    }

    if (s[i] == '~') {                      /* ① 瞬时 */
        out->kind = 1; i++;
        if (s[i] != '{') return 0;
        int j = i + 1;
        while (s[j] && s[j] != '}') j++;
        out->content = (char *)malloc(j - i);
        memcpy(out->content, s + i + 1, j - i - 1);
        out->content[j - i - 1] = '\0';
        i = j + 1;
    } else if (s[i] == '&') {               /* ② ③ ⑤ ⑥ */
        i++;
        if (s[i] == 'r') { out->chord_relative = 1; i++; }
        else if (s[i] == 'g') { out->voicing_g = 1; i++; }
        if (s[i] == '"') {
            int j = i + 1;
            while (s[j] && s[j] != '"') j++;
            int closed = s[j] == '"';
            if (!closed) {
                j = i + 1;
                while (s[j] && s[j] != ' ') j++;
            }
            out->content = (char *)malloc(j - i);
            memcpy(out->content, s + i + 1, j - i - 1);
            out->content[j - i - 1] = '\0';
            i = closed ? j + 1 : j;
        } else if (s[i] == '{' || s[i] == '[') {
            int j = i;
            while (s[j] && s[j] != ' ') j++;
            out->content = (char *)malloc(j - i + 1);
            memcpy(out->content, s + i, j - i);
            out->content[j - i] = '\0';
            i = j;
        } else {
            free(out->content); out->content = NULL; return 0;
        }
        out->kind = out->voicing_g ? 6 : (out->chord_relative ? 5 : 2);
    } else {
        return 0;
    }

    out->n_units = count_units(out->content, '_');
    if (end_col) *end_col = i;
    return 1;
}

/* ---------------------------------------------------------------- 和弦 */

void parse_chord_token(const char *s, int col, Chord *out) {
    memset(out, 0, sizeof(*out));
    out->col = col;
    if (*s == '-') { out->is_none = 1; return; }
    if (*s == '#') { out->root_alter = 1; s++; }
    else if (*s == 'b') { out->root_alter = -1; s++; }
    out->root = *s ? *s : 'C';
    s++;
    int qi = 0;
    while (*s && *s != '_' && qi < 15)
        out->quality[qi++] = *s++;
    out->quality[qi] = '\0';
    if (strstr(out->quality, "maj7")) { out->is_maj7 = 1; out->has_seventh = 1; }
    else if (strstr(out->quality, "7")) { out->has_seventh = 1; }
    /* min 是 dim 的别名(作者确认);m 才是小三和弦 */
    if (strstr(out->quality, "dim") || strstr(out->quality, "min")) out->is_dim = 1;
    if (strstr(out->quality, "aug") || (!strcmp(out->quality, "maj"))) out->is_aug = 1;
    if (strstr(out->quality, "m") && !strstr(out->quality, "maj")
        && !strstr(out->quality, "min") && !out->is_dim)
        out->is_minor = 1;

    if (*s == '_') {
        s++;
        while (*s && isdigit((unsigned char)*s) && out->bass_n < 8)
            out->bass[out->bass_n++] = *s++ - '0';
    }
}

/* ---------------------------------------------------------------- 标记展开 */

/* 和弦内音的半音偏移(相对和弦根音) */
static int chord_tone_semitone(const Chord *c, int degree, int *is_chord_tone) {
    *is_chord_tone = 1;
    switch (degree) {
        case 1: return 0;
        case 3: return c->is_minor || c->is_dim ? 3 : 4;
        case 5: return c->is_dim ? 6 : (c->is_aug ? 8 : 7);
        case 7: /* 仅七和弦有 7 音;三和弦的 7 是外音(调内音) */
            if (c->has_seventh) return c->is_maj7 ? 11 : 10;
            *is_chord_tone = 0; return 0;
        case 8: return 12;
        case 9: if (c->has_seventh) return 14;   /* 九音:仅七和弦有(实践未用,保守) */
                *is_chord_tone = 0; return 0;
        default: *is_chord_tone = 0; return 0;
    }
}

static int chord_root_step_index(char root) {
    for (int i = 0; i < 7; i++) if (kStepNames[i] == toupper((unsigned char)root)) return i;
    return 0;
}

/* 和弦根音位置标注消解(rule.md 附录 B-5 匡正):
 * `_3`/`_5` = 原位三和弦、`_7` = 原位七和弦(低音 = 根音);
 * `_6`/`_56` = 三音为低音;`_46`/`_34` = 五音为低音;`_2` = 七音为低音。
 * 返回 0 = 无低音标注;否则写入低音 (step, alter)。 */
int chord_bass_resolve(const Chord *c, const GroupState *gs, char *step, int *alter) {
    if (c->bass_n <= 0) return 0;
    int pos = 1;                       /* 位置:1=根音 3=三音 5=五音 7=七音 */
    if (c->bass_n == 1) {
        if (c->bass[0] == 2) pos = 7;
        else if (c->bass[0] == 6) pos = 3;
        else if (c->bass[0] == 4) pos = 5;
    } else if (c->bass_n >= 2) {
        if (c->bass[0] == 5 && c->bass[1] == 6) pos = 3;
        else if (c->bass[0] == 4 && c->bass[1] == 6) pos = 5;
        else if (c->bass[0] == 3 && c->bass[1] == 4) pos = 5;
    }
    int ri = chord_root_step_index(c->root);
    int semi, diat;
    if (pos == 7 && !c->has_seventh) {
        /* 三和弦的「七音」= 紧邻 8 音下方的调内音:根音级数 + 6 级 */
        char st2; int al2;
        degree_to_step_oct(gs, ri + 1 + 6, 0, &st2, &al2, NULL);
        *step = st2; *alter = al2;
        return 1;
    }
    int isct = 0;
    semi = chord_tone_semitone(c, pos, &isct);
    diat = (pos == 3) ? 2 : (pos == 5) ? 4 : (pos == 7) ? 6 : 0;
    int li = (ri + diat) % 7;
    int target = kStepSemis[ri] + c->root_alter + semi;
    int al = target - kStepSemis[li];
    while (al > 6) al -= 12;
    while (al < -6) al += 12;
    *step = kStepNames[li];
    *alter = al;
    return 1;
}

/* 把 (step, alter, octave) 写入声部 */
void voice_push(Voice *v, Note nt) {
    if (v->n >= v->cap) {
        v->cap = v->cap ? v->cap * 2 : 64;
        v->notes = (Note *)realloc(v->notes, v->cap * sizeof(Note));
    }
    nt.seq = v->n;              /* 稳定排序键:同列音保持推入顺序 */
    v->notes[v->n++] = nt;
}

/* 解析一个格内容(单音 / 柱式和弦)→ 追加入声部;返回消耗的列数 */
static int emit_unit(const char *unit, int len, const MarkDef *d, const Chord *chord,
                     const GroupState *gs, int col0, Voice *v) {
    int consumed = 0;
    int pending_shift = 0;          /* `.` 前缀标记:作用于其后一音 */
    for (int i = 0; i < len; ) {
        char c = unit[i];
        if (c == '_' || c == ' ') { i++; continue; }
        if (c == '.') { pending_shift--; i++; continue; }   /* 孤立 '.' 作前缀兜底 */
        if (c == '\'' || c == '*') { pending_shift++; i++; continue; }

        if (c == '{') {                                   /* 柱式和弦 */
            int j = i + 1;
            while (j < len && unit[j] != '}') j++;
            int first = 1;
            for (int k = i + 1; k < j; ) {
                if (unit[k] == '(') {                     /* 条件音 (7/8) */
                    int a = 0, b = 0;
                    sscanf(unit + k, "(%d/%d)", &a, &b);
                    while (k < j && unit[k] != ')') k++;
                    k++;
                    int isct = 0;
                    chord_tone_semitone(chord, a, &isct);
                    int use = isct ? a : b;
                    char st; int al; int o8 = 0;
                    degree_to_step_oct(gs, use, 0, &st, &al, &o8);
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = col0 + consumed; nt.dur = 1; nt.is_chord = !first; first = 0;
                    nt.step = st; nt.alter = al;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, o8);
                    nt.octave = oct + v->shift_oct;
                    voice_push(v, nt);
                    v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                    continue;
                }
                if (isdigit((unsigned char)unit[k])) {
                    int dg = unit[k] - '0'; k++;
                    int al = 0;
                    while (k < j && (unit[k] == '+' || unit[k] == '-' || unit[k] == '~')) {
                        if (unit[k] == '+') al++; else if (unit[k] == '-') al--; else al = 0;
                        k++;
                    }
                    char st; int alo;
                    if (d->chord_relative) {
                        int isct = 0;
                        int semi = chord_tone_semitone(chord, dg, &isct);
                        if (isct) {
                            int ri = chord_root_step_index(chord->root);
                            int o8 = 0;
                            int acc = semi + chord->root_alter;
                            while (acc < 0) { acc += 12; o8--; }
                            while (acc >= 12) { acc -= 12; o8++; }
                            int idx = ri; alo = 0;
                            for (int q = 0; q < 7; q++) {
                                int s2 = kStepSemis[(ri + q) % 7];
                                int delta = s2 - kStepSemis[ri];
                                if (delta < 0) delta += 12;
                                if (delta == acc) { idx = (ri + q) % 7; alo = 0; break; }
                                if (delta > acc) { idx = (ri + q) % 7; alo = acc - delta; break; }
                            }
                            st = kStepNames[idx];
                            Note nt; memset(&nt, 0, sizeof(nt));
                            nt.col = col0 + consumed; nt.dur = 1; nt.is_chord = !first; first = 0;
                            nt.step = st; nt.alter = alo + al;
                            int oct = v->anchor_oct;
                            if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, o8);
                            nt.octave = oct + v->shift_oct;
                            voice_push(v, nt);
                            v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                            continue;
                        }
                    }
                    int og8 = 0;
                    degree_to_step_oct(gs, dg, al, &st, &alo, &og8);
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = col0 + consumed; nt.dur = 1; nt.is_chord = !first; first = 0;
                    nt.step = st; nt.alter = alo;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, og8);
                    nt.octave = oct + v->shift_oct;
                    voice_push(v, nt);
                    v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                    continue;
                }
                if (isalpha((unsigned char)unit[k])) {    /* 和弦内绝对音 */
                    char st = (char)toupper((unsigned char)unit[k]);
                    k++;
                    if (st < 'A' || st > 'G') continue;
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = col0 + consumed; nt.dur = 1; nt.is_chord = !first; first = 0;
                    nt.step = st; nt.alter = 0;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, 0);
                    nt.octave = oct + v->shift_oct;
                    voice_push(v, nt);
                    v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                    continue;
                }
                k++;
            }
            consumed++;
            i = (j < len) ? j + 1 : len;
            continue;
        }

        if (c == '0') {                                   /* 停止 / 休止 */
            Note nt; memset(&nt, 0, sizeof(nt));
            nt.col = col0 + consumed; nt.dur = 1; nt.is_rest = 1;
            voice_push(v, nt);
            consumed++;
            i++;
            continue;
        }

        if (isdigit((unsigned char)c)) {                  /* 单音(可连续书写) */
            int dg = c - '0';
            i++;
            int al = 0, sh = pending_shift;
            pending_shift = 0;
            while (i < len) {
                char t = unit[i];
                if (t == '+') { al++; i++; }
                else if (t == '-') { al--; i++; }
                else if (t == '~') { al = 0; i++; }
                else if (t == '\'') { sh++; i++; }
                else if (t == '*') { sh++; i++; }
                else if (t == '.') { sh--; i++; }          /* '.' = 后缀:降一个八度组 */
                else break;
            }
            char st; int alo;
            if (d->chord_relative) {
                int isct = 0;
                int semi = chord_tone_semitone(chord, dg, &isct);
                if (isct) {
                    int ri = chord_root_step_index(chord->root);
                    int o8 = 0;
                    int acc = semi + chord->root_alter;
                    while (acc < 0) { acc += 12; o8--; }
                    while (acc >= 12) { acc -= 12; o8++; }
                    int idx = ri; alo = 0;
                    for (int q = 0; q < 7; q++) {
                        int s2 = kStepSemis[(ri + q) % 7];
                        int delta = s2 - kStepSemis[ri];
                        if (delta < 0) delta += 12;
                        if (delta == acc) { idx = (ri + q) % 7; alo = 0; break; }
                        if (delta > acc) { idx = (ri + q) % 7; alo = acc - delta; break; }
                    }
                    st = kStepNames[idx];
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = col0 + consumed; nt.dur = 1;
                    nt.step = st; nt.alter = alo + al;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, sh + o8);
                    else oct += sh + o8;
                    nt.octave = oct + v->shift_oct;
                    voice_push(v, nt);
                    v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
                    consumed++;
                    continue;
                }
            }
            int og8 = 0;
            degree_to_step_oct(gs, dg, al, &st, &alo, &og8);
            Note nt; memset(&nt, 0, sizeof(nt));
            nt.col = col0 + consumed; nt.dur = 1;
            nt.step = st; nt.alter = alo;
            int oct = v->anchor_oct;
            if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, sh + og8);
            else oct += sh + og8;
            nt.octave = oct + v->shift_oct;
            voice_push(v, nt);
            v->anchor_step = st; v->anchor_oct = oct; v->has_anchor = 1;
            consumed++;
            continue;
        }

        if (isalpha((unsigned char)c)) {                  /* 绝对音:A~G 音名;小写 a~f 吉他弦 */
            char st; int al = 0, sh = pending_shift;
            pending_shift = 0;
            int oct;
            if (c >= 'a' && c <= 'f') {
                if (!gs->tuning_valid) { i++; continue; }
                int si = c - 'a';
                if (gs->tuning[si][0] < 'A' || gs->tuning[si][0] > 'G' ||
                    gs->tuning[si][1] < '0' || gs->tuning[si][1] > '9') { i++; continue; }
                /* ③类吉他:有活跃和弦 → 按把位表(定义/内建常用表)取弦上音;无 → 空弦音 */
                st = gs->tuning[si][0];
                oct = gs->tuning[si][1] - '0';
                al = gs->tuning[si][2] - '2';
                const char *fr = chord ? voicing_lookup(gs, chord) : NULL;
                if (fr && si < (int)strlen(fr) && fr[si] != 'x' &&
                    fr[si] >= '0' && fr[si] <= '9') {
                    al += fr[si] - '0';                /* 品数加半音 */
                    while (al > 6) { al -= 12; oct++; }
                    while (al < -6) { al += 12; oct--; }
                }
            } else {
                st = (char)toupper((unsigned char)c);
                if (st < 'A' || st > 'G') { i++; continue; }
                oct = v->anchor_oct;
                if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, 0);
            }
            i++;
            while (i < len) {
                char t = unit[i];
                if (t == '+') { al++; i++; }
                else if (t == '-') { al--; i++; }
                else if (t == '~') { al = 0; i++; }
                else if (t == '\'' || t == '*') { sh++; i++; }
                else break;
            }
            Note nt; memset(&nt, 0, sizeof(nt));
            nt.col = col0 + consumed; nt.dur = 1;
            nt.step = st; nt.alter = al; nt.octave = oct + sh;
            voice_push(v, nt);
            v->anchor_step = st; v->anchor_oct = nt.octave; v->has_anchor = 1;
            consumed++;
            continue;
        }
        i++;                                              /* 未识别字符:跳过 */
    }
    return consumed;
}

/* 取 col 处(之前最近一个)的活动和弦;无则 NULL */
static const Chord *chord_at(const GroupState *gs, int col) {
    const Chord *ch = NULL;
    for (int c = 0; c < gs->nchords; c++)
        if (gs->chords[c].col <= col) ch = &gs->chords[c];
        else break;
    return ch;
}

/* 把定义内容铺满 span 列:内容按 _ 切分为单位,单位内逐字成格(1 格 = 1 列);
 * 周期 = 一周期总格数;不足则整周期重复(自动重复语义)。 */
void expand_mark(const MarkDef *d, const Chord *chord, const GroupState *gs,
                 int start_col, int span_cols, Voice *v, int us_cell) {
    if (!d || !d->content || span_cols <= 0 || !v) return;

    /* 每次展开拥有独立的回写地板。空白单元只能延长本次展开中
     * 最近一个非空单元产生的音，不能越过相邻标记/复制事件。 */
    int saved_floor = v->emit_floor;
    v->emit_floor = v->n;

    /* ⑥ 吉他把位表注 X&g"xxxxxx":每字符一个弦号(1 为低弦),按调弦表取弦音 */
    if (d->kind == 6) {
        if (!gs->tuning_valid) { v->emit_floor = saved_floor; return; }
        size_t clen = strlen(d->content);
        for (int col = 0; col < span_cols; col++) {
            char cc = d->content[col % (clen ? clen : 1)];
            int si;
            if (cc >= '1' && cc <= '6') si = cc - '1';
            else if (cc >= 'a' && cc <= 'f') si = cc - 'a';
            else continue;
            if (gs->tuning[si][0] < 'A' || gs->tuning[si][0] > 'G') continue;
            Note nt; memset(&nt, 0, sizeof(nt));
            nt.col = start_col + col; nt.dur = 1;
            nt.step = gs->tuning[si][0];
            nt.alter = gs->tuning[si][2] - '2';
            nt.octave = gs->tuning[si][1] - '0';
            voice_push(v, nt);
            v->anchor_step = nt.step; v->anchor_oct = nt.octave; v->has_anchor = 1;
        }
        v->emit_floor = saved_floor;
        return;
    }


    const char *c = d->content;
    char *units[256]; int lens[256]; int nu = 0;
    while (*c && nu < 256) {
        if (us_cell && *c == '_') {         /* 定义内 _ = 空格单元,本身占 1 列 */
            units[nu] = (char *)malloc(1);
            units[nu][0] = '\0'; lens[nu] = 0; nu++;
            c++;
            continue;
        }
        int l = 0;
        while (c[l] && c[l] != '_') l++;
        units[nu] = (char *)malloc(l + 1);
        memcpy(units[nu], c, l); units[nu][l] = '\0';
        lens[nu] = l; nu++;
        c += l;
        if (*c == '_') { c++; }             /* 管道/括号内容:下划线仅为分隔符 */
    }
    if (nu == 0) { v->emit_floor = saved_floor; return; }

    /* 量出周期宽度(在探针声部上试写,不落音符) */
    Voice probe; memset(&probe, 0, sizeof(probe));
    probe.anchor_oct = v->anchor_oct; probe.anchor_step = v->anchor_step;
    probe.has_anchor = v->has_anchor;
    int width = 0;
    for (int i = 0; i < nu; i++) {
        int u = emit_unit(units[i], lens[i], d, chord, gs, 0, &probe);
        width += (u > 0 ? u : 1);           /* 空格单元同样占 1 列 */
    }
    free(probe.notes);
    if (width <= 0) {
        for (int i = 0; i < nu; i++) free(units[i]);
        v->emit_floor = saved_floor;
        return;
    }

    if (width == 1) {
        /* 单单位标记(如 R&r"{135}"):一次性,不自动重复;该音持满调用跨度 */
        int q0 = v->n;
        int used = emit_unit(units[0], lens[0], d, chord, gs, start_col, v);
        if (used <= 0) used = 1;
        for (int q = v->n - 1; q >= q0; q--)          /* 只触及本标记自己的音 */
            v->notes[q].dur = span_cols;
        for (int i = 0; i < nu; i++) free(units[i]);
        v->emit_floor = saved_floor;
        return;
    }

    /* 保存调用点锚点:每个重复周期都从该锚点重新开始,避免跨周期音区累积漂移 */
    char save_step = v->anchor_step;
    int  save_oct  = v->anchor_oct;
    int  save_has  = v->has_anchor;

    /* 逐列铺排:和弦改变处重新放置标记(循环重头,单元索引归零) */
    int col = 0, ui = 0;
    int exp0 = v->emit_floor;
    const Chord *prev_ch = chord_at(gs, start_col);
    while (col < span_cols) {
        v->anchor_step = save_step; v->anchor_oct = save_oct; v->has_anchor = save_has;
        const Chord *cur = chord_at(gs, start_col + col);
        if (cur != prev_ch) { ui = 0; prev_ch = cur; }   /* 和弦改变:重新放置一次标记 */
        const Chord *use = cur ? cur : chord;
        int used = emit_unit(units[ui], lens[ui], d, use, gs, start_col + col, v);
        if (used <= 0) {
            /* 空格单元:前一音(含同刻柱式和弦)时值延长 1 列;只触及本标记自己的音 */
            if (v->n > exp0) {
                int lc = v->notes[v->n - 1].col;
                for (int q = v->n - 1; q >= exp0 && v->notes[q].col == lc; q--)
                    v->notes[q].dur += 1;
            }
            used = 1;
        }
        col += used;
        ui = (ui + 1) % nu;
    }
    for (int i = 0; i < nu; i++) free(units[i]);
    v->emit_floor = saved_floor;
}
