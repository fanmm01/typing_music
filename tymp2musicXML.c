#include "tymp.h"


/*get a new xml file and write the head*/
FILE * initNewMusicXML(char *filename){
    if (!filename) return NULL;
    size_t len = strlen(filename);
    char* full_filename = (char*)malloc(len + 10);
    if (!full_filename) return NULL;
    if (!(ifStrEndwith(filename,".musicxml") || ifStrEndwith(filename,".xml")))
        sprintf(full_filename,"%s.musicxml",filename);
    else strcpy(full_filename,filename);

    FILE *fp = fopen(full_filename,"w");
    free(full_filename);          /* 文件名缓冲区用完即释放，避免泄漏 */
    if (!fp) return NULL;         /* 打开失败时不可写 xmlHead，直接返回 NULL */
    fputs(xmlHead,fp);
    return fp;
}

/*write part-list from instrument list*/
int makeInstrumentPart();

/*get instrument list according to the file stream*/
int getInstrumentList();


/*get speed。*/

/*to write a note.*/

/*get one of a measure's one part.*/




