#include "tymp.h"



FILE * initNewMusicXML(char *filename){
    char* full_filename = (char*)malloc(10 + strlen(filename));
    if (!(ifStrEndwith(filename,".musicxml") || ifStrEndwith(filename,".xml")))
        sprintf(full_filename,"%s.musicxml",filename);
    else strcpy(full_filename,filename);

    FILE *fp = fopen(full_filename,"w");
    fprintf(fp,xmlHead);
    return fp;
}