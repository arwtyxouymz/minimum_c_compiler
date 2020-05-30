#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef struct Type Type;
typedef struct Member Member;

//
// tokenize.c
//

// トークンの種類
typedef enum {
    TK_RESERVED, // 記号
    TK_STR,      // 文字列リテラル
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

    char *contents; // 終端NULL('\0')を含む文字列リテラルのコンテンツ
    char cont_len;  // 文字列リテラルの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
Token *peek(char *s);
Token *consume(char *op);
Token *consume_ident();
void expect(char *op);
long expect_number();
char *expect_ident();
bool at_eof();
Token *tokenize();


extern char *filename;
extern Token *token;
extern char *user_input;



//
// parse.c
//

// 変数
typedef struct Var Var;
struct Var {
    char *name;    // 変数の名前
    Type *ty;      // Type
    bool is_local; // Local変数かGlobal変数か

    // ローカル変数
    int  offset;   // RBPからのオフセット

    // グローバル変数
    char *contents;
    int cont_len;
};

typedef struct VarList VarList;
struct VarList {
    VarList *next;
    Var *var;
};

// AST node
typedef enum {
    ND_ADD,       // num + num
    ND_PTR_ADD,   // ptr + num or num + ptr
    ND_SUB,       // num - num
    ND_PTR_SUB,   // ptr - num
    ND_PTR_DIFF,  // ptr - ptr
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
    ND_STMT_EXPR, // 文式?
    ND_IF,        // if文
    ND_WHILE,     // while文
    ND_FOR,       // for文
    ND_BLOCK,     // Block
    ND_FUNCALL,   // 関数呼び出し
    ND_MEMBER,    //  . (struct member access)
    ND_ADDR,      // アドレス
    ND_DEREF,     // 参照外し
    ND_NUM,       // 整数
    ND_NULL,      // NULL
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind; // ノードの型
    Node *next;    // 次のノード
    Type *ty;      // Type Ex) int or point to int
    Token *tok;    // トークン

    Node *lhs;     // 左辺
    Node *rhs;     // 右辺

    /* if文 or while文 or for文 の時に使う */
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;

    /* Block もしくは 文式 の時に使う */
    Node *body;

    /* 構造体のメンバアクセス */
    Member *member;

    /* 関数呼び出しの時に使う */
    char *funcname;
    Node *args;
    
    Var *var;      // kindがND_VARの時に使う
    int val;       // kindがND_NUMの場合のみ使う
};

typedef struct Function Function;
struct Function {
    Function *next;
    char *name;
    VarList *params;

    Node *node;
    VarList *locals;
    int stack_size;
};

typedef struct {
    VarList *globals;
    Function *fns;
} Program;

Program *program();

//
//typing.c
//
typedef enum {
    TY_CHAR,
    TY_INT,
    TY_PTR,
    TY_STRUCT,
    TY_ARRAY
} TypeKind;

struct Type {
    TypeKind kind;
    int    size;      // sizeof()の時に使う
    Type   *base;     // pointer or array
    size_t array_len; // 配列の時に使う
    Member *members;  // struct
};

// Struct member
struct Member {
    Member *next;
    Type *ty;
    char *name;
    int offset;
};

extern Type *char_type;
extern Type *int_type;

bool is_integer(Type *ty);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
void add_type(Node *node);

//
// codegen.c
//
void codegen(Program *prog);
