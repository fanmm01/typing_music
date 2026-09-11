#ifndef _TYMP_H_
#define _TYMP_H_ 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>

#include "instruments.h"


#define MAX_NUM 10000
#define MAX(A,B) ((A>B)?A:B)
#define MIN(A,B) ((A<B)?A:B)
#define MOD_M(A,B) ((A%B)?(A%B):B)


bool is_all_chinese(const char *str) {
    if (!str || *str == '\0') return false;   // 空串不算中文
    setlocale(LC_ALL, "en_US.UTF-8");         // 设置 UTF-8 locale（也可用 "" 取系统默认）
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    size_t len = strlen(str);
    const char *p = str;
    wchar_t wc;
    int ret;
    while ((ret = mbrtowc(&wc, p, len - (p - str), &state)) > 0) {
        // 判断 Unicode 码点是否在 CJK 基本区 (U+4E00 ~ U+9FFF)
        // 可根据需要添加扩展区：U+3400~U+4DBF, U+20000~U+2A6DF 等
        if (!((wc >= 0x4E00 && wc <= 0x9FFF) ||
              (wc >= 0x3400 && wc <= 0x4DBF) ||
              (wc >= 0x20000 && wc <= 0x2A6DF))) {
            return false;
        }
        p += ret;
    }
    return (ret == 0);   // ret==0 表示正常结束（遇到空字符）
}

bool contains_chinese(const char *str) {
    if (!str || *str == '\0') return false;   // 空串肯定不含中文
    setlocale(LC_ALL, "en_US.UTF-8");         // 确保 UTF-8 locale
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    size_t len = strlen(str);
    const char *p = str;
    wchar_t wc;
    int ret;
    while ((ret = mbrtowc(&wc, p, len - (p - str), &state)) > 0) {
        // 判断是否属于中文 Unicode 范围（可根据需要扩展）
        if ((wc >= 0x4E00 && wc <= 0x9FFF) ||          // CJK 基本区
            (wc >= 0x3400 && wc <= 0x4DBF) ||          // CJK 扩展 A
            (wc >= 0x20000 && wc <= 0x2A6DF) ||        // CJK 扩展 B
            (wc >= 0x2A700 && wc <= 0x2B73F) ||        // CJK 扩展 C
            (wc >= 0x2B740 && wc <= 0x2B81F) ||        // CJK 扩展 D
            (wc >= 0x2B820 && wc <= 0x2CEAF) ||        // CJK 扩展 E
            (wc >= 0xF900 && wc <= 0xFAFF) ||          // CJK 兼容汉字
            (wc >= 0xFE30 && wc <= 0xFE4F)) {          // CJK 兼容形式（部分标点）
            return true;                               // 发现中文，立即返回
        }
        p += ret;
    }
    // 如果循环正常结束（遇到空字符或到达末尾），说明没有中文
    return false;
}


int ifStrEndwith(char* str,char* tok){
    int len = strlen(str);
    int len_tok = strlen(tok);
    if(len==0) return 0;
    if(len_tok==0) return 0;
    if(len<len_tok) return 0;
    for (int i=0;i<len_tok;i++){
        if(str[len-len_tok+i]!=tok[i]) return 0;
    }
    return 1;
}

int ifStrStartwith(char* str,char* tok){
    int len = strlen(str);
    int len_tok = strlen(tok);
    if(len==0) return 0;
    if(len_tok==0) return 0;
    if(len<len_tok) return 0;
    for (int i=0;i<len_tok;i++){
        if(str[i]!=tok[i]) return 0;
    }
    return 1;
}


typedef struct pitch{
    char step;
    int alter;
    int octave;
} pitch;

typedef enum notetype{
    sixteent=16,
    eighth=8,
    quarter=4,
    half=2,
    whole=1,
} notetype;

typedef enum tie{
    start=1,
    end=2,
} tie;

typedef enum syllabic{
    single=0,
    start=1,
    end=2,
    middle=2,
} syllabic;

typedef struct time_modification{
    int actualNotes;
    int normalNotes;
    notetype normalType;
} time_modification;

typedef struct lyric{
    syllabic syllabic;
    char* text;
}lyric;

typedef enum notation_name{
    tied=0,
    tuplet=2,
/*后续随更新增补*/
} notation_name;

/*notations,暂时实现下面几个必要的： 
<notations>
    <tied type="start"/>
    <tuplet type="start" number="1"/>
</notations>
记作：
{notation_type:tied
type:start
attribute2:""}
{notation_type:tuplet
type:end
attribute2:"1"}
*/
typedef struct notations{
    notation_name notation_type;
    tie type;
    char* attribute2;
} notations;

typedef struct note_pitch_mxml{
    bool isChord;
    bool isRest;
    pitch pitch;
    int duration;
    notetype type;
    tie tie[2];
    int voice;
    bool isDot;
    char *accidental;
    notations notations[2];
    lyric lyric;
} note_pitch_mxml;

typedef struct note_tymp{
    int solfa;
    signed char alter; 
    /*'+''=''-',或记为'1''0''+1'.both acceptable.*/
    int octave;
    int dur;
    int pos;
    bool isChord;
    char* lyc;
} note_tymp;



/*
<!--==musicxml head==-->
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC
  "-//Recordare//DTD MusicXML 4.0 Partwise//EN"
  "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="4.0">
*/
const char xmlHead[210] = 
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE score-partwise PUBLIC\n"
"  \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\"\n"
"  \"http://www.musicxml.org/dtds/partwise.dtd\">\n"
"<score-partwise version=\"4.0\">\n"
"\n";



#endif //TYMP_H