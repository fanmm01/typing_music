#include <stdio.h>
#include <stdlib.h>
#include <string.h> 



#define MAX_NUM 10000
#define MAX(A,B) ((A>B)?A:B)
#define MIN(A,B) ((A<B)?A:B)
#define MOD_M(A,B) ((A%B)?(A%B):B)

//拍号结构
typedef struct timesign{
    int num;//分子
    int den;//分母
} timesign;

const char gt[6][3] = {"E2","A2","D3","G3","B3","E3"};
//char *drm[];


/* 初始化一个line的行首。*/
int write_linehead(FILE *fp,char *tok,int chsBef){
    fprintf(fp,"%-*.*s",chsBef,chsBef,tok);
    fprintf(fp,"||");
    return 0;
}


/*初始化一个新的空line，为其添加足够数量的空格备用。*/
int write_blankline(FILE *fp, int n, char *tok,int chaBef){
    write_linehead(fp,tok,chaBef);
    for (int i = 0;i < MAX_NUM;i++){
        fprintf(fp," ");
    }
    return 0;
}


/*获得相对意义上的精度，即（与输入的精度不同地，“精确到几分之一拍”。*/
int inline get_related_accurcy(timesign time, int Accuracy){
    int relaccu;
    switch (Accuracy){
        case 1:
        case 2:
        case 3:
        case 4:
            relaccu = Accuracy;
            break;
        case 8:
        case 16:
        case 32:
            int tok = time.den;
            relaccu = Accuracy / tok;
            break;
        default:
            relaccu = 4;
            break;
    }
    return relaccu;
}

/*时间戳提示。*/
int write_timestp(FILE *fp, timesign time,
    int chaBef, int Accuracy){
    write_linehead(fp,"#timestamp:",chaBef);
    int accu = get_related_accurcy(time,Accuracy);
    for (int i = 0;(i * accu * (time.num)) < MAX_NUM;i++){
        for (int j = 0;j < (time.num);j++){
            fprintf(fp,"%c",((j%time.num==0)?'¡':'.'));
            for (int k = 0; k < accu - 1;k++){
                fprintf(fp," ");
            }
        }
    }
    return 0;
}

/*为格式化乐谱文件（一般指：在尾端）添加n个新的、非首行的空乐谱行。*/
int newline_nonFirst(FILE *fp,
        int n, timesign time, int chaBef,
        int numofHarm, int numofAcco, 
        int Accuracy,char** instruments){
    for (int i = 0;i < n;i++){
        fprintf(fp,"\n");
        write_timestp(fp,time,chaBef,Accuracy);
        write_blankline(fp,1,"{ ",chaBef);
        write_blankline(fp,1,"chords:",chaBef);
        write_blankline(fp,2,"",chaBef);
        for (int j = 0;j < numofHarm;j++){
            write_blankline(fp,1,"[",chaBef);
            write_blankline(fp,1,"",chaBef);
        }
        write_timestp(fp,time,chaBef,Accuracy);
        for (int j = 0;j < numofAcco;j++){
            char* inst = instruments[j];
            char* headtok = (char*)malloc(40*sizeof(char));
            sprintf(headtok,"(-%s-",inst);
            write_blankline(fp,1,headtok,chaBef);
            write_blankline(fp,1,"",chaBef);  
            if ((j != 0) && (j%5==0)) write_timestp(fp,time,chaBef,Accuracy);      
        }   
        write_blankline(fp,1,"}",chaBef); 
    }
    return 0;
}

//提供吉他六弦空把位音调。
inline char* getGT(){
    char* gttok = (char*)malloc(100*sizeof(char));
    sprintf(gttok,"&gt: {a=%s,b=%s,c=%s,d=%s,e=%s,f=%s}",gt[0],gt[1],gt[2],gt[3],gt[4],gt[5]);
    return gttok;
}

//建立一个新的首行。chaBef一般设为45较为合适。
int new_firstLine(FILE* fp,char* keyroot,
        timesign time, double speed, 
        int numofHarm, int numofAcco,
        int numofLines,int Accuracy,
        int chaBef,char **instruments){
    char* firstline_tok = (char*)malloc(100*sizeof(char));
    char* gttok = (char*)malloc(100*sizeof(char));
    gttok = getGT();
    sprintf(firstline_tok,"{ |%d/%d |=%s |vb%lf %s}",
        time.num,time.den,keyroot,speed,gttok);
    int fltokN = strlen(firstline_tok);
    int flChaBef = fltokN + (4 - (fltokN + 1) % 4);
    int RealChaBef = MAX(flChaBef,chaBef);
    write_timestp(fp,time,RealChaBef,Accuracy);
    write_blankline(fp,1,firstline_tok,RealChaBef);
    write_blankline(fp,1,"chords,eg.C,Am,Bmin7:",RealChaBef);
    write_blankline(fp,1,"/*LV.pitch range of 1st note:*/ *3",RealChaBef);
    write_blankline(fp,1,"/*lirics*/",RealChaBef);
    for (int j = 0;j < numofHarm;j++){
        write_blankline(fp,1,"[/*Harmony*/",RealChaBef);
        write_blankline(fp,1,"/*lyrics*/",RealChaBef);
    }
    write_timestp(fp,time,RealChaBef,Accuracy);
    for (int j = 0;j < numofAcco;j++){
        char* inst = instruments[j];
        char* headtok = (char*)malloc(40*sizeof(char));
        sprintf(headtok,"(Accomponiment#1:%s",inst);
        write_blankline(fp,1,headtok,chaBef);
        write_blankline(fp,1,"",chaBef);  
        if ((j != 0) && (j%5==0)) write_timestp(fp,time,chaBef,Accuracy);      
    }   
    write_blankline(fp,1,"}",chaBef); 
    return 0;
}

/*
初始化一个新的空格式化乐谱文件的方法。
乐谱文件的格式见我们在rule.txt所做的约定。
参数注释：
filename:文件名。
keyroot:调号。如1=G，则keyroot="G".
timesign:拍号。是一个结构体，两个参数分别为分子与分母。
speed:速度。每分钟x拍。
numofHarm:和声声部数量。
numofAcco:伴奏声部数量。
numofLines:初始时总谱行数。
Accuracy：精度。2：精确到1/2拍。3：精确到1/3拍（三连音）。4：精确到1/4拍。8/16/32：精确到8/16/32分音符。
instruments: 乐器表,最后一个元素必须是NULL。指的是，各伴奏声部从上到下分别对应的乐器名称。支持中文或英文或英文简写。
*/
int newfileMaker(char* filename, char* title,char* author,char* keyroot,
        timesign timesign, double speed, 
        int numofHarm, int numofAcco,
        int numofLines,int Accuracy,char **instruments){
    FILE *fp;
    fp = fopen(filename, "w");
    fprintf(fp,"%s by %s\ninstruments:", title,author);
    for (int i = 0;i < numofAcco;i++){
        if (instruments[i]) continue;
        else sprintf((instruments[i]),"instrument#%d",i);
    }
    instruments[numofAcco] = NULL;
    for (int i = 0;instruments[i]!=NULL;i++){
        fprintf(fp,instruments[i]);
    }
    new_firstLine(fp,keyroot,timesign,speed,
        numofHarm,numofAcco,numofLines,
        Accuracy,40,instruments);
    newline_nonFirst(fp,numofLines,timesign,20,
        numofHarm,numofAcco,Accuracy,instruments);
    fclose(fp);
}

int main(){
    char *filename = (char *)malloc(50),
    *title = (char *)malloc(50),
    *author = (char *)malloc(50),
    *keyroot = (char *)malloc(3);
    timesign timesign;
    double speed;
    int numofHarm,numofAcco,numofLines,Accuracy;
    char ** instruments;
    int mode;
    scanf("mode(0:按照默认设置快速配置除文件名,拍号,和声和伴奏声部数量外的全部内容；1:自行输入标题作者与调号拍号，仅默认设置乐器名待自行更换；2：全量创建时自行输入): %d",mode);
    scanf("filename: %s", filename);

    switch (mode)
    {
    case 0:
        strcpy(title, "song");
        strcpy(author, "author");
        strcpy(keyroot, "C");
        scanf("Numerator of timesign:%d", &(timesign.num));
        scanf("Denominator:%d", &(timesign.den));
        speed = 120.0;
        numofHarm = 2;
        numofAcco = 5;
        numofLines = 10;
        Accuracy = 4;
        instruments = (char**)malloc((numofAcco+1) * sizeof(char*));
        for (int i = 0;i < numofAcco;i++){
            sprintf(instruments[i],"Inst.#%d", i);
        }
        instruments[numofAcco] = NULL;
        break;
    case 1: {
            // 自行输入标题、作者、调号、拍号、和声/伴奏数量
            
            printf("Title: ");
            scanf("%49s", title);
            printf("Author: ");
            scanf("%49s", author);
            printf("Key root (e.g., C, Dm): ");
            scanf("%2s", keyroot);
            printf("Numerator of timesign: ");
            scanf("%d", &timesign.num);
            printf("Denominator of timesign: ");
            scanf("%d", &timesign.den);
            printf("Number of Harmonies: ");
            scanf("%d", &numofHarm);
            printf("Number of Accompaniment: ");
            scanf("%d", &numofAcco);

            // 其余参数默认
            speed = 120.0;
            numofLines = 10;
            Accuracy = 4;

            // 分配乐器名并设默认值（等待用户自行更换）
            instruments = (char**)malloc((numofAcco+1) * sizeof(char*));
            for (int i = 0; i < numofAcco; i++) {
                instruments[i] = (char*)malloc(20);
                sprintf(instruments[i], "Inst.#%d", i);
            }
            break;
        }
        case 2: {
            // 全量自行输入
            printf("Title: ");
            scanf("%49s", title);
            printf("Author: ");
            scanf("%49s", author);
            printf("Key root: ");
            scanf("%2s", keyroot);
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
            printf("Accuracy: ");
            scanf("%d", &Accuracy);

            // 逐个输入乐器名
            instruments = (char**)malloc((numofAcco+1) * sizeof(char*));
            for (int i = 0; i < numofAcco; i++) {
                instruments[i] = (char*)malloc(50);  // 预留更长空间
                printf("Instrument #%d name: ", i);
                scanf("%49s", instruments[i]);
            }
            break;
        }
        default:
            printf("Invalid mode.\n");
            return 1;
    }
    newfileMaker(filename,title,author,keyroot,
        timesign,speed,numofHarm,numofAcco,
        numofLines,Accuracy,instruments);
}
