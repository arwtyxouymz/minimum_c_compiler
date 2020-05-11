#define _GNU_SOURCE
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

//
// tokenize.c
//

// トークンの種類
typedef enum {
    TK_RESERVED, // 記号
    TK_NUM,      // 数値
    TK_IDENT,    // 変数
    TK_EOF,      // 入力の終わりを表すトークン
} TokenKind;

// トークン型
typedef struct Token Token;
struct Token {
    TokenKind kind; // トークンの型
    Token *next;    // 次の入力トークン
    int val;        // kindがTK_NUMの場合，その数値
    char *str;      // トークン文字列
    int len;        // トークンの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
bool consume(char *op);
Token *consume_ident();
void expect(char *op);
long expect_number();
bool at_eof();
Token *tokenize();


extern Token *token;
extern char *user_input;



//
// parse.c
//

typedef struct Var Var;
struct Var {
    Var *next; // 次の変数がNULL
    char *name; // 変数の名前
    int offset; // RBPからのオフセット
};

// ローカル変数
Var *locals;

// AST node
typedef enum {
    ND_ADD,       // +
    ND_SUB,       // -
    ND_MUL,       // *
    ND_DIV,       // /
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_ASSIGN,    // =
    ND_VAR,       // 変数
    ND_RETURN,    // return
    ND_EXPR_STMT, // 式文
    ND_IF,        // if文
    ND_WHILE,     // while文
    ND_NUM,       // 整数
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind; // ノードの型
    Node *next;    // 次のノード

    Node *lhs;     // 左辺
    Node *rhs;     // 右辺

    /* if文 or while文 の時に使う */
    Node *cond;
    Node *then;
    Node *els;
    
    Var *var;      // kindがND_VARの時に使う
    int val;       // kindがND_NUMの場合のみ使う
};

typedef struct Function Function;
struct Function {
    Node *node;
    Var *locals;
    int stack_size;
};

Function *program();

//
// codegen.c
//
void codegen(Function *prog);
