import io

# ============ tympImpl.h:GroupState 加把位表 ============
h = 'srcs/tympImpl.h'
t = io.open(h, encoding='utf-8').read()
old = "    MarkDef *defs; int ndefs, defcap;\n    Chord   *chords; int nchords, chordcap;\n    int   accu;           /* 每拍列数 */"
new = "    MarkDef *defs; int ndefs, defcap;\n    Chord   *chords; int nchords, chordcap;\n    char  voic_name[32][16];\n    char  voic_fret[32][8];\n    int   nvoic;          /* 和弦把位表(④类/{D/200232}) */\n    int   accu;           /* 每拍列数 */"
assert old in t, 'h1'
t = t.replace(old, new, 1)
io.open(h, 'w', encoding='utf-8', newline='\n').write(t)

# ============ tympUtils.c:把位表函数 + 内建常用表 ============
p = 'srcs/tympUtils.c'
s = io.open(p, encoding='utf-8').read()

anchor = "/* ---------------------------------------------------------------- 标记定义 */"
add = '''/* ---------------------------------------------------------------- 吉他把位表 */

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
            gs->voic_fret[i][7] = '\\0';
            return;
        }
    }
    strncpy(gs->voic_name[gs->nvoic], name, 15);
    gs->voic_name[gs->nvoic][15] = '\\0';
    strncpy(gs->voic_fret[gs->nvoic], frets, 7);
    gs->voic_fret[gs->nvoic][7] = '\\0';
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
    out[n] = '\\0';
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

/* ---------------------------------------------------------------- 标记定义 */'''
assert anchor in s, 'u1'
s = s.replace(anchor, add, 1)

# emit_unit a~f 分支:查把位表取品数
old = """        if (c0 >= 'a' && c0 <= 'f') {
            if (!gs->tuning_valid) { i++; continue; }
            int si = c0 - 'a';
            if (gs->tuning[si][0] < 'A' || gs->tuning[si][0] > 'G' ||
                gs->tuning[si][1] < '0' || gs->tuning[si][1] > '9') { i++; continue; }
            st = gs->tuning[si][0];
            oct = gs->tuning[si][1] - '0';
            al = gs->tuning[si][2] - '2';
        } else {"""
new = """        if (c0 >= 'a' && c0 <= 'f') {
            if (!gs->tuning_valid) { i++; continue; }
            int si = c0 - 'a';
            if (gs->tuning[si][0] < 'A' || gs->tuning[si][0] > 'G' ||
                gs->tuning[si][1] < '0' || gs->tuning[si][1] > '9') { i++; continue; }
            /* ③类吉他:有活跃和弦 → 按把位表(定义/内建常用表)取弦上音;无 → 空弦音 */
            st = gs->tuning[si][0];
            oct = gs->tuning[si][1] - '0';
            al = gs->tuning[si][2] - '2';
            const char *fr = chord ? voicing_lookup(gs, chord) : NULL;
            if (fr && si < (int)strlen(fr) && fr[si] != 'x' &&
                fr[si] >= '0' && fr[si] <= '9') {
                al += fr[si] - '0';                    /* 品数加半音 */
                while (al > 6) { al -= 12; oct++; }
                while (al < -6) { al += 12; oct--; }
            }
        } else {"""
assert old in s, 'u2'
s = s.replace(old, new, 1)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('p9 ok')
