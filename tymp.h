#ifndef _TYMP_H_
#define _TYMP_H_ 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "instruments.h"

#define MAX_NUM 10000
#define MAX_READ_NUM 102400

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

typedef enum mxml_dynamic {          /* 8 级 + 4 级兼容 */
    DYN_PPPP,
    DYN_PPP,
    DYN_PP, 
    DYN_P, 
    DYN_MP, 
    DYN_MF, 
    DYN_F, 
    DYN_FF,
    DYN_FFF,
    DYN_FFFF,
} mxml_dynamic;

typedef enum syllabic{
    SYL_SINGLE=0,
    SYL_BEGIN=1,
    SYL_END=2,
    SYL_MIDDLE=3,
} syllabic;

typedef enum DirectionKind {
    DIR_TEMPO=0,
    DIR_DYNAMIC=1,
    DIR_WORDS=2,
    DIR_WEDGE=3,
    DIR_FERMATA=4,
} DirectionKind;


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

typedef struct direction_mxml {
    DirectionKind kind;
    double tempo;        /* DIR_TEMPO:|vb120 → metronome + <sound tempo>;DIR_WORDS 回原速时同用 */
    char  *words;        /* DIR_WORDS:"rit." / "accel." / "A tempo" */
    int    dynamic;      /* DIR_DYNAMIC:mxml_dynamic */
    int    wedge;        /* DIR_WEDGE:0 无;1 crescendo start;-1 stop;2 diminuendo start;-2 stop */
    int    col;          /* 小节内起点列(排序键;不输出 <offset>) */
    char   placement;    /* 0 = 默认;'b' = placement="below" */
    struct direction_mxml *next;
} direction_mxml;

/* ---- 新增:harmony(rule_0_3 #7 和弦行) ---- */
typedef struct harmony_mxml {
    char root;           /* A~G */
    int  root_alter;     /* -1..1 */
    char kind[16];       /* major/minor/major-seventh/dominant/diminished/other */
    int  has_bass;
    char bass_step;      /* <bass-step> */
    int  bass_alter;
    int  col;            /* 小节内起点列(排序键) */
    struct harmony_mxml *next;
} harmony_mxml;


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

// 除去" ""\r""\n""\t""\v"外的全部的ASCII字符（0-127）
/* static:头文件中的定义,供多编译单元共同包含而不产生重复符号 */
static const unsigned char all_non_space_nl[] = {
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x0C,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,
    0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,
    0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,
    0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
    0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
    0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
};

// 数组长度：126
static const size_t all_len = sizeof(all_non_space_nl);


/* tymp_internal.h 依赖本文件上方定义的全部类型(time_modification / direction_mxml /
   harmony_mxml / notetype 等),故必须在本文件末尾包含,形成单向依赖。 */
#include "tymp_internal.h"

#endif //TYMP_H