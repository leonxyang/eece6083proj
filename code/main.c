#include "lex.h"

extern char *strbuffer;

int main(int argc, const char *argv[]){
    readFile(argv[1]);
    TokenType token;
    initLex(&token);
    do{
        if (!getToken(&token)){
            printToken(&token);
        }
    }while (token.tokenclass != ENDOFFILE);
    closeLex(strbuffer);
    closeFile();
    return 0;
}