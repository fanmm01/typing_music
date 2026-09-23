import io

q = 'srcs/tymp2musicXML.c'
t = io.open(q, encoding='utf-8').read()

# ---- 1) 力度 / wedge / fermata direction ----
old = """typedef struct { int col; int kind; char text[24]; double tempo; } DirEv;  /* kind:1=tempo 2=words */
static DirEv g_dirs[512]; static int g_ndirs = 0;"""
new = """typedef struct { int col; int kind; char text[24]; double tempo; int level; } DirEv;
/* kind: 1=tempo 2=words 3=dynamics 4=wedge(start) 5=fermata */
static DirEv g_dirs[512]; static int g_ndirs = 0;"""
assert old in t, 'b1'
t = t.replace(old, new, 1)

old = """    if (!strcmp(w, "rit.") || !strcmp(w, "rit"))      { if (g_ndirs<512){g_dirs[g_ndirs++]=(DirEv){col,2,"rit.",0};} }"""
new = """    int dl = parse_dynamic_level(tok);
    if (dl > 0)                                     { if (g_ndirs<512){g_dirs[g_ndirs++]=(DirEv){col,3,"",0,dl};} return; }
    if (!strcmp(w, "cresc") || !strcmp(w, "si-") || !strcmp(w, "dim") || !strcmp(w, "si+")) {
        if (g_ndirs<512){g_dirs[g_ndirs++]=(DirEv){col,4,(!strcmp(w,"dim")||!strcmp(w,"si+"))?"diminuendo":"crescendo",0,0};} return;
    }
    if (!strcmp(w, "se"))                           { if (g_ndirs<512){g_dirs[g_ndirs++]=(DirEv){col,4,"stop",0,0};} return; }
    if (tok[0]=='|' && (tok[1]=='\\xe3' || (unsigned char)tok[1]==0xE3)) { if (g_ndirs<512){g_dirs[g_ndirs++]=(DirEv){col,5,"",0,0};} return; } /* |。 */
    if (!strcmp(w, "rit.") || !strcmp(w, "rit"))      { if (g_ndirs<512){g_dirs[g_ndirs++]=(DirEv){col,2,"rit.",0};} }"""
assert old in t, 'b2'
t = t.replace(old, new, 1)

old = """static void write_direction_words(FILE *fp, int m, int measure_cols) {
    for (int i = 0; i < g_ndirs; i++) {
        if (g_dirs[i].kind != 2) continue;
        if (g_dirs[i].col / measure_cols != m) continue;
        fprintf(fp, "      <direction>\\n        <direction-type><words>%s</words></direction-type>\\n",
                g_dirs[i].text);
        if (g_dirs[i].tempo > 0)
            fprintf(fp, "        <sound tempo=\\"%.0f\\"/>\\n", g_dirs[i].tempo);
        fprintf(fp, "      </direction>\\n");
    }
}"""
new = """static void write_direction_words(FILE *fp, int m, int measure_cols) {
    static const char *dyn_names[10] = { "pppp", "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff", "ffff" };
    for (int i = 0; i < g_ndirs; i++) {
        if (g_dirs[i].col / measure_cols != m) continue;
        if (g_dirs[i].kind == 2) {
            fprintf(fp, "      <direction>\\n        <direction-type><words>%s</words></direction-type>\\n",
                    g_dirs[i].text);
            if (g_dirs[i].tempo > 0)
                fprintf(fp, "        <sound tempo=\\"%.0f\\"/>\\n", g_dirs[i].tempo);
            fprintf(fp, "      </direction>\\n");
        } else if (g_dirs[i].kind == 3 && g_dirs[i].level >= 1 && g_dirs[i].level <= 10) {
            fprintf(fp, "      <direction>\\n        <direction-type><dynamics><%s/></dynamics></direction-type>\\n      </direction>\\n",
                    dyn_names[g_dirs[i].level - 1]);
        } else if (g_dirs[i].kind == 4) {
            fprintf(fp, "      <direction>\\n        <direction-type><wedge type=\\"%s\\"/></direction-type>\\n      </direction>\\n",
                    g_dirs[i].text);
        } else if (g_dirs[i].kind == 5) {
            fprintf(fp, "      <direction>\\n        <direction-type><words>fermata</words></direction-type>\\n      </direction>\\n");
        }
    }
}"""
assert old in t, 'b3'
t = t.replace(old, new, 1)

# ---- 2) 声部级固定变音:创建时从组状态拷贝 ----
old = """        v->octave_base = 4;
        const char *pr = strchr(h, '>');
        const char *ps = strchr(h, '*');
        if (pr) v->octave_base = atoi(pr + 1);
        else if (ps && isdigit((unsigned char)ps[1])) v->octave_base = atoi(ps + 1);
        v->anchor_oct = v->octave_base;"""
new = """        v->octave_base = 4;
        const char *pr = strchr(h, '>');
        const char *ps = strchr(h, '*');
        if (pr) v->octave_base = atoi(pr + 1);
        else if (ps && isdigit((unsigned char)ps[1])) v->octave_base = atoi(ps + 1);
        v->anchor_oct = v->octave_base;
        for (int f2 = 0; f2 < 8; f2++) v->fixed_alter[f2] = ctx.gs.fixed_alter[f2];"""
assert old in t, 'b4'
t = t.replace(old, new, 1)

# ---- 3) 固定变音段(apply_segment 开头:@[...] → 更新声部 fixed_alter) ----
old = """static void apply_segment(Ctx *ctx, Voice *v, const char *text, int base_col) {
    if (!text || !v) return;
    char *tmp = dup_str(text);
    for (char *p = tmp; *p; p++) if (*p == '/') *p = ' ';    /* 机动行空格替代 */"""
new = """static void apply_segment(Ctx *ctx, Voice *v, const char *text, int base_col) {
    if (!text || !v) return;
    /* |@[...] 固定变音段:更新本声部固定变音(近似:不重算已解析音符) */
    if (text[0] == '@') {
        GroupState tmpg = ctx->gs;
        parse_fixed_acc(text, &tmpg);
        for (int f2 = 0; f2 < 8; f2++) v->fixed_alter[f2] = tmpg.fixed_alter[f2];
        if (g_verbose) fprintf(stderr, "  固定变音段(列%d)→ 声部固定变音已更新\\n", base_col);
        return;
    }
    char *tmp = dup_str(text);
    for (char *p = tmp; *p; p++) if (*p == '/') *p = ' ';    /* 机动行空格替代 */"""
assert old in t, 'b5'
t = t.replace(old, new, 1)

# ---- 4) row_to_voice 用声部级 fixed_alter ----
t = t.replace('gs->fixed_alter[dg == 8 ? 1 : dg]', 'v->fixed_alter[dg == 8 ? 1 : dg]')
old = "                    int fa = (dg >= 1 && dg <= 7) ? gs->fixed_alter[dg] : 0;"
new = "                    int fa = (dg >= 1 && dg <= 7) ? v->fixed_alter[dg] : 0;"
assert old in t, 'b6'
t = t.replace(old, new, 1)

# ---- 5) |__ 细分(IT_EQ:内容以 _ 开头 → 每格细分 1/2^k) ----
old = """            if (it->text && it->text[0] && it->text[0] != '[') {
                MarkDef tmp; memset(&tmp, 0, sizeof(tmp));
                tmp.content = it->text; tmp.kind = 2;
                const Chord *ch = NULL;
                for (int c = gs->nchords - 1; c >= 0; c--)
                    if (gs->chords[c].col <= it->col) { ch = &gs->chords[c]; break; }
                Chord dummy; memset(&dummy, 0, sizeof(dummy)); dummy.root = 'C';
                expand_mark(&tmp, ch ? ch : &dummy, gs, it->col, span, v);"""
new = """            if (it->text && it->text[0] && it->text[0] != '[') {
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
                expand_mark(&tmp, ch ? ch : &dummy, gs, it->col, span, v);
                if (subdiv > 0) {
                    /* 每格时值 = 1/2^subdiv 列(tick 粒度) */
                    int tick = (g_divisions / 4) >> subdiv;
                    if (tick < 1) tick = 1;
                    for (int k2 = before2; k2 < v->n; k2++) {
                        if (!v->notes[k2].is_chord) v->notes[k2].dur_tick = tick;
                    }
                }"""
assert old in t, 'b7'
t = t.replace(old, new, 1)

# ---- 6) 清理:write_attributes 壳合并 ----
old = """static void write_attributes(FILE *fp, const GroupState *gs) { (void)0; }
static void write_attributes_kind(FILE *fp, const GroupState *gs, int kind) {"""
new = """static void write_attributes_kind(FILE *fp, const GroupState *gs, int kind) {"""
assert old in t, 'b8'
t = t.replace(old, new, 1)
t = t.replace('write_attributes_kind(wfp, &ctx.gs, v->kind);',
              'write_attributes_kind(wfp, &ctx.gs, v->kind);', 1)

io.open(q, 'w', encoding='utf-8', newline='\n').write(t)
print('p7 ok')
