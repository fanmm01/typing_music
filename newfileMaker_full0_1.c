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
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    for (size_t i = len; i > 0; --i) {
        if (src[i - 1] == sep) {
            strncpy(dir_buf, src, i - 1);
            dir_buf[i - 1] = '\0';
            return;
        }
    }
    strcpy(dir_buf, ".");
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
    write_linehead(fp, "#timestamp:", chaBef);
    int accu = get_related_accurcy(time, Accuracy);
    if (time.num * accu == 0) return;
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
        int Accuracy, char **instruments) {
    for (int i = 0; i < n; i++) {
        fprintf(fp, "\n");
        write_timestp(fp, time, chaBef, Accuracy);
        write_blankline(fp, 1, "{ ", chaBef);
        write_blankline(fp, 1, "chords:", chaBef);
        write_blankline(fp, 2, "", chaBef);
        for (int j = 0; j < numofHarm; j++) {
            write_blankline(fp, 1, "[", chaBef);
            write_blankline(fp, 1, "", chaBef);
        }
        write_timestp(fp, time, chaBef, Accuracy);
        for (int j = 0; j < numofAcco; j++) {
            char *inst = instruments[j];
            char *headtok = (char *)malloc(40 * sizeof(char));
            sprintf(headtok, "(-%s-", inst);
            write_blankline(fp, 1, headtok, chaBef);
            write_blankline(fp, 1, "", chaBef);
            free(headtok);  // 释放，避免内存泄漏
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
        int chaBef, char **instruments) {
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
    write_blankline(fp, 1, "/*LV.pitch range of 1st note:*/ *3", RealChaBef);
    write_blankline(fp, 1, "/*lirics*/", RealChaBef);
    for (int j = 0; j < numofHarm; j++) {
        write_blankline(fp, 1, "[/*Harmony*/", RealChaBef);
        write_blankline(fp, 1, "/*lyrics*/", RealChaBef);
    }
    write_timestp(fp, time, RealChaBef, Accuracy);
    for (int j = 0; j < numofAcco; j++) {
        char *inst = instruments[j];
        char *headtok = (char *)malloc(80 * sizeof(char));
        if (j == 0)sprintf(headtok, "(Accomponiment#%d:%s; Initial pitch range: *4",j,inst);
        else sprintf(headtok, "(Accomponiment#%d:%s;                      *4",j,inst);
        write_blankline(fp, 1, headtok, RealChaBef);
        write_blankline(fp, 1, "", RealChaBef);
        free(headtok);  // 释放
        if ((j != 0) && (j % 5 == 0)) write_timestp(fp, time, RealChaBef, Accuracy);
    }
    write_blankline(fp, 1, "}", RealChaBef);

    free(firstline_tok);
    free(gttok);  // 释放 getGT 分配的内存
    return 0;
}


int findInstruments(InstrumentPrototype* insts,char* inputInst){
    int j = 0;
    int FOUND = 0;
    for (int i = 0;i < NUM_OF_INSTRUMENT_GROUPS;i++){
         if (!strcmp(inputInst, InstrumentGroupList[i].chinese) ||
            !strcmp(inputInst, InstrumentGroupList[i].group_name)) {
            for (int k = InstrumentGroupList[i].start_index;k <= InstrumentGroupList[i].end_index;k++){
                insts[j++] = InstrumentPrototypeList[k];
                FOUND = 1;
                break;
            }    
        }
    }
    if (!FOUND)for (int i = 0;i < NUM_OF_INSTRUMENT_PROTS - 1&& j < 15;i++){
        if (!strcmp(inputInst, InstrumentPrototypeList[i].base_id) ||
                !strcmp(inputInst, InstrumentPrototypeList[i].base_name) ||
                !strcmp(inputInst, InstrumentPrototypeList[i].chinese)) {
            for (int i = 0;i < 16;i++) insts[i] = InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS];
            insts[0] = InstrumentPrototypeList[i];
            j = 1;
            break;
        } else
        if (strstr(inputInst,InstrumentPrototypeList[i].base_id)
                || strstr(inputInst,InstrumentPrototypeList[i].base_name)
                ||strstr(inputInst,InstrumentPrototypeList[i].chinese) ||
                strstr(InstrumentPrototypeList[i].base_id, inputInst)
                || strstr(InstrumentPrototypeList[i].base_name, inputInst)
                || strstr(InstrumentPrototypeList[i].chinese, inputInst))
            insts[j++] = InstrumentPrototypeList[i];
    }
    insts[j] = InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS];

    if (j != 0) return j;
    else{
        for (int i = 0;i < 16;i++) insts[i] = InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS];
        insts[0] = InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS-1];
        return 0;
    }
}

InstrumentPrototype searchAndSetInstrument(int index,char* instrumentname){
    char *instrument = (char *)malloc(50);
    char inputInstrument[50]; 
    printf("Instrument #%d name: ",index);
    scanf("%49s", inputInstrument);
    InstrumentPrototype options[16];
    int n = findInstruments(options,inputInstrument);
    if (n > 1){
        printf("Insert number to choose(-1 to self-define)\n");
        int j = 0;
        for (int i = 0;i < n;i++){
            if (options[i].base_id != NULL)
                printf("%d,%s %s %s %s\n",
                    i,
                    options[i].base_id,
                    options[i].base_name,
                    options[i].chinese,
                    options[i].gm_program);
            else break;
        }
        scanf("%d",&j);
        if (j >= 0 && j < n){
            instrumentname = strdup(options[j].chinese);
            return options[j];
        }
        else if (j == -1){
            printf("input the name or 'I' for depending on system:");
            char buf[50];
            scanf("%s",buf);
            instrumentname = strdup(buf);
            return InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS-1];
        }
    } else if (n == 1 && (strcmp((options[0].base_id),"INST"))) 
        return options[0];
    else {
        printf("Input the name or 'I' for depending on system:");
        char buf[50];
        scanf("%s",buf);
        instrumentname = strdup(buf);
        return InstrumentPrototypeList[NUM_OF_INSTRUMENT_PROTS-1];
    };
}

Instrument GetInstrumentFromPrototypeAndIndex(InstrumentPrototype prototype,int index, char* name){
    Instrument instrument;
    bool isChineseName = contains_chinese(name); 
    if (prototype.base_id == NULL) return (Instrument){0};
    else {
        sprintf(instrument.score_part_id,"%s%d",prototype.base_id,index);
        sprintf(instrument.instrument_id,"%s%d-I1",prototype.base_id,index);
        if (strcmp(prototype.base_id, "INST") == 0){
            if (isChineseName){
                sprintf(instrument.part_name_En,"%s#%d",prototype.base_name,index);
                sprintf(instrument.part_name_Zh,"%s",name);
            } else{
                sprintf(instrument.part_name_En,"%s",name);
                sprintf(instrument.part_name_Zh,"%s#%d",prototype.chinese,index);
            } 
        }
        else {
            strcpy(instrument.part_name_En,prototype.base_name);
            strcpy(instrument.part_name_Zh,prototype.chinese);
        }
        instrument.prototype = prototype;  
    }
}


int findIfContextInAppeartimeList(char *context, AppearTime *Appts){
    int n = 0;
    for (int i = 0;;i++){
        if (!strcmp(context,Appts[i].name)) return i;
        if (!(Appts[i].name)) return i;
    }
}

int GetInstrumentsFromPrototypes(Instrument *instrumentlist, InstrumentPrototype *proptypeList,int NumOfProplist, char** names){
    AppearTime *apprTimeList = calloc(NumOfProplist, sizeof(AppearTime));
    int n = 0;
    for (int i = 0;i < NumOfProplist;i++){
        int dex = findContextInAppeartimeList(proptypeList[i].base_name,apprTimeList);
        apprTimeList[dex].name = names[i];
        apprTimeList[dex].time++;
        apprTimeList[dex].prototype = proptypeList[i];
        if (dex==n) n++;
    }
    for (int k = 0;k < n;k++){
        for (int j = 0;j < apprTimeList[k].time;j++){
           instrumentlist[k] = GetInstrumentFromPrototypeAndIndex(apprTimeList[k].prototype,j,apprTimeList[k].name);
        }
    }
    free (apprTimeList);
    return n;
}

/*
初始化一个新的空格式化乐谱文件。
instruments: 乐器表，最后一个元素必须是 NULL。
*/
int newfileMaker(char *filename, char *title, char *author, char *keyroot,
        timesign timesign, double speed,
        int numofHarm, int numofAcco,
        int numofLines, int Accuracy, char **instruments) {

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

    // 补全缺失的乐器名，并确保末尾 NULL
    for (int i = 0; i < numofAcco; i++) {
        if (!instruments[i]) {
            instruments[i] = (char *)malloc(20 * sizeof(char));
            sprintf(instruments[i], "instrument#%d", i);
        }
    }
    instruments[numofAcco] = NULL;

    for (int i = 0; instruments[i] != NULL; i++) {
        fprintf(fp, "%s ", instruments[i]);  // 加 %s 防格式化崩溃
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

// 安全释放 instruments
void free_instruments(char **instruments, int numofAcco) {
    if (!instruments) return;
    for (int i = 0; i < numofAcco; i++) {
        if (instruments[i]) free(instruments[i]);
    }
    free(instruments);
}

int main() {
    char *filename = (char *)malloc(50);
    char *title = (char *)malloc(50);
    char *author = (char *)malloc(50);
    char *keyroot = (char *)malloc(4);  // 放大到 4 字节更安全
    timesign timesign;
    double speed;
    int numofHarm, numofAcco, numofLines, Accuracy;
    char **instruments = NULL;
    int mode;

    printf("mode(0:快速配置; 1:部分手动; 2:全手动): ");
    scanf("%d", &mode);
    printf("filename: ");
    scanf("%49s", filename);

    switch (mode) {
        case 0:
            strcpy(title, "song");
            strcpy(author, "author");
            strcpy(keyroot, "C");
            printf("Numerator of timesign: ");
            scanf("%d", &timesign.num);
            printf("Denominator: ");
            scanf("%d", &timesign.den);
            speed = 120.0;
            numofHarm = 2;
            numofAcco = 5;
            numofLines = 10;
            Accuracy = 4;

            instruments = (char **)malloc((numofAcco + 1) * sizeof(char *));
            for (int i = 0; i < numofAcco; i++) {
                instruments[i] = (char *)malloc(20);
                sprintf(instruments[i], "Inst.#%d", i);
            }
            instruments[numofAcco] = NULL;
            break;

        case 1: {
            printf("Title: ");
            scanf("%49s", title);
            printf("Author: ");
            scanf("%49s", author);
            printf("Key root (e.g., C, Dm): ");
            scanf("%3s", keyroot);
            printf("Numerator of timesign: ");
            scanf("%d", &timesign.num);
            printf("Denominator of timesign: ");
            scanf("%d", &timesign.den);
            printf("Number of Harmonies: ");
            scanf("%d", &numofHarm);
            printf("Number of Accompaniment: ");
            scanf("%d", &numofAcco);

            speed = 120.0;
            numofLines = 10;
            Accuracy = 4;

            instruments = (char **)malloc((numofAcco + 1) * sizeof(char *));
            for (int i = 0; i < numofAcco; i++) {
                instruments[i] = (char *)malloc(20);
                sprintf(instruments[i], "Inst.#%d", i);
            }
            instruments[numofAcco] = NULL;
            break;
        }

        case 2: {
            printf("Title: ");
            scanf("%49s", title);
            printf("Author: ");
            scanf("%49s", author);
            printf("Key root: ");
            scanf("%3s", keyroot);
            printf("Numerator of timesign: ");
            scanf("%d", &timesign.num);
            printf("Denominator of timesign: ");
            scanf("%d", &timesign.den);
            printf("Speed (bpm): ");
            scanf("%lf", &speed);
            printf("Number of Harmonies: ");
            scanf("%d", &numofHarm);
            printf("Number of Accompaniment: ");
            scanf("%d", &numofAcco);
            printf("Number of Lines: ");
            scanf("%d", &numofLines);
            printf("Accuracy: ");;
            scanf("%d", &Accuracy);
            Instrument instrumentslist[numofAcco+1];
                char** instrumentNames = (char**)malloc(50*30*sizeof(char*));
                InstrumentPrototype prototypes[numofAcco+1];
            for(int i = 0;i < numofAcco;i++){
                instrumentNames[i] = (char*)malloc(50*sizeof(char));
                prototypes[i] = searchAndSetInstrument(i,instrumentNames[i]);
            }
            GetInstrumentsFromPrototypes(instrumentslist,prototypes,numofAcco,instrumentNames);
            instruments[numofAcco] = NULL;
            break;
        }
        

        default:
            printf("Invalid mode.\n");
            return 1;
    }

    newfileMaker(filename, title, author, keyroot,
            timesign, speed, numofHarm, numofAcco,
            numofLines, Accuracy, instruments);

    // 清理
    free_instruments(instruments, numofAcco);
    free(filename);
    free(title);
    free(author);
    free(keyroot);
    return 0;
}