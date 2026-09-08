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



#endif