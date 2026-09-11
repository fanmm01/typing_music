#include "tymp.h"



// 拍号结构
typedef struct timesign {
    int num;  // 分子
    int den;  // 分母
} timesign;

typedef struct AppearTime{
    char* name;
    int time;
    InstrumentPrototype prototype;
} AppearTime;

const char gt[6][3] = {"E2", "A2", "D3", "G3", "B3", "E3"};

/*仅供测试用：跨平台获取源文件目录*/
void get_source_dir(char *dir_buf, size_t buf_size) {
    const char *src = __FILE__;
    size_t len = strlen(src);
    if (!dir_buf || buf_size == 0) return;
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    for (size_t i = len; i > 0; --i) {
        if (src[i - 1] == sep) {
            size_t n = i - 1;
            if (n >= buf_size) n = buf_size - 1;   /* 不越界 */
            memcpy(dir_buf, src, n);
            dir_buf[n] = '\0';
            return;
        }
    }
    strncpy(dir_buf, ".", buf_size - 1);
    dir_buf[buf_size - 1] = '\0';
}


/* 初始化一个 line 的行首。 */
int write_linehead(FILE *fp, char *tok, int chsBef) {
    fprintf(fp, "%-*.*s", chsBef, chsBef, tok);
    fprintf(fp, "||");
    return 0;
}

/* 初始化一个新的空 line，为其添加足够数量的空格备用。 */
int write_blankline(FILE *fp, int n, char *tok, int chaBef) {
    (void)n;  // 参数 n 暂未使用，抑制警告
    write_linehead(fp, tok, chaBef);
    for (int i = 0; i < MAX_NUM; i++) {
        fprintf(fp, " ");
    }
    fprintf(fp, "\n");  // 补换行，否则所有内容挤在一行
    return 0;
}

/* 获得相对意义上的精度。 */
int get_related_accurcy(timesign time, int Accuracy) {
    if (time.den <= 0) return 4;  /* 分母非法时回退默认精度，避免除零 */
    int relaccu;
    switch (Accuracy) {
        case 1:
        case 2:
        case 3:
        case 4:
            relaccu = Accuracy;
            break;
        case 8:
        case 16:
        case 32: {
            int tok = time.den;
            relaccu = Accuracy / tok;
            break;
        }
        default:
            relaccu = 4;
            break;
    }
    // 保证至少精度为 1，避免 relaccu=0 导致除零/死循环
    if (relaccu < 1) relaccu = 1;
    return relaccu;
}

/* 时间戳提示。 */
int write_timestp(FILE *fp, timesign time, int chaBef, int Accuracy) {
    // 非法拍号直接跳过：既避免负分子导致的死循环，也避免输出悬空的行头
    if (time.num <= 0 || time.den <= 0) return 0;
    int accu = get_related_accurcy(time, Accuracy);
    if (accu <= 0) return 0;
    write_linehead(fp, "#timestamp:", chaBef);
    // 每小节：num 拍，每拍 accu 个分格 → 一小节共 num*accu 个标记
    for (int i = 0; (i * time.num * accu) < MAX_NUM; i++) {
        for (int j = 0; j < time.num; j++) {
            // 每小节第一拍标 '|'，其余标 '.'
            fprintf(fp, "%c", (j == 0) ? '|' : '.');
            for (int k = 0; k < accu - 1; k++) {
                fprintf(fp, " ");
            }
        }
    }
    fprintf(fp, "\n");
    return 0;
}

/* 为格式化乐谱文件添加 n 个新的、非首行的空乐谱行。 */
int newline_nonFirst(FILE *fp,
        int n, timesign time, int chaBef,
        int numofHarm, int numofAcco,
        int Accuracy, Instrument *instruments) {
    for (int i = 0; i < n; i++) {
        fprintf(fp, "\n");
        write_timestp(fp, time, chaBef, Accuracy);
        write_blankline(fp, 1, "{ ", chaBef);
        write_blankline(fp, 1, "chords:", chaBef);
        write_blankline(fp, 1, "", chaBef);   /* 旋律声部第一行 */
        write_blankline(fp, 1, "", chaBef);   /* 旋律声部前的空行 */
        for (int j = 0; j < numofHarm; j++) {
            write_blankline(fp, 1, "", chaBef);  /* 和声声部前的空行 */
            write_blankline(fp, 1, "[", chaBef);
            write_blankline(fp, 1, "", chaBef);
        }
        write_timestp(fp, time, chaBef, Accuracy);
        for (int j = 0; j < numofAcco; j++) {
            char headtok[64];
            snprintf(headtok, sizeof(headtok), "(-%s-", instruments[j].score_part_id);
            write_blankline(fp, 1, "", chaBef);     /* 伴奏声部前的空行 */
            write_blankline(fp, 1, headtok, chaBef);/* 后续组只体现结构 id */
            write_blankline(fp, 1, "", chaBef);
            if ((j != 0) && (j % 5 == 0)) write_timestp(fp, time, chaBef, Accuracy);
        }
        write_blankline(fp, 1, "}", chaBef);
    }
    return 0;
}

// 提供吉他六弦空把位音调。
char *getGT() {
    char *gttok = (char *)malloc(100 * sizeof(char));
    sprintf(gttok, "&gt: {a=%s,b=%s,c=%s,d=%s,e=%s,f=%s}",
            gt[0], gt[1], gt[2], gt[3], gt[4], gt[5]);
    return gttok;
}

// 建立一个新的首行。
int new_firstLine(FILE *fp, char *keyroot,
        timesign time, double speed,
        int numofHarm, int numofAcco,
        int numofLines, int Accuracy,
        int chaBef, Instrument *instruments) {
    (void)numofLines;  // 暂未使用
    char *firstline_tok = (char *)malloc(100 * sizeof(char));
    char *gttok = getGT();  // 直接赋值，不再先 malloc（避免泄漏）
    sprintf(firstline_tok, "{ |%d/%d |=%s |vb%lf %s}",
            time.num, time.den, keyroot, speed, gttok);
    int fltokN = strlen(firstline_tok);
    int flChaBef = fltokN + (-1 + 4 - (fltokN + 1) % 4);
    int RealChaBef = MAX(flChaBef, chaBef);
    write_timestp(fp, time, RealChaBef, Accuracy);
    write_blankline(fp, 1, firstline_tok, RealChaBef);
    write_blankline(fp, 1, "chords,eg.C,Am,Bmin7:", RealChaBef);
    write_blankline(fp, 1, "", RealChaBef);   /* 旋律声部前的空行 */
    write_blankline(fp, 1, "/*LV.pitch range of 1st note:*/ *3", RealChaBef);
    write_blankline(fp, 1, "/*lirics*/", RealChaBef);
    for (int j = 0; j < numofHarm; j++) {
        write_blankline(fp, 1, "", RealChaBef);  /* 和声声部前的空行 */
        write_blankline(fp, 1, "[/*Harmony*/", RealChaBef);
        write_blankline(fp, 1, "/*lyrics*/", RealChaBef);
    }
    write_timestp(fp, time, RealChaBef, Accuracy);
    for (int j = 0; j < numofAcco; j++) {
        Instrument *inst = &instruments[j];
        char headtok[128];
        /* 首组行头并列显示 结构id<英文全名，第三行放中文全名 */
        if (j == 0) snprintf(headtok, sizeof(headtok),
                "(Accomponiment#%d:%s<%s; Initial pitch range: *4",
                j, inst->score_part_id, inst->part_name_En);
        else snprintf(headtok, sizeof(headtok),
                "(Accomponiment#%d:%s<%s;                      *4",
                j, inst->score_part_id, inst->part_name_En);
        write_blankline(fp, 1, "", RealChaBef);  /* 伴奏声部前的空行 */
        write_blankline(fp, 1, headtok, RealChaBef);
        write_blankline(fp, 1, inst->part_name_Zh ? inst->part_name_Zh : "", RealChaBef);
        if ((j != 0) && (j % 5 == 0)) write_timestp(fp, time, RealChaBef, Accuracy);
    }
    write_blankline(fp, 1, "}", RealChaBef);

    free(firstline_tok);
    free(gttok);  // 释放 getGT 分配的内存
    return 0;
}


/* 用户可选候选的最大数量，调用方 options 数组须至少这么大 */
#define MAX_INSTRUMENT_OPTIONS 16

/*
在乐器原型表中检索 inputInst。
insts 容量须 >= MAX_INSTRUMENT_OPTIONS，返回候选数量（至少 1，兜底为通用 INST）。
*/
int findInstruments(InstrumentPrototype* insts,char* inputInst){
    int j = 0;
    if (!insts || !inputInst) return 0;
    for (int i = 0;i < NUM_OF_INSTRUMENT_GROUPS;i++){
        if (!strcmp(inputInst, InstrumentGroupList[i].chinese) ||
            !strcmp(inputInst, InstrumentGroupList[i].group_name)) {
            /* 整组匹配：加入组内全部乐器，受候选上限约束 */
            for (int k = InstrumentGroupList[i].start_index;
                 k <= InstrumentGroupList[i].end_index && j < MAX_INSTRUMENT_OPTIONS;k++){
                insts[j++] = InstrumentPrototypeList[k];
            }
            return j;
        }
    }
    for (int i = 0;i < NUM_OF_INSTRUMENT_PROTS && j < MAX_INSTRUMENT_OPTIONS;i++){
        if (!strcmp(inputInst, InstrumentPrototypeList[i].base_id) ||
                !strcmp(inputInst, InstrumentPrototypeList[i].base_name) ||
                !strcmp(inputInst, InstrumentPrototypeList[i].chinese)) {
            insts[j++] = InstrumentPrototypeList[i];  /* 精确匹配：唯一结果 */
            return j;
        } else
        if (strstr(inputInst,InstrumentPrototypeList[i].base_id)
                || strstr(inputInst,InstrumentPrototypeList[i].base_name)
                ||strstr(inputInst,InstrumentPrototypeList[i].chinese) ||
                strstr(InstrumentPrototypeList[i].base_id, inputInst)
                || strstr(InstrumentPrototypeList[i].base_name, inputInst)
                || strstr(InstrumentPrototypeList[i].chinese, inputInst))
            insts[j++] = InstrumentPrototypeList[i];
    }
    if (j != 0) return j;
    /* 兜底：通用乐器，交由调用方进入自定义命名流程 */
    insts[0] = InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS-1];
    return 1;
}

/* 读取一行输入到 out_name（容量由调用方保证 >= 50），返回通用 INST 原型 */
static InstrumentPrototype ask_selfdefine(char *out_name){
    char buf[50];
    printf("Input the name or 'I' for depending on system: ");
    scanf("%49s", buf);
    if (out_name) strcpy(out_name, buf);
    return InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS-1];
}

/*
交互式选择第 index 件伴奏乐器。
选中的名字（中文名或自定义名）会写入 out_name（调用方保证容量 >= 50）。
*/
InstrumentPrototype searchAndSetInstrument(int index,char* out_name){
    char inputInstrument[50];
    printf("Instrument #%d name: ",index);
    scanf("%49s", inputInstrument);
    InstrumentPrototype options[MAX_INSTRUMENT_OPTIONS];
    int n = findInstruments(options,inputInstrument);
    if (n > 1){
        printf("Insert number to choose(-1 to self-define)\n");
        for (int i = 0;i < n;i++){
            if (options[i].base_id != NULL)
                printf("%d,%s %s %s %d\n",
                    i,
                    options[i].base_id,
                    options[i].base_name,
                    options[i].chinese,
                    options[i].gm_program);
            else break;
        }
        int j = -2;
        while (j < -1 || j >= n){   /* 非法选择重新提示，避免坠底返回 */
            printf("choice: ");
            fflush(stdout);
            if (scanf("%d",&j) != 1){
                int c; while ((c = getchar()) != '\n' && c != EOF) {}
                j = -2;
            }
        }
        if (j == -1) return ask_selfdefine(out_name);
        if (out_name) strcpy(out_name, options[j].chinese);
        return options[j];
    }
    if (n == 1 && strcmp(options[0].base_id,"INST")){
        if (out_name) strcpy(out_name, options[0].chinese);
        return options[0];
    }
    /* n == 0（兜底 INST）或精确命中通用 INST：走自定义命名 */
    return ask_selfdefine(out_name);
}

/* 复制字符串（替代 strdup，避免 mingw 下的特性宏问题）；失败返回 NULL */
static char *dup_str(const char *s){
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *p = (char *)malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

/*
由原型与序号构造一个 Instrument，四个字符串字段全部堆分配，
由 FreeInstrument / free_instruments 统一释放。
prototype.base_id 为 NULL 时返回全零结构。
*/
Instrument MakeInstrumentFromPrototype(InstrumentPrototype prototype,int index, const char* name){
    Instrument instrument = {0};
    if (prototype.base_id == NULL) return instrument;
    bool isChineseName = contains_chinese(name);
    instrument.score_part_id = (char *)malloc(strlen(prototype.base_id) + 16);
    instrument.instrument_id = (char *)malloc(strlen(prototype.base_id) + 20);
    sprintf(instrument.score_part_id,"%s%d",prototype.base_id,index);
    sprintf(instrument.instrument_id,"%s%d-I1",prototype.base_id,index);
    if (strcmp(prototype.base_id, "INST") == 0){
        if (isChineseName){
            instrument.part_name_En = (char *)malloc(strlen(prototype.base_name) + 16);
            sprintf(instrument.part_name_En,"%s#%d",prototype.base_name,index);
            instrument.part_name_Zh = dup_str(name);
        } else{
            instrument.part_name_En = dup_str(name);
            instrument.part_name_Zh = (char *)malloc(strlen(prototype.chinese) + 16);
            sprintf(instrument.part_name_Zh,"%s#%d",prototype.chinese,index);
        }
    }
    else {
        instrument.part_name_En = dup_str(prototype.base_name);
        instrument.part_name_Zh = dup_str(prototype.chinese);
    }
    instrument.prototype = prototype;
    return instrument;
}

/* mode 0/1 的默认乐器：结构 id 形如 "Inst.#i"，与既有输出格式保持一致 */
static Instrument MakeDefaultInstrument(int index){
    Instrument instrument = {0};
    char buf[32];
    snprintf(buf, sizeof(buf), "Inst.#%d", index);
    instrument.score_part_id = dup_str(buf);
    snprintf(buf, sizeof(buf), "Inst.#%d-I1", index);
    instrument.instrument_id = dup_str(buf);
    snprintf(buf, sizeof(buf), "Instrument#%d", index);
    instrument.part_name_En = dup_str(buf);
    snprintf(buf, sizeof(buf), "乐器#%d", index);
    instrument.part_name_Zh = dup_str(buf);
    instrument.prototype = InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS-1];
    return instrument;
}

/* 释放单个 Instrument 的四个字符串字段并清零，NULL 安全 */
static void FreeInstrument(Instrument *inst){
    if (!inst) return;
    free(inst->score_part_id);
    free(inst->instrument_id);
    free(inst->part_name_En);
    free(inst->part_name_Zh);
    memset(inst, 0, sizeof(*inst));
}


/*
在出现表里查找 context（原型基础名）对应条目，未命中则返回第一个空槽。
注意：比较的是已存条目的原型名，而不是显示名——同一原型（两把小提琴等）
必须合并到同一条目才能按 0,1,2... 编号；空槽以 time==0 判定。
*/
int findIfContextInAppeartimeList(const char *context, AppearTime *Appts){
    for (int i = 0;;i++){
        if (Appts[i].time == 0 || !Appts[i].prototype.base_name) return i;  /* 空槽 */
        if (!strcmp(context,Appts[i].prototype.base_name)) return i;
    }
}

/*
把原型列表整理成乐器实例列表：同一原型（两把小提琴等）合并计数，
按 0,1,2... 依次编号，保证结构 id 唯一。
返回写入 instrumentlist 的乐器总数。
*/
int GetInstrumentsFromPrototypes(Instrument *instrumentlist, InstrumentPrototype *proptypeList,int NumOfProplist, char** names){
    if (NumOfProplist <= 0) return 0;
    AppearTime *apprTimeList = calloc(NumOfProplist, sizeof(AppearTime));
    if (!apprTimeList) return 0;
    int n = 0;
    for (int i = 0;i < NumOfProplist;i++){
        int dex = findIfContextInAppeartimeList(proptypeList[i].base_name,apprTimeList);
        if (!apprTimeList[dex].name) apprTimeList[dex].name = names[i];  /* 保留第一个输入名 */
        apprTimeList[dex].time++;
        apprTimeList[dex].prototype = proptypeList[i];
        if (dex==n) n++;
    }
    int out = 0;
    for (int k = 0;k < n;k++){
        for (int j = 0;j < apprTimeList[k].time;j++){
           instrumentlist[out++] = MakeInstrumentFromPrototype(
                   apprTimeList[k].prototype,j,apprTimeList[k].name);
        }
    }
    free (apprTimeList);
    return out;
}

/*
初始化一个新的空格式化乐谱文件。
instruments: 乐器数组，长度为 numofAcco；score_part_id 为 NULL 的槽位
会被填充为默认乐器（由调用方的 free_instruments 负责释放）。
*/
int newfileMaker(char *filename, char *title, char *author, char *keyroot,
        timesign timesign, double speed,
        int numofHarm, int numofAcco,
        int numofLines, int Accuracy, Instrument *instruments) {

    //测试用：
    char dir[1024];
    get_source_dir(dir, sizeof(dir));
    char file[2048];
    #if defined(_WIN32)
        snprintf(file, sizeof(file), "%s\\%s", dir, filename);
    #else
        snprintf(file, sizeof(file), "%s/%s", dir, filename);
    #endif

    FILE *fp;
    fp = fopen(file, "w");
    if (!fp) {
        printf("无法打开文件 %s\n", filename);
        return -1;
    }

    fprintf(fp, "%s by %s\ninstruments:", title, author);

    // 补全缺失的乐器，并写出乐器表（结构 id）
    for (int i = 0; i < numofAcco; i++) {
        if (!instruments[i].score_part_id) {
            instruments[i] = MakeDefaultInstrument(i);
        }
        fprintf(fp, "%s ", instruments[i].score_part_id);
    }
    fprintf(fp, "\n");

    new_firstLine(fp, keyroot, timesign, speed,
            numofHarm, numofAcco, numofLines,
            Accuracy, 38, instruments);
    newline_nonFirst(fp, numofLines, timesign, 18,
            numofHarm, numofAcco, Accuracy, instruments);

    fclose(fp);
    return 0;
}

// 安全释放 instruments（数组与其中每件乐器的字符串字段）
void free_instruments(Instrument *instruments, int numofAcco) {
    if (!instruments) return;
    for (int i = 0; i < numofAcco; i++) {
        FreeInstrument(&instruments[i]);
    }
    free(instruments);
}

/* ---------- 输入助手：保证读入合法的值，非法输入重新提示 ---------- */
static void ask_int(const char *prompt, int *out) {
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (scanf("%d", out) == 1) return;
        int c; while ((c = getchar()) != '\n' && c != EOF) {}
    }
}

static void ask_double(const char *prompt, double *out) {
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (scanf("%lf", out) == 1) return;
        int c; while ((c = getchar()) != '\n' && c != EOF) {}
    }
}

/* 读入字符串，cap 为缓冲区实际容量（读入至多 cap-1 个非空白字符） */
static void ask_str(const char *prompt, char *buf, int cap) {
    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%%ds", cap - 1);
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (scanf(fmt, buf) == 1) return;
        int c; while ((c = getchar()) != '\n' && c != EOF) {}
    }
}

/* 校验影响分配的输入参数，防止负值/零值导致非法分配或死循环 */
static int validate_counts(timesign ts, double speed, int numofHarm,
        int numofAcco, int numofLines, int Accuracy) {
    return ts.num > 0 && ts.den > 0 && speed > 0
        && numofHarm >= 0 && numofAcco >= 1
        && numofLines >= 0 && Accuracy > 0;
}

/* 释放 main 中四个基础字符串缓冲区 */
static void free_basic_buffers(char *filename, char *title,
        char *author, char *keyroot) {
    free(filename);
    free(title);
    free(author);
    free(keyroot);
}

int main() {
    char *filename = (char *)malloc(50);
    char *title = (char *)malloc(50);
    char *author = (char *)malloc(50);
    char *keyroot = (char *)malloc(4);  // 放大到 4 字节更安全
    timesign timesign;
    double speed;
    int numofHarm, numofAcco, numofLines, Accuracy;
    Instrument *instruments = NULL;
    int mode;

    if (!filename || !title || !author || !keyroot) {
        printf("内存分配失败。\n");
        free_basic_buffers(filename, title, author, keyroot);
        return 1;
    }

    ask_int("mode(0:快速配置; 1:部分手动; 2:全手动): ", &mode);
    ask_str("filename: ", filename, 50);

    switch (mode) {
        case 0:
            strcpy(title, "song");
            strcpy(author, "author");
            strcpy(keyroot, "C");
            ask_int("Numerator of timesign: ", &timesign.num);
            ask_int("Denominator: ", &timesign.den);
            speed = 120.0;
            numofHarm = 2;
            numofAcco = 5;
            numofLines = 10;
            Accuracy = 4;

            if (!validate_counts(timesign, speed, numofHarm, numofAcco,
                    numofLines, Accuracy)) {
                printf("非法输入。\n");
                free_basic_buffers(filename, title, author, keyroot);
                return 1;
            }
            /* 默认乐器由 newfileMaker 填充 */
            instruments = (Instrument *)calloc(numofAcco, sizeof(Instrument));
            break;

        case 1: {
            ask_str("Title: ", title, 50);
            ask_str("Author: ", author, 50);
            ask_str("Key root (e.g., C, Dm): ", keyroot, 4);
            ask_int("Numerator of timesign: ", &timesign.num);
            ask_int("Denominator of timesign: ", &timesign.den);
            ask_int("Number of Harmonies: ", &numofHarm);
            ask_int("Number of Accompaniment: ", &numofAcco);

            speed = 120.0;
            numofLines = 10;
            Accuracy = 4;

            if (!validate_counts(timesign, speed, numofHarm, numofAcco,
                    numofLines, Accuracy)) {
                printf("非法输入。\n");
                free_basic_buffers(filename, title, author, keyroot);
                return 1;
            }
            /* 默认乐器由 newfileMaker 填充 */
            instruments = (Instrument *)calloc(numofAcco, sizeof(Instrument));
            break;
        }

        case 2: {
            ask_str("Title: ", title, 50);
            ask_str("Author: ", author, 50);
            ask_str("Key root: ", keyroot, 4);
            ask_int("Numerator of timesign: ", &timesign.num);
            ask_int("Denominator of timesign: ", &timesign.den);
            ask_double("Speed (bpm): ", &speed);
            ask_int("Number of Harmonies: ", &numofHarm);
            ask_int("Number of Accompaniment: ", &numofAcco);
            ask_int("Number of Lines: ", &numofLines);
            ask_int("Accuracy: ", &Accuracy);

            if (!validate_counts(timesign, speed, numofHarm, numofAcco,
                    numofLines, Accuracy)) {
                printf("非法输入。\n");
                free_basic_buffers(filename, title, author, keyroot);
                return 1;
            }

            Instrument *instrumentslist =
                (Instrument *)calloc(numofAcco, sizeof(Instrument));
            InstrumentPrototype *prototypes =
                (InstrumentPrototype *)calloc(numofAcco, sizeof(InstrumentPrototype));
            char **names = (char **)calloc(numofAcco, sizeof(char *));
            if (!instrumentslist || !prototypes || !names) {
                printf("内存分配失败。\n");
                free(instrumentslist);
                free(prototypes);
                free(names);
                free_basic_buffers(filename, title, author, keyroot);
                return 1;
            }
            for (int i = 0; i < numofAcco; i++) {
                names[i] = (char *)malloc(50);
                prototypes[i] = searchAndSetInstrument(i, names[i]);
            }
            int built = GetInstrumentsFromPrototypes(instrumentslist,
                    prototypes, numofAcco, names);
            for (int i = 0; i < numofAcco; i++) free(names[i]);
            free(names);
            free(prototypes);
            if (built != numofAcco) {
                printf("乐器初始化失败。\n");
                free_instruments(instrumentslist, numofAcco);
                free_basic_buffers(filename, title, author, keyroot);
                return 1;
            }
            instruments = instrumentslist;
            break;
        }

        default:
            printf("Invalid mode.\n");
            free_basic_buffers(filename, title, author, keyroot);
            return 1;
    }

    if (!instruments) {
        printf("内存分配失败。\n");
        free_basic_buffers(filename, title, author, keyroot);
        return 1;
    }

    newfileMaker(filename, title, author, keyroot,
            timesign, speed, numofHarm, numofAcco,
            numofLines, Accuracy, instruments);

    // 清理
    free_instruments(instruments, numofAcco);
    free_basic_buffers(filename, title, author, keyroot);
    return 0;
}