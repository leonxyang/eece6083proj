#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "lex.h"

//macros to identify letter digtis and special char
#define ISLETTER(c) (lettertable[c]? 1 : 0)
#define ISDIGIT(c) (digittable[c]? 1 : 0)
#define ISSPCHAR(c) (spchartable[c]? 1 : 0)



//forward declarations
//static inline bool ISLETTER(char);
//static inline bool ISDIGIT(char);
//static inline bool ISSPCHAR(char);
void initLetterTable(void);
void initDigitTable(void);
void initSpCharTable(void);
unsigned long djb2hash (char *);
void initKeywordHash(void);
void initLex(TokenType *);
int readFile(const char*);
void closeFile(void);
void readBuffer(void);
void putBack(void);
int skipLayoutNComment(void);
void reportError(const char*);
int getToken(TokenType *);
void printToken(TokenType *);

//positions
static int linenumber = 0;
static int linepos = 0;    //char position in a line
static int linestart = 0;    //current line start position
static int prelinestart = 0;    //previous line start position
static int pos = 0;    //buffer position

//input file buffer
static char *buffer;

//working variable
static char c;
static char upchar;

//working tables //can be compressed
static int lettertable[128] = {0};
static int digittable[128] = {0};
static int spchartable[128] = {0};
static char keyword[19][12] = {"PROGRAM", "IS", "BEGIN", "END", "GLOBAL", "PROCEDURE", "VARIABLE", "INTEGER", "FLOAT", "STRING", "BOOL",  "IF", "THEN", "ELSE", "FOR", "RETURN", "NOT","TRUE", "FALSE"};
static unsigned long keywordhash[19] = {0};
static int strbuffersize = 128;    //start with 128
static int strbufferpos = 0;
char *strbuffer = 0;    //buffer to store the strings

//error messages
//static const char *slhcommenterror = "slash comment format error\n";
static const char *blkcommenterror = "block comment format error\n";
static const char *putbackwarning = "putback reached the end of previous line\n";
static const char *lexerror = "lex error\n";
static const char *tokenstrerror = "insufficient space in tokenstring";

//lex state
typedef enum{
    ST_BEGIN, ST_ID, ST_NUM, ST_SPCHAR, ST_STRING, ST_EQUAL, ST_LESS, ST_GREATER, ST_NOTEQUAL, ST_COLON, ST_END
} LexState_e;

/*
static inline bool ISLETTER(char c){
    int i = (int)(c);
    return lettertable[i] ? 1 : 0;
}

static inline bool ISDIGIT(char c){
    int i = (int)(c);
    return digittable[i] ? 1 : 0;
}

static inline bool ISSPCHAR(char c){
    int i = (int)(c);
    return spchartable[i] ? 1 : 0;
}
*/

void initLetterTable(void){    //ASCII code
    int i;
    for (i = 65; i < 91; i++)
    {
        lettertable[i] = 1;
        lettertable[i + 32] = 1;
    }
}

void initDigitTable(void){
    int i;
    for (i = 48; i < 58; i++){
        digittable[i] = 1;
    }
}

void initSpCharTable(void){
    int i;
    spchartable[33] = 1;    //!
    spchartable[34] = 1;    //"
    spchartable[38] = 1;    //&
    for (i = 40; i < 48; i++){    //()*+,-./
        spchartable[i] = 1;
    }
    for (i = 58; i < 63; i++){    //:;<=>
        spchartable[i] = 1;
    }
    spchartable[91] = 1;    //[
    spchartable[93] = 1;    //]
    //spchartable[94] = 1;    //^
    spchartable[124] = 1;   //|
}

unsigned long djb2hash (char * str){    //djb2 hash
    unsigned long hash = 5381;
    int c;

    while (c = (*str)){
        hash = ((hash << 5) + hash) + c;    //hash * 33 + c
        str += 1;
    }
    
    return hash;
}

void initKeywordHash(void){    //use djb2 hash
    int i;
    for (i = 0; i < 19; i++){
        keywordhash[i] = djb2hash(*(keyword + i));
    }
}

void initLex(TokenType *token){
    initLetterTable();
    initDigitTable();
    initSpCharTable();
    initKeywordHash();
    memset(token->tokenstring, 0, TOKENBUFFERSIZE);
}

void closeLex(char *strbuffer){
    if (strbuffer){
        free(strbuffer);
    }
}

int readFile(const char *filename){
    FILE *inputfile;
    long bytenum;

    inputfile = fopen(filename, "r");

    if (inputfile == NULL){
        fprintf(stderr, "%s\n", "open file error");
        return 1;
    }
    
    fseek(inputfile, 0L, SEEK_END);
    bytenum = ftell(inputfile);
    fseek(inputfile, 0L, SEEK_SET);

    buffer = (char*)calloc(bytenum, sizeof(char));
    if (buffer == NULL){
        fprintf(stderr, "%s\n", "read buffer failed");
        return 1;
    }

    fread(buffer, sizeof(char), bytenum, inputfile);
    fclose(inputfile);

    return 0;
}

void closeFile(void){
    free(buffer);
}

void readBuffer(void){    //read a char and keep track of the postion
    c = buffer[pos++];
    if (buffer[pos - 2] == '\n'){

        //printf("last line = %d    last line start = %d    last newline pos = %d\n", linenumber, linestart, pos - 2);

        linenumber += 1;
        linepos = 0;
        prelinestart = linestart;
        linestart = pos - 1;
    }else{
        linepos++;
    }
}

void putBack(void){    //move back one char

    //printf("putback is called on char =  %d\n", c);
    

    pos -= 1;
    c = buffer[pos - 1];

    //printf("recovered char = %d\n", c);

    if (c == '\n'){
        linenumber -= 1;
        linestart = prelinestart;    //now prelinestart == linestart, so it cannot go back further if crossed a line
        linepos = pos - linestart;
        //reportError(putbackwarning);
    }else{
        linepos--;
    }
}

int skipLayoutNComment(void){
    int commentflag;

    do{
        readBuffer();
    }while ( (c == ' ') || (c == '\t') || (c == '\n') );   //skip whitespace

    while(c == '/'){
        readBuffer();
        if (c == '/'){
            do{
                readBuffer();
            }while (c != '\n' && c != '\0');
        }
        else if (c == '*'){    //case /*
            commentflag = 1;
            readBuffer();
            do{    //read 2 char ahead
                char temp = c;
                if (temp == '\0'){
                    break;
                }
                readBuffer();    //c is the char after temp
                if (c == '\0'){
                    break;
                }
                if (temp == '/' && c == '*'){
                    commentflag += 1;
                }
                if (temp == '*' && c == '/'){
                    commentflag -= 1;
                }
            }while (commentflag != 0);
            if (commentflag != 0){    //block comment error
                reportError(blkcommenterror);
                return 1;
            }
        }else{  //not a comment section
            putBack();    //now c is '/'
            return 0;
        }
        while ( (c == ' ') || (c == '\t') || (c == '\n') ){    //skip whitespace after a comment
        readBuffer();       
        }       //c could be at the start of a token or EOF
    }
    putBack();    //go back one char
    return 0;    
}

void reportError(const char * message){
    fprintf(stdout, "%s", message);
    printf("line = %d, linepos = %d \n", linenumber, linepos);
}

int getToken(TokenType * token){
    LexState_e state = ST_BEGIN;
    int writeflag = 1;
    int decsepflag = 0;    //decimal sperator flag
    int tokenstart = 0;    //mark the begin of a token
    int strpos = 0;    //position of each string in the string buffer
    
    if (skipLayoutNComment()){  //skip whitespace and comments
        return 1;
    }
    //start to read token from buffer
    while (state != ST_END){
        switch (state){
            case ST_BEGIN:    //lex start state
                tokenstart = pos;   //mark the begin of a token
                token->pos.linenum = linenumber;
                token->pos.charpos = linepos;
                readBuffer();
                if (ISLETTER((int)(c))){
                    state = ST_ID;
                    token->tokenclass = ID;
                }else if (ISDIGIT((int)(c))){
                    state = ST_NUM;
                    token->tokenclass = INTEGER;
                }else if (ISSPCHAR((int)(c))){
                    state = ST_SPCHAR;
                }else {
                    state = ST_END;
                    if (c == '\0'){    //EOF or error
                        token->tokenclass = ENDOFFILE;
                        strncpy(token->tokenstring, "EOF", TOKENBUFFERSIZE);
                        writeflag = 0;    //do not write to tokenstring
                    } else {
                        token->tokenclass = ERROR;
                    }
                }
                break;
            case ST_ID:     //for identifier
                readBuffer();

                //printf("current char = %c\n", c);

                if ((!ISLETTER((int)(c))) && (!ISDIGIT((int)(c)))){   //if c is not a letter or digit 
                    putBack();    //go back one char
                    state = ST_END;    //it's done for this token. record the location
                }
                break;
            case ST_NUM:    //for number
                readBuffer();

               // printf("current char = %c\n", c);

                if ((c == '.') && (!decsepflag)){   //if enounter a decimal seperator in the first time
                    decsepflag = 1;
                    token->tokenclass = FLOAT;
                }else if (!ISDIGIT((int)(c))){     //if c is not a digit
                    putBack();      //go back one char
                    state = ST_END;    //done for this token
                }
                break;
            case ST_SPCHAR:    //for special char
                //may need to read further, test first
                if (c == '<'){
                    state = ST_LESS;
                    token->tokenclass = LESS;
                }else if (c == '>'){
                    state = ST_GREATER;
                    token->tokenclass = GREATER;
                }else if (c == '='){
                    state = ST_EQUAL;
                    token->tokenclass = EQUAL;
                }else if (c == '!'){
                    state = ST_NOTEQUAL;
                    token->tokenclass = NOTEQUAL;
                }else if (c == ':'){
                    state = ST_COLON;    //or assign
                    token->tokenclass = COLON;
                }else if (c == '"'){    //string
                    state = ST_STRING;
                    token->tokenclass = STRING;
                    strpos = strbufferpos;    //mark the begin of a string in the buffer
                }
                else {
                    state = ST_END;    //no need to read further
                    switch(c){
                        case '&':
                            token->tokenclass = AND;
                            break;
                        case '|':
                            token->tokenclass = OR;
                            break;
                        case '+':
                            token->tokenclass = PLUS;
                            break;
                        case '-':
                            token->tokenclass = MINUS;
                            break;
                        case '*':
                            token->tokenclass = MUL;
                            break;
                        case '/':
                            token->tokenclass = DIV;
                            break;
                        case '(':
                            token->tokenclass = LEFTPAREN;
                            break;
                        case ')':
                            token->tokenclass = RIGHTPAREN;
                            break;
                        case '[':
                            token->tokenclass = LEFTSBRACKET;
                            break;
                        case ']':
                            token->tokenclass = RIGHTSBRACKET;
                            break;
                        case ';':
                            token->tokenclass = SEMICOLON;
                            break;
                        case '.':
                            token->tokenclass = PERIOD;
                            break;
                    }
                }
                break;
            case ST_LESS:
                readBuffer();
                state = ST_END;    //done for this token
                if (c == '='){      //less equal
                    token->tokenclass = LESSEQUAL;
                }else {
                    putBack();
                }
                break;
            case ST_GREATER:
                readBuffer();
                state = ST_END;    //done for this token
                if (c == '='){      //greater equal
                    token->tokenclass = GREATEREQUAL;
                }else {
                    putBack();
                }
                break;
            case ST_EQUAL:
                readBuffer();
                state = ST_END;
                if (c == '='){      //equal
                    state = ST_END;
                    token->tokenclass = EQUAL;
                }else {     //single '='. error
                    putBack();
                    token->tokenclass = ERROR;
                }
                break;
            case ST_NOTEQUAL:
                readBuffer();
                state = ST_END;
                if (c == '='){      //not equal
                    token->tokenclass = NOTEQUAL;
                }else {     //single '!'. error
                    putBack();
                    token->tokenclass = ERROR;
                }
                break;
            case ST_COLON:
                *token->tokenstring = strbufferpos;     //mark the begin of a string in the string buffer
                readBuffer();
                state = ST_END;
                if (c == '='){      //assign :=
                    token->tokenclass = ASSIGN;
                }else {     //single colon :
                    putBack();
                }
                break;
            case ST_STRING:
                readBuffer();
                writeflag = 0;      //do not write to token string
                
                if (strbuffer == 0){     //allocate memory
                    strbuffer = (char*)calloc(strbuffersize, sizeof(char));
                }
                if (c == '"'){        //end of string
                    strbuffer[strbufferpos++] = '\0';
                    state = ST_END;
                    *(token->tokenstring) = strpos;    //position of string in the buffer
                }else if (c == '\0'){       //end of file. error
                    strbuffer[strbufferpos++] = '\0';
                    state = ST_END;
                    token->tokenclass = ERROR;
                }else{
                    if (strbufferpos == strbuffersize){       //if the string buffer is full
                        char *tmp = (char*)calloc(strbuffersize * 2, sizeof(char));     //double the string buffer size
                        strncpy(tmp, strbuffer, strbuffersize);     //copy old to tmp
                        strbuffersize = strbuffersize * 2;      //update string buffer size
                        free(strbuffer);
                        strbuffer = tmp;
                    }
                    strbuffer[strbufferpos++] = c; 
                }
                break;
            default:
                state = ST_END;
                token->tokenclass = ERROR;
                reportError(lexerror);
                break;
        }
        if (state == ST_END && writeflag){
            int i;
            for (i = tokenstart; i < pos; i++){     //convert to upper case
                upchar = toupper(buffer[i]);
                buffer[i] = upchar;
            }
            if ((pos - tokenstart) < TOKENBUFFERSIZE){
                strncpy(token->tokenstring, buffer+tokenstart, pos - tokenstart);
            }else{
                reportError(tokenstrerror);
            }
        }
    }
    //check keyword
    if (token->tokenclass == ID){
        unsigned long tokenhash = djb2hash(token->tokenstring);

        int i;
        for (i = 0; i < 17; i++){
            if (tokenhash == keywordhash[i]){
                //token->tokenclass = i + 256;    //match the keyword macro
                //for testing purpose use 999
                token->tokenclass = 999;    //keyword == 999
            }
        }
        if (tokenhash == keywordhash[17] || tokenhash == keywordhash[18]){      //if token is TRUE or FALSE
            token->tokenclass = BOOLEAN;
        }
    }
    return 0;
}

void printToken(TokenType* token){
    switch(token->tokenclass){
        case ID: printf("ID"); 
        break;
        case INTEGER: printf("INT");
        break;
        case FLOAT: printf("FLOAT");
        break;
        case STRING: printf("STRING");
        printf(": %s\n", strbuffer + (*(token->tokenstring)));
        return;
        break;
        case BOOLEAN: printf("BOOLEAN");
        break;
        case ENDOFFILE: printf("EOF");
        break;
        case 999: printf("keyword");
        break;
        default: printf("OP");
        break; 
    }
    printf(": %s\n", token->tokenstring);
    memset(token->tokenstring, 0, sizeof token->tokenstring);
}