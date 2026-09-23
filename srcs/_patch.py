import io

p = 'srcs/tympUtils.c'
s = io.open(p, encoding='utf-8').read()

def rep(old, new, must=1):
    global s
    n = s.count(old)
    assert n >= must, 'NOT FOUND: %r (found %d)' % (old[:60], n)
    s = s.replace(old, new)
    print('ok: %r -> %d' % (old[:50], n))

# --- emit_unit: 条件音 (7/8) 的 use=8 ---
rep("""                    int use = isct ? a : b;
                    char st; int al;
                    degree_to_step(gs, use, 0, &st, &al);""",
"""                    int use = isct ? a : b;
                    char st; int al; int o8 = 0;
                    degree_to_step_oct(gs, use, 0, &st, &al, &o8);""")
rep("""                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, 0);
                    nt.octave = oct;
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
                            int acc = semi + chord->root_alter;
                            while (acc < 0) acc += 12;
                            while (acc >= 12) acc -= 12;""",
"""                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, o8);
                    nt.octave = oct;
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
                            while (acc >= 12) { acc -= 12; o8++; }""")

# --- emit_unit: 和弦内 isct resolve 加 o8 ---
rep("""                            st = kStepNames[idx];
                            Note nt; memset(&nt, 0, sizeof(nt));
                            nt.col = col0 + consumed; nt.dur = 1; nt.is_chord = !first; first = 0;
                            nt.step = st; nt.alter = alo + al;
                            int oct = v->anchor_oct;
                            if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, 0);
                            nt.octave = oct;""",
"""                            st = kStepNames[idx];
                            Note nt; memset(&nt, 0, sizeof(nt));
                            nt.col = col0 + consumed; nt.dur = 1; nt.is_chord = !first; first = 0;
                            nt.step = st; nt.alter = alo + al;
                            int oct = v->anchor_oct;
                            if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, o8);
                            nt.octave = oct;""")

# --- emit_unit: 和弦内普通 digit ---
rep("""                    degree_to_step(gs, dg, al, &st, &alo);
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = col0 + consumed; nt.dur = 1; nt.is_chord = !first; first = 0;
                    nt.step = st; nt.alter = alo;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, 0);
                    nt.octave = oct;""",
"""                    int og8 = 0;
                    degree_to_step_oct(gs, dg, al, &st, &alo, &og8);
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = col0 + consumed; nt.dur = 1; nt.is_chord = !first; first = 0;
                    nt.step = st; nt.alter = alo;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, og8);
                    nt.octave = oct;""")

# --- emit_unit: 单音 isct resolve 加 o8 ---
rep("""                    st = kStepNames[idx];
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = col0 + consumed; nt.dur = 1;
                    nt.step = st; nt.alter = alo + al;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, sh);
                    else oct += sh;
                    nt.octave = oct;""",
"""                    st = kStepNames[idx];
                    Note nt; memset(&nt, 0, sizeof(nt));
                    nt.col = col0 + consumed; nt.dur = 1;
                    nt.step = st; nt.alter = alo + al;
                    int oct = v->anchor_oct;
                    if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, sh + o8);
                    else oct += sh + o8;
                    nt.octave = oct;""")

# --- emit_unit: 单音 isct 内 acc 归一化带 o8 ---
rep("""            if (d->chord_relative) {
                int isct = 0;
                int semi = chord_tone_semitone(chord, dg, &isct);
                if (isct) {
                    int ri = chord_root_step_index(chord->root);
                    int acc = semi + chord->root_alter;
                    while (acc < 0) acc += 12;
                    while (acc >= 12) acc -= 12;""",
"""            if (d->chord_relative) {
                int isct = 0;
                int semi = chord_tone_semitone(chord, dg, &isct);
                if (isct) {
                    int ri = chord_root_step_index(chord->root);
                    int o8 = 0;
                    int acc = semi + chord->root_alter;
                    while (acc < 0) { acc += 12; o8--; }
                    while (acc >= 12) { acc -= 12; o8++; }""")

# --- emit_unit: 单音普通 ---
rep("""            degree_to_step(gs, dg, al, &st, &alo);
            Note nt; memset(&nt, 0, sizeof(nt));
            nt.col = col0 + consumed; nt.dur = 1;
            nt.step = st; nt.alter = alo;
            int oct = v->anchor_oct;
            if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, sh);
            else oct += sh;
            nt.octave = oct;""",
"""            int og8 = 0;
            degree_to_step_oct(gs, dg, al, &st, &alo, &og8);
            Note nt; memset(&nt, 0, sizeof(nt));
            nt.col = col0 + consumed; nt.dur = 1;
            nt.step = st; nt.alter = alo;
            int oct = v->anchor_oct;
            if (v->has_anchor) resolve_octave(v->anchor_step, v->anchor_oct, st, &oct, sh + og8);
            else oct += sh + og8;
            nt.octave = oct;""")

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('ALL OK')
