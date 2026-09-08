#ifndef INSTRUMENTS_H
#define INSTRUMENTS_H

#include <stdio.h>

/* 乐器类别：决定如何生成 MusicXML 的 midi-instrument */
typedef enum {
    INS_PITCHED   = 0,   /* 有音高：写 midi-program */
    INS_UNPITCHED = 1,   /* 无音高打击单件：写 midi-channel + midi-unpitched，不写 midi-program */
    INS_DRUMKIT   = 2    /* 架子鼓整组：走打击通道，可写 program=1 标准鼓包或不写 */
} InsKind;

typedef struct {
    const char *base_id;     /* 基础id前缀，生成 Vln1/Vln2 用 */
    const char *base_name;   /* 英文基础名，生成 part-name/instrument-name */
    const char *chinese;     /* 中文基础名 */
    int         gm_program;  /* GM 1-128；UNPITCHED/DRUMKIT 可填0表示不写 */
    InsKind     kind;        /* 类别 */
    int         unpitched;   /* 若为单件无音高，填GM鼓键位如军鼓38；否则0 */
} InstrumentPrototype;

static const InstrumentPrototype g_proto[] = {
    /* ---------- 木管 ---------- */
    {"Picc",  "Piccolo",           "短笛",      73, INS_PITCHED, 0},
    {"FL",    "Flute",             "长笛",      74, INS_PITCHED, 0},
    {"OB",    "Oboe",              "双簧管",    69, INS_PITCHED, 0},
    {"EH",    "English Horn",      "英国管",    70, INS_PITCHED, 0},
    {"CL",    "Clarinet in Bb",    "单簧管(Bb)",72, INS_PITCHED, 0}, /* GM不分子调，移调用<transpose> */
    {"CLA",   "Clarinet in A",     "单簧管(A)", 72, INS_PITCHED, 0},
    {"BCL",   "Bass Clarinet",     "低音单簧管",72, INS_PITCHED, 0},
    {"BN",    "Bassoon",           "大管",      71, INS_PITCHED, 0},
    {"CBN",   "Contrabassoon",     "低音大管",  71, INS_PITCHED, 0},
    {"SSAX",  "Soprano Saxophone", "高音萨克斯",65, INS_PITCHED, 0},
    {"ASAX",  "Alto Saxophone",    "中音萨克斯",66, INS_PITCHED, 0},
    {"TSAX",  "Tenor Saxophone",   "次中音萨克斯",67, INS_PITCHED, 0},
    {"BSAX",  "Baritone Saxophone","上低音萨克斯",68, INS_PITCHED, 0},

    /* ---------- 铜管 ---------- */
    {"HN",    "Horn in F",         "圆号(F)",   61, INS_PITCHED, 0},
    {"TPT",   "Trumpet in Bb",     "小号(Bb)",  57, INS_PITCHED, 0},
    {"TPTM",  "Muted Trumpet",     "弱音小号",  60, INS_PITCHED, 0},
    {"TRB",   "Trombone",          "长号",      58, INS_PITCHED, 0},
    {"BTRB",  "Bass Trombone",     "低音长号",  58, INS_PITCHED, 0},
    {"TUBA",  "Tuba",              "大号",      59, INS_PITCHED, 0},

    /* ---------- 有音高打击/键盘打击 ---------- */
    {"TIMP",  "Timpani",           "定音鼓",    48, INS_PITCHED, 0},
    {"XYL",   "Xylophone",         "木琴",      14, INS_PITCHED, 0},
    {"MAR",   "Marimba",           "马林巴",    13, INS_PITCHED, 0},
    {"VIB",   "Vibraphone",        "颤音琴",    12, INS_PITCHED, 0},
    {"GLOCK", "Glockenspiel",      "钟琴",      10, INS_PITCHED, 0},
    {"TUB",   "Tubular Bells",     "管钟",      15, INS_PITCHED, 0},
    {"CEL",   "Celesta",           "钢片琴",     9, INS_PITCHED, 0},
    {"MBOX",  "Music Box",         "八音盒",    11, INS_PITCHED, 0},

    /* ---------- 无音高打击单件（用 unpitched 键位） ---------- */
    {"BD",    "Bass Drum",         "底鼓",       0, INS_UNPITCHED, 36},
    {"SD",    "Snare Drum",        "军鼓",       0, INS_UNPITCHED, 38},
    {"ESD",   "Electric Snare",    "电军鼓",     0, INS_UNPITCHED, 40},
    {"CHH",   "Closed Hi-Hat",     "闭镲",       0, INS_UNPITCHED, 42},
    {"OHH",   "Open Hi-Hat",       "开镲",       0, INS_UNPITCHED, 46},
    {"CRSH",  "Crash Cymbal",      "吊镲1",      0, INS_UNPITCHED, 49},
    {"RIDE",  "Ride Cymbal",       "叮叮镲",     0, INS_UNPITCHED, 51},
    {"TOML",  "Low Tom",           "低通鼓",     0, INS_UNPITCHED, 45},
    {"TOMH",  "High Tom",          "高通鼓",     0, INS_UNPITCHED, 50},
    {"COW",   "Cowbell",           "牛铃",       0, INS_UNPITCHED, 56},
    {"TAMB",  "Tambourine",        "铃鼓",       0, INS_UNPITCHED, 54},
    {"CONGA", "Conga",             "康加鼓",     0, INS_UNPITCHED, 64},
    {"SHAKE", "Shaker",            "沙锤",       0, INS_UNPITCHED, 70},
    {"WD",    "Woodblock",          "木鱼/木块", 0, INS_UNPITCHED, 76},

    /* ---------- 架子鼓整组 ---------- */
    {"DRUMS", "Drum Set",          "架子鼓",     1, INS_DRUMKIT, 0}, /* program=1标准鼓包可写可不写 */

    /* ---------- 弦乐 ---------- */
    {"Vln",   "Violin",            "小提琴",    41, INS_PITCHED, 0},
    {"Vla",   "Viola",             "中提琴",    42, INS_PITCHED, 0},
    {"Vc",    "Violoncello",       "大提琴",    43, INS_PITCHED, 0},
    {"Cb",    "Contrabass",        "低音提琴",  44, INS_PITCHED, 0},
    {"STR1",  "String Ensemble 1", "弦乐合奏1", 49, INS_PITCHED, 0},
    {"STR2",  "String Ensemble 2", "弦乐合奏2", 50, INS_PITCHED, 0},

    /* ---------- 键盘/拨弦 ---------- */
    {"PNO",   "Piano",             "钢琴",       1, INS_PITCHED, 0},
    {"EP1",   "Electric Piano 1",  "电钢琴1",    5, INS_PITCHED, 0},
    {"EP2",   "Electric Piano 2",  "电钢琴2",    6, INS_PITCHED, 0},
    {"HONK",  "Honky-tonk Piano",  "酒吧钢琴",   4, INS_PITCHED, 0},
    {"HARP",  "Harp",              "竖琴",      47, INS_PITCHED, 0},
    {"ORG1",  "Drawbar Organ",     "拉杆风琴",  17, INS_PITCHED, 0},
    {"ORG2",  "Rock Organ",        "摇滚风琴",  19, INS_PITCHED, 0},
    {"ORG3",  "Church Organ",      "管风琴",    20, INS_PITCHED, 0},
    {"HPS",   "Harpsichord",       "羽管键琴",   7, INS_PITCHED, 0},
    {"CLAV",  "Clavinet",          "击弦古钢琴", 8, INS_PITCHED, 0},

    /* ---------- 人声 ---------- */
    {"SOP",   "Soprano",           "女高音",    53, INS_PITCHED, 0},
    {"ALT",   "Alto",              "女中音",    53, INS_PITCHED, 0},
    {"TEN",   "Tenor",             "男高音",    54, INS_PITCHED, 0},
    {"BAR",   "Baritone",          "男中音",    53, INS_PITCHED, 0},
    {"BAS",   "Bass",              "男低音",    55, INS_PITCHED, 0},
    {"CHOIR", "Choir Aahs",        "合唱",      53, INS_PITCHED, 0},
/* ---------- 吉他 ---------- */
    {"NGTR",  "Nylon Guitar",       "古典尼龙吉他",25, INS_PITCHED, 0},
    {"SGTR",  "Steel Guitar",       "民谣钢弦吉他",26, INS_PITCHED, 0},
    {"JGTR",  "Jazz Guitar",        "爵士电吉他", 27, INS_PITCHED, 0},
    {"CGTR",  "Clean Guitar",       "清音电吉他", 28, INS_PITCHED, 0},
    {"MGTR",  "Muted Guitar",       "闷音电吉他", 29, INS_PITCHED, 0},
    {"OGTR",  "Overdrive Guitar",   "过载电吉他", 30, INS_PITCHED, 0},
    {"DGTR",  "Distortion Guitar",  "失真电吉他", 31, INS_PITCHED, 0},
    {"HGTR",  "Guitar Harmonics",   "吉他和声",   32, INS_PITCHED, 0},

    /* ---------- 贝斯 ---------- */
    {"ABASS", "Acoustic Bass",      "原声贝斯",  33, INS_PITCHED, 0},
    {"FBASS", "Fingered Bass",      "电贝斯指弹",34, INS_PITCHED, 0},
    {"PBASS", "Picked Bass",        "电贝斯拨片",35, INS_PITCHED, 0},
    {"FLBASS","Fretless Bass",      "无品贝斯",  36, INS_PITCHED, 0},
    {"S1BASS","Synth Bass 1",       "合成贝斯1", 39, INS_PITCHED, 0},
    {"S2BASS","Synth Bass 2",       "合成贝斯2", 40, INS_PITCHED, 0},

    /* ---------- 流行键盘/其他 ---------- */
    {"ACC",   "Accordion",          "手风琴",    22, INS_PITCHED, 0},
    {"HARM",  "Harmonica",          "口琴",      23, INS_PITCHED, 0},
    {"LEAD1", "Synth Lead Square",  "合成主音方波",81, INS_PITCHED, 0},
    {"LEAD2", "Synth Lead Saw",     "合成主音锯齿",82, INS_PITCHED, 0},
    {"PAD1",  "New Age Pad",        "合成垫新世纪",89, INS_PITCHED, 0},
    {"PAD2",  "Warm Pad",           "合成垫暖音", 90, INS_PITCHED, 0},
    {"BRASS", "Brass Section",      "铜管合奏",  62, INS_PITCHED, 0},
    {"SBR1",  "Synth Brass 1",      "合成铜管1", 63, INS_PITCHED, 0},

    /* 终止 */
    {NULL, NULL, NULL, 0, INS_PITCHED, 0}
};


#endif // INSTRUMENTS_H