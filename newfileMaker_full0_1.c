#include <stdio.h>
#include <stdlib.h>
#include <string.h> 



#define MAX_NUM 10000

//拍号结构
typedef struct timesign{
    int num;//分子
    int den;//分母
} timesign;

/* 
初始化一个新的line，为其添加足够数量的空格备用。
*/

int write_linehead(FILE *fp,char *tok,int chsBef){
    fprintf(fp,"%s",tok);
    for (int i = 0;i < chsBef - strlen(tok) ;i++){
        fprintf(fp," ");
    }
    fprintf(fp,"||");
}

int write_blankline(FILE *fp, int n, char *tok,int chsBef){
    write_linehead(fp,tok,chsBef);
    for (int i = 0;i < MAX_NUM;i++){
        fprintf(fp," ");
    }
}

/*时间戳提示。*/
int write_timestp(FILE *fp, int timesign,int chaBef, int Accuracy){
    write_linehead(fp,"",19);

}

/*
为格式化乐谱文件（一般指：在尾端）添加n个新的、非首行的空乐谱行。
*/
int newline_nonFirst(FILE *fp,
        int n, timesign timesign, 
        int numofHarm, int numofAcco, int Accuracy){
    for(int i = 0;i < n;i++){
        for (int j = 0;j < 19;j++){
            fprintf(fp," ");
        }
        fprintf(fp,"||");

    }
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
*/
int newfileMaker(char* filename, char* keyroot,
        timesign timesign, double speed, 
        int numofHarm, int numofAcco,
        int numofLines,int Accuracy){
    FILE *fp;
    fp = fopen(filename, 'w');

}
