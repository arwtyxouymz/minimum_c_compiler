#include "9cc.h"


// 全てのローカル変数はこのリストに蓄積されていく
static VarList *locals;

// ローカル変数を名前で見つける
static Var *find_var(Token *tok) {
    for (VarList *vl = locals; vl; vl = vl->next) {
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

static Var *new_lvar(char *name) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
    return var;
}

// program    = function*
// function   = ident "(" params? ")" "{" stmt* "}"
// params     = ident ("," ident)*
// stmt2       = expr ";"
//            | "return" expr ";"
//            | "{" stmt* "}"
//            | "if" "(" expr ")" stmt ("else" stmt)?
//            | "while" "(" expr ")" stmt
//            | "for" "(" expr? ";" expr? ";" expr? ")" stmt
// expr       = assign
// assign     = equality ("=" assign)?
// equality   = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add        = mul ("+" mul | "-" mul)*
// mul        = unary ("*" unary | "/" unary)*
// unary      = "+"? primary
//            | "-"? primary
//            | "*" unary
//            | "&" unary
// primary    = num | ident args?  | "(" expr ")"
// args       = "(" ")"

static Function *function();
static Node *stmt();
static Node *stmt2();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *func_args();
static Node *primary();

// program    = function*
Function *program() {
    Function head = {};
    Function *cur = &head;

    while (!at_eof()) {
        cur->next = function();
        cur = cur->next;
    }
    return head.next;
}

static VarList *read_func_params() {
    if (consume(")"))
        return NULL;

    VarList *head = calloc(1, sizeof(VarList));
    head->var = new_lvar(expect_ident());
    VarList *cur = head;

    while (!consume(")")) {
        expect(",");
        cur->next = calloc(1, sizeof(VarList));
        cur->next->var = new_lvar(expect_ident());
        cur = cur->next;
    }

    return head;
}

// function   = ident "(" params? ")" "{" stmt* "}"
// params     = ident ("," ident)*
static Function *function() {
    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    fn->name = expect_ident();
    expect("(");
    fn->params = read_func_params();
    expect("{");

    Node head = {};
    Node *cur = &head;
    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }

    fn->node = head.next;
    fn->locals = locals;
    return fn;
}

static Node *read_expr_stmt() {
    Token *tok = token;
    return new_unary(ND_EXPR_STMT, expr(), tok);
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
static Node *stmt2() {
    Token *tok;
    if ((tok = consume("return"))) {
        Node *node = new_unary(ND_RETURN, expr(), tok);
        expect(";");
        return node;
    }

    if ((tok = consume("{"))) {
        Node head = {};
        Node *cur = &head;
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
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

// unary      = "+"? primary
//            | "-"? primary
//            | "*" unary
//            | "&" unary
static Node *unary() {
    Token *tok;
    if ((tok = consume("+")))
        return unary();
    if ((tok = consume("-")))
        return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
    if ((tok = consume("*")))
        return new_unary(ND_DEREF, unary(), tok);
    if ((tok = consume("&")))
        return new_unary(ND_ADDR, unary(), tok);
    return primary();
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

// primary    = num | ident func-args?  | "(" expr ")"
static Node *primary() {
    // 次のトークンが"("なら "(" expr ")" のはず
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    Token *tok;
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
            var = new_lvar(strndup(tok->str, tok->len));
        }
        return new_var_node(var, tok);
    }

    // そうでなければ整数のはず
    tok = token;
    if (tok->kind != TK_NUM)
        error_tok(tok, "expected expression");
    return new_num(expect_number(), tok);
}
