#ifndef _TYMP_H_
#define _TYMP_H_ 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "instruments.h"


#define MAX_NUM 10000
#define MAX(A,B) ((A>B)?A:B)
#define MIN(A,B) ((A<B)?A:B)
#define MOD_M(A,B) ((A%B)?(A%B):B)


/* UTF-8 序列长度；非法首字节返回 0 */
static inline int utf8_seq_len(const unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 0;
}

/* 解码一个 UTF-8 码点；len 来自 utf8_seq_len */
static inline unsigned decode_utf8(const unsigned char *p, int len) {
    switch (len) {
        case 1: return p[0];
        case 2: return ((unsigned)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        case 3: return ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6)
                     | (p[2] & 0x3F);
        case 4: return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3F) << 12)
                     | ((unsigned)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        default: return 0xFFFFFFFFu;   /* 非法 */
    }
}

/* 码点是否属于 CJK 汉字范围 */
static inline bool cp_is_cjk(unsigned cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF)      /* CJK 基本区 */
        || (cp >= 0x3400 && cp <= 0x4DBF)      /* CJK 扩展 A */
        || (cp >= 0x20000 && cp <= 0x2A6DF)    /* CJK 扩展 B */
        || (cp >= 0x2A700 && cp <= 0x2B73F)    /* CJK 扩展 C */
        || (cp >= 0x2B740 && cp <= 0x2B81F)    /* CJK 扩展 D */
        || (cp >= 0x2B820 && cp <= 0x2CEAF)    /* CJK 扩展 E */
        || (cp >= 0xF900 && cp <= 0xFAFF)      /* CJK 兼容汉字 */
        || (cp >= 0xFE30 && cp <= 0xFE4F);     /* CJK 兼容形式 */
}

static inline bool is_all_chinese(const char *str) {
    if (!str || *str == '\0') return false;   // 空串不算中文
    const unsigned char *p = (const unsigned char *)str;
    while (*p) {
        int len = utf8_seq_len(*p);
        if (len == 0) return false;           // 非法字节序列
        if (!cp_is_cjk(decode_utf8(p, len))) return false;
        p += len;
    }
    return true;
}

static inline bool contains_chinese(const char *str) {
    if (!str || *str == '\0') return false;   // 空串肯定不含中文
    const unsigned char *p = (const unsigned char *)str;
    while (*p) {
        int len = utf8_seq_len(*p);
        if (len == 0) return false;
        if (cp_is_cjk(decode_utf8(p, len))) return true;
        p += len;
    }
    return false;
}


static inline int ifStrEndwith(char* str,char* tok){
    if(!str||!tok) return 0;
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

static inline int ifStrStartwith(char* str,char* tok){
    if(!str||!tok) return 0;
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
    TIE_START=1,
    TIE_END=2,
} tie;

typedef enum syllabic{
    SYL_SINGLE=0,
    SYL_BEGIN=1,
    SYL_END=2,
    SYL_MIDDLE=3,
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



/* 由 tymp2musicXML.c 实现：新建 .musicxml 文件并写入 xmlHead，失败返回 NULL */
FILE * initNewMusicXML(char *filename);

/*
<!--==musicxml head==-->
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC
  "-//Recordare//DTD MusicXML 4.0 Partwise//EN"
  "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="4.0">
*/
static const char xmlHead[210] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE score-partwise PUBLIC\n"
"  \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\"\n"
"  \"http://www.musicxml.org/dtds/partwise.dtd\">\n"
"<score-partwise version=\"4.0\">\n"
"\n";



#endif //TYMP_H