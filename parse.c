#include "9cc.h"


// 全てのローカル変数はこのリストに蓄積されていく
Var *locals;

// ローカル変数を名前で見つける
static Var *find_var(Token *tok) {
    for (Var *var = locals; var; var = var->next)
        if (strlen(var->name) == tok->len && !strncmp(tok->str, var->name, tok->len)) {
            return var;
        }
    return NULL;
}

static Node *new_node(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr) {
    Node *node = new_node(kind);
    node->lhs = expr;
    return node;
}

static Node *new_num(int val) {
    Node *node = new_node(ND_NUM);
    node->val = val;
    return node;
}

static Node *new_var_node(Var *var) {
    Node *node = new_node(ND_VAR);
    node->var = var;
    return node;
}

static Var *new_lvar(char *name) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->next = locals;
    locals = var;
    return var;
}

// program    = stmt*
// stmt       = expr ";"
//                | "return" expr ";"
// expr       = assign
// assign     = equality ("=" assign)?
// equality   = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add        = mul ("+" mul | "-" mul)*
// mul        = unary ("*" unary | "/" unary)*
// unary      = ("+" | "-")? primary
// primary    = num | ident | "(" expr ")"

static Node *stmt();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *primary();

// program    = stmt*
Function *program() {
    locals = NULL;

    Node head = {};
    Node *cur = &head;
    while (!at_eof()) {
        cur->next = stmt();
        cur = cur->next;
    }

    Function *prog = calloc(1, sizeof(Function));
    prog->node = head.next;
    prog->locals = locals;
    return prog;
}

// stmt       = expr ";"
//                | "return" expr ";"
static Node *stmt() {
    Node *node;

    if (consume("return")) {
        node = new_unary(ND_RETURN, expr());
    } else {
        node = expr();
    }

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
    if (consume("="))
        node = new_binary(ND_ASSIGN, node, assign());
    return node;
}

// equality   = relational ("==" relational | "!=" relational)*
static Node *equality() {
    Node *node = relational();

    for (;;) {
        if (consume("=="))
            node = new_binary(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_binary(ND_NE, node, relational());
        else
            return node;
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational() {
    Node *node = add();

    for (;;) {
        if (consume("<"))
            node = new_binary(ND_LT, node, add());
        else if (consume("<="))
            node = new_binary(ND_LE, node, add());
        else if (consume(">"))
            node = new_binary(ND_LT, add(), node);
        else if (consume(">="))
            node = new_binary(ND_LE, add(), node);
        else
            return node;
    }
}

// add        = mul ("+" mul | "-" mul)*
static Node *add() {
    Node *node = mul();

    for (;;) {
        if (consume("+"))
            node = new_binary(ND_ADD, node, mul());
        else if (consume("-"))
            node = new_binary(ND_SUB, node, mul());
        else
            return node;
    }
}

// mul        = unary ("*" unary | "/" unary)*
static Node *mul() {
    Node *node = unary();

    for (;;) {
        if (consume("*"))
            node = new_binary(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_binary(ND_DIV, node, unary());
        else
            return node;
    }
}

// unary      = ("+" | "-")? primary
static Node *unary() {
    if (consume("+"))
        return unary();
    if (consume("-"))
        return new_binary(ND_SUB, new_num(0), unary());
    return primary();
}

// primary    = num | ident | "(" expr ")"
static Node *primary() {
    // 次のトークンが"("なら "(" expr ")" のはず
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    Token *tok = consume_ident();
    if (tok) {
        Var *var = find_var(tok);
        if (!var) {
            var = new_lvar(strndup(tok->str, tok->len));
        }
        return new_var_node(var);
    }

    // そうでなければ整数のはず
    return new_num(expect_number());
}
