#ifndef GLOBALS_H 
#define GLOBALS_H

#include <stdlib.h>

#define ID 256

#define PROGRAM 257
#define IS 258
#define BEGIN 259
#define END 260
#define GLOBAL 261
#define PROCEDURE 262
#define VARIABLE 263

#define INTEGER 264
#define FLOAT 265
#define STRING 266
#define BOOL 267

#define IF 268
#define THEN 269
#define ELSE 270
#define FOR 271
#define RETURN 272

#define NOT 273

#define OR 274
#define AND 275

#define PLUS 276
#define MINUS 277
#define LESS 278
#define GREATER 279
#define LESSEQUAL 280
#define GREATEREQUAL 281
#define EQUAL 282
#define NOTEQUAL 283
#define ASSIGN 284
#define MUL 285
#define DIV 286

#define COMMA 287
#define PERIOD 288
#define COLON 289
#define SEMICOLON 290
#define CARET 291

#define DOUBLEQUOTE 292
#define LEFTPAREN 293
#define RIGHTPAREN 294
#define LEFTSBRACKET 295
#define RIGHTSBRACKET 296

#define BOOLEAN 297

#define ERROR 298
#define ENDOFFILE 299

#define TOKENBUFFERSIZE 32

extern char * strbuffer;    //points to the buffer that holds the string

typedef struct{
    int linenum;
    int charpos;
} PosInFile;

typedef struct{
    int tokenclass;
    PosInFile pos;
    char tokenstring[TOKENBUFFERSIZE];
} TokenType;

unsigned long djb2hash(char *);

int readFile(const char *);
void closeFile(void);

void initLex(TokenType *);
void closeLex(char *);

int getToken(TokenType *);
void printToken(TokenType *);

#endif