#include "9cc.h"


// 全てのローカル変数はこのリストに蓄積されていく
static VarList *locals;
// 全てのグローバル変数はこのリストに蓄積されていく
static VarList *globals;
static VarList *scope;

// ローカル変数を名前で見つける
static Var *find_var(Token *tok) {
    for (VarList *vl = scope; vl; vl = vl->next) {
        Var *var = vl->var;
        if (strlen(var->name) == tok->len && !strncmp(tok->str, var->name, tok->len)) {
            return var;
        }
    }
    return NULL;
}

static Node *new_node(NodeKind kind, Token *tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

static Node *new_num(int val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

static Node *new_var_node(Var *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

static Var *new_var(char *name, Type *ty, bool is_local) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;

    VarList *sc = calloc(1, sizeof(VarList));
    sc->var = var;
    sc->next = scope;
    scope = sc;
    return var;
}

static Var *new_lvar(char *name, Type *ty) {
    Var *var = new_var(name, ty, true);

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
    return var;
}

static Var *new_gvar(char *name, Type *ty) {
    Var *var = new_var(name, ty, false);

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = globals;
    globals = vl;
    return var;
}

static char *new_label() {
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return strndup(buf, 20);
}

// program       = (global-var | function)*
// global-var    = basetype ident ("[" num "]")* ";"
// function      = basetype ident "(" params? ")" "{" stmt* "}"
// params        = param ("," param)*
// param         = basetype ident
// stmt2         = expr ";"
//               | "return" expr ";"
//               | "{" stmt* "}"
//               | "if" "(" expr ")" stmt ("else" stmt)?
//               | "while" "(" expr ")" stmt
//               | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//               | declaration
// declaration   = basetype ident ("[" num "]")* ("=" expr) ";"
// basetype      = ("char" | "int" | struct-decl) "*"*
// struct-decl   = "struct" "{" struct-member "}"
// struct-member = basetype ident ("[" num "]")* ";"
// expr          = assign
// assign        = equality ("=" assign)?
// equality      = relational ("==" relational | "!=" relational)*
// relational    = add ("<" add | "<=" add | ">" add | ">=" add)*
// add           = mul ("+" mul | "-" mul)*
// mul           = unary ("*" unary | "/" unary)*
// unary         = ("+", "-", "*", "&")? unary
//               | postfix
// postfix       = primary ("[" expr "]")*
// primary       = "(" "{" stmt-expr-tail
//               | "(" expr ")"
//               | "sizeof" unary
//               | ident func-args?
//               | str
//               | num
// args          = "(" ident ("," ident)* ")"
// stmt-expr     = "(" "{" stmt stmt* "}" ")"

static Function *function();
static Type *basetype();
static Type *struct_decl();
static Member *struct_member();
static void global_var();
static Node *declaration();
static bool is_typename();
static Node *stmt();
static Node *stmt2();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *postfix();
static Node *func_args();
static Node *primary();

// トークンを一つ先読みして関数かグローバル変数かを判別する
static bool is_function() {
    Token *tok = token;
    basetype();
    bool isfunc = consume_ident() && consume("(");
    token = tok;
    return isfunc;
}

// program     = (global-var | function)*
Program *program() {
    Function head = {};
    Function *cur = &head;
    globals = NULL;

    while (!at_eof()) {
        if (is_function()) {
            cur->next = function();
            cur = cur->next;
        } else {
            global_var();
        }
    }

    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns = head.next;
    return prog;
}

// basetype    = ("char" | "int" | struct-decl) "*"*
static Type *basetype() {
    if (!is_typename(token)) {
        error_tok(token, "typename expected");
    }

    Type *ty;
    if (consume("char"))
        ty = char_type;
    else if (consume("int"))
        ty = int_type;
    else
        ty = struct_decl();

    while (consume("*"))
        ty = pointer_to(ty);
    return ty;
}

static Type *read_type_suffix(Type *base) {
    if (!consume("["))
        return base;
    int sz = expect_number();
    expect("]");
    base = read_type_suffix(base);
    return array_of(base, sz);
}

// struct-decl = "struct" "{" struct-member "}"
static Type *struct_decl() {
    // Read struct members
    expect("struct");
    expect("{");

    Member head = {};
    Member *cur = &head;

    while (!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_STRUCT;
    ty->members = head.next;

    // Assign offsets within the struct to members
    int offset = 0;
    for (Member *mem = ty->members; mem; mem = mem->next) {
        mem->offset = offset;
        offset += mem->ty->size;
    }
    ty->size = offset;

    return ty;
}

// struct-member = basetype ident ("[" num "]")* ";"
static Member *struct_member() {
    Member *mem = calloc(1, sizeof(Member));
    mem->ty = basetype();
    mem->name = expect_ident();
    mem->ty = read_type_suffix(mem->ty);
    expect(";");
    return mem;
}

// param      = basetype ident
static VarList *read_func_param() {
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = new_lvar(name, ty);
    return vl;
}

// params     = param ("," param)*
static VarList *read_func_params() {
    if (consume(")"))
        return NULL;

    VarList *head = read_func_param();
    VarList *cur = head;

    while (!consume(")")) {
        expect(",");
        cur->next = read_func_param();
        cur = cur->next;
    }

    return head;
}

// function   = basetype ident "(" params? ")" "{" stmt* "}"
static Function *function() {
    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    basetype();
    fn->name = expect_ident();
    expect("(");

    VarList *sc = scope;
    fn->params = read_func_params();
    expect("{");

    Node head = {};
    Node *cur = &head;
    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    scope = sc;

    fn->node = head.next;
    fn->locals = locals;
    return fn;
}

// global-var  = basetype ident ("[" num "]")* ";"
static void global_var() {
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    expect(";");
    new_gvar(name, ty);
}

// declaration = basetype ident ("[" num "]")* ("=" expr) ";"
static Node *declaration() {
    Token *tok = token;
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    Var *var = new_lvar(name, ty);

    if (consume(";")) {
        return new_node(ND_NULL, tok);
    }

    expect("=");

    Node *lhs = new_var_node(var, tok);
    Node *rhs = expr();
    expect(";");
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
    return new_unary(ND_EXPR_STMT, node, tok);
}

static Node *read_expr_stmt() {
    Token *tok = token;
    return new_unary(ND_EXPR_STMT, expr(), tok);
}

static bool is_typename() {
    return peek("char") || peek("int") || peek("struct");
}

static Node *stmt() {
    Node *node = stmt2();
    add_type(node);
    return node;
}

// stmt2      = expr ";"
//            | "return" expr ";"
//            | "{" stmt* "}"
//            | "if" "(" expr ")" stmt ("else" stmt)?
//            | "while" "(" expr ")" stmt
//            | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//            | declaration
static Node *stmt2() {
    Token *tok;
    if ((tok = consume("return"))) {
        Node *node = new_unary(ND_RETURN, expr(), tok);
        expect(";");
        return node;
    }

    if ((tok = consume("if"))) {
        Node *node = new_node(ND_IF, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if (consume("else"))
            node->els = stmt();
        return node;
    }

    if ((tok = consume("while"))) {
        Node *node = new_node(ND_WHILE, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if ((tok = consume("for"))) {
        Node *node = new_node(ND_FOR, tok);
        expect("(");
        if (!consume(";")) {
            node->init = read_expr_stmt();
            expect(";");
        }
        if (!consume(";")) {
            node->cond = expr();
            expect(";");
        }
        if (!consume(")")) {
            node->inc = read_expr_stmt();
            expect(")");
        }
        node->then = stmt();
        return node;
    }

    if ((tok = consume("{"))) {
        Node head = {};
        Node *cur = &head;
        VarList *sc = scope;
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }
        scope = sc;

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
        return node;
    }


    if (is_typename()) {
        return declaration();
    }

    Node *node = read_expr_stmt();
    expect(";");

    return node;
}

// expr       = assign
static Node *expr() {
    return assign();
}

// assign       = equality ("=" assign)?
static Node *assign() {
    Node *node = equality();
    Token *tok;
    if ((tok = consume("=")))
        node = new_binary(ND_ASSIGN, node, assign(), tok);
    return node;
}

// equality   = relational ("==" relational | "!=" relational)*
static Node *equality() {
    Node *node = relational();
    Token *tok;

    for (;;) {
        if ((tok = consume("==")))
            node = new_binary(ND_EQ, node, relational(), tok);
        else if ((tok = consume("!=")))
            node = new_binary(ND_NE, node, relational(), tok);
        else
            return node;
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational() {
    Node *node = add();
    Token *tok;

    for (;;) {
        if ((tok = consume("<")))
            node = new_binary(ND_LT, node, add(), tok);
        else if ((tok = consume("<=")))
            node = new_binary(ND_LE, node, add(), tok);
        else if ((tok = consume(">")))
            node = new_binary(ND_LT, add(), node, tok);
        else if ((tok = consume(">=")))
            node = new_binary(ND_LE, add(), node, tok);
        else
            return node;
    }
}

static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
        return new_binary(ND_ADD, lhs, rhs, tok);
    }
    if (lhs->ty->base && is_integer(rhs->ty)) {
        return new_binary(ND_PTR_ADD, lhs, rhs, tok);
    }
    if (is_integer(lhs->ty) && rhs->ty->base) {
        return new_binary(ND_PTR_ADD, lhs, rhs, tok);
    }
    error_tok(tok, "invalid operands");
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
        return new_binary(ND_SUB, lhs, rhs, tok);
    }
    if (lhs->ty->base && is_integer(rhs->ty)) {
        return new_binary(ND_PTR_SUB, lhs, rhs, tok);
    }
    if (lhs->ty->base && rhs->ty->base) {
        return new_binary(ND_PTR_DIFF, lhs, rhs, tok);
    }
    error_tok(tok, "invalid operands");
}

// add        = mul ("+" mul | "-" mul)*
static Node *add() {
    Node *node = mul();
    Token *tok;

    for (;;) {
        if ((tok = consume("+")))
            node = new_add(node, mul(), tok);
        else if ((tok = consume("-")))
            node = new_sub(node, mul(), tok);
        else
            return node;
    }
}

// mul        = unary ("*" unary | "/" unary)*
static Node *mul() {
    Node *node = unary();
    Token *tok;

    for (;;) {
        if ((tok = consume("*")))
            node = new_binary(ND_MUL, node, unary(), tok);
        else if ((tok = consume("/")))
            node = new_binary(ND_DIV, node, unary(), tok);
        else
            return node;
    }
}

// unary      = ("+", "-", "*", "&")? unary
//            | postfix
static Node *unary() {
    Token *tok;
    if ((tok = consume("+")))
        return unary();
    if ((tok = consume("-")))
        return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
    if ((tok = consume("&")))
        return new_unary(ND_ADDR, unary(), tok);
    if ((tok = consume("*")))
        return new_unary(ND_DEREF, unary(), tok);
    return postfix();
}

static Member *find_member(Type *ty, char *name) {
    for (Member *mem = ty->members; mem; mem = mem->next)
        if (!strcmp(mem->name, name))
            return mem;
    return NULL;
}

static Node *struct_ref(Node *lhs) {
    add_type(lhs);
    if (lhs->ty->kind != TY_STRUCT)
        error_tok(lhs->tok, "not a struct");

    Token *tok = token;
    Member *mem = find_member(lhs->ty, expect_ident());
    if (!mem)
        error_tok(tok, "no such member");

    Node *node = new_unary(ND_MEMBER, lhs, tok);
    node->member = mem;

    return node;
}

// postfix     = primary ("[" expr "]" | "." ident)*
static Node *postfix() {
    Node *node = primary();
    Token *tok;

    for (;;) {
        if ((tok = consume("["))) {
            // x[y] is short for *(x + y)
            Node *exp = new_add(node, expr(), tok);
            expect("]");
            node = new_unary(ND_DEREF, exp, tok);
            continue;
        }

        if ((tok = consume("."))) {
            node = struct_ref(node);
            continue;
        }

        return node;
    }
}

// stmt-expr = "(" "{" stmt stmt* "}" ")"
// Statement expression is a GNU C extention
static Node *stmt_expr(Token *tok) {
    VarList *sc = scope;

    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = stmt();
    Node *cur = node->body;

    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }

    expect(")");

    scope = sc;

    if (cur->kind != ND_EXPR_STMT) {
        error_tok(cur->tok, "stmt expr returning void is not supported");
    }
    memcpy(cur, cur->lhs, sizeof(Node));
    return node;
}

// func-args = "(" (assign ("," assign)*)? ")"
static Node *func_args() {
    if (consume(")")) {
        return NULL;
    }

    Node *head = assign();
    Node *cur = head;
    while (consume(",")) {
        cur->next = assign();
        cur = cur->next;
    }
    expect(")");
    return head;
}

// primary     = "(" "{" stmt-expr-tail
//             | "(" expr ")"
//             | "sizeof" unary
//             | ident func-args?
//             | str
//             | num
static Node *primary() {
    Token *tok;

    // 次のトークンが"("なら "(" expr ")" のはず
    if ((tok = consume("("))) {
        if (consume("{"))
            return stmt_expr(tok);
        Node *node = expr();
        expect(")");
        return node;
    }

    if ((tok = consume("sizeof"))) {
        Node *node = unary();
        add_type(node);
        return new_num(node->ty->size, tok);
    }

    if ((tok = consume_ident())) {
        // 関数呼び出し
        if (consume("(")) {
            Node *node = new_node(ND_FUNCALL, tok);
            node->funcname = strndup(tok->str, tok->len);
            node->args = func_args();
            return node;
        }

        // 変数
        Var *var = find_var(tok);
        if (!var) {
            error_tok(tok, "undefined variable");
        }
        return new_var_node(var, tok);
    }

    tok = token;
    if (tok->kind == TK_STR) {
        token = token->next;

        Type *ty = array_of(char_type, tok->cont_len);
        Var *var = new_gvar(new_label(), ty);
        var->contents = tok->contents;
        var->cont_len = tok->cont_len;
        return new_var_node(var, tok);
    }

    // そうでなければ整数のはず
    if (tok->kind != TK_NUM)
        error_tok(tok, "expected expression");
    return new_num(expect_number(), tok);
}
