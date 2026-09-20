#include "tymp.h"


bool char_in_charlist(char tok,char *list, int n){
    for (int i = 0;i < n;n++){
        if (tok == list[i]) return true;
    }
    return false;
}


void strip_chars(char *line,char *chars,int nChars,bool isTwisted,char *endTokens,int nEdTokens){
    int length = strlen(line);
    for (int i = 0;i < nChars;i++){
        char tok = chars[i];
        if (!isTwisted){
            for (int k = 0;k < length;k++){
                if (line[k] == tok){
                    for (int j = k;j < length;j++)
                        line[j] = line[j + 1];   
                    k--;
                    length--;  
                }
                if (!endTokens[0] || !nEdTokens) continue;
                else if (char_in_charlist(line[k],endTokens,nEdTokens)){
                    break;
                }
            }
        }
        else {
            for (int k = length - 1;k >= 0;k++){
                if (line[k] == tok){
                    for (int j = k;j < length;j++)
                        line[j] = line[j + 1];
                    k++;
                    length--;     
                }
                if (!endTokens[0] || !nEdTokens) continue;
                else if (char_in_charlist(line[k],endTokens,nEdTokens)){
                    break;
                } 
            }
        }
    }
}


void strip_crlf(char *line){
    strip_chars(line,(char[2]){'\r','\n'},2,false,(char[1]){'\0'},0);
}

void strip_trailing_spaces(char *line){
    strip_chars(line,(char[1]){' '},1,true,all_non_space_nl,122);
}

int get_total_lines(FILE *fp){
    if (!fp) return -1;
    int lines = 0;
    char *buf = malloc(1028 * sizeof(char));
    while (fgets(buf,1024,fp) != NULL){
        int len = strlen(buf);
        if (buf[len - 1] == '\n') lines++;
        else continue;
    }
    return lines;
}

char** read_all_lines(const char *path, int* lin){
    FILE *fp = fopen(path,"r");
    if (!fp) return;
    int lines = get_total_lines(fp);   
    char **ret = (char **)malloc(lines * sizeof(char*));
    rewind(fp);
    for (int i = 0;i < lines;i++){
        char *buf = (char *)malloc(MAX_READ_NUM * sizeof(char));
        fgets(buf,MAX_NUM,fp);
        int len = strlen(buf);
        ret[i] = strdup(buf);
        lin[i] = i + 1;
    }
    return ret;
}

bool line_is_timestamp(const char *line){
    char *buf = strdup(line);
    strip_chars(buf, (char *){' ',},1,false,all_non_space_nl,122);
    return (!strncmp(buf,"#timestamp:",11));
}



TimeState parse_time_spec(const char *tok){

}

