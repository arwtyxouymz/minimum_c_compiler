#include "9cc.h"

static char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static int labelseq = 1;
static char *funcname;

static void gen(Node *node);

static void gen_addr(Node *node) {
    switch (node->kind) {
        case ND_VAR:
            printf("  lea rax, [rbp-%d]\n", node->var->offset);
            printf("  push rax\n");
            return;
        case ND_DEREF:
            gen(node->lhs);
            return;
    }

    error_tok(node->tok, "Not an lvalue");
}

static void gen_lval(Node *node) {
    if (node->ty->kind == TY_ARRAY)
        error_tok(node->tok, "not an lvalue");
    gen_addr(node);
}

static void load() {
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

static void store() {
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n");
    printf("  push rdi\n");
}

static void gen(Node *node) {
    // statement 系
    switch (node->kind) {
        case ND_NULL:
            return;
        case ND_NUM:
            printf("  push %d\n", node->val);
            return;
        case ND_VAR:
            gen_addr(node);
            if (node->ty->kind != TY_ARRAY)
                load();
            return;
        case ND_ASSIGN:
            gen_lval(node->lhs);
            gen(node->rhs);
            store();
            return;
        case ND_EXPR_STMT:
            gen(node->lhs);
            printf("  add rsp, 8\n");
            return;
        case ND_IF:
        {
            int seq = labelseq++;
            if (node->els) {
                gen(node->cond);
                printf("  pop rax\n");
                printf("  cmp rax, 0\n");
                printf("  je  .L.else.%d", seq);
                gen(node->then);
                printf("  jmp .L.end.%d", seq);
                printf(".L.else.%d:", seq);
                gen(node->els);
                printf(".L.end.%d:", seq);
            } else {
                gen(node->cond);
                printf("  pop rax\n");
                printf("  cmp rax, 0\n");
                printf("  je  .L.end.%d\n", seq);
                gen(node->then);
                printf(".L.end.%d:\n", seq);
            }
            return;
        }
        case ND_WHILE:
        {
            int seq = labelseq++;
            printf(".L.begin.while.%d:\n", seq);
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .L.end.while.%d\n", seq);
            gen(node->then);
            printf("  jmp .L.begin.while.%d\n", seq);
            printf(".L.end.while.%d:\n", seq);
            return;
        }
        case ND_FOR:
        {
            int seq = labelseq++;
            if (node->init)
                gen(node->init);
            printf(".L.begin.%d:\n", seq);
            if (node->cond) {
                gen(node->cond);
                printf("  pop rax\n");
                printf("  cmp rax, 0\n");
                printf("  je  .L.end.%d\n", seq);
            }
            gen(node->then);
            if (node->inc)
                gen(node->inc);
            printf("  jmp .L.begin.%d\n", seq);
            printf(".L.end.%d:\n", seq);
            return;
        }
        case ND_BLOCK:
            for (Node *n = node->body; n; n = n->next)
                gen(n);
            return;
        case ND_FUNCALL:
        {
            int nargs = 0;
            for (Node *arg = node->args; arg; arg = arg->next) {
                gen(arg);
                nargs++;
            }

            for (int i = nargs - 1; i >= 0; i--)
                printf("  pop %s\n", argreg[i]);

            // ABIの要求により，関数呼び出しの前にRSPを16バイト境界に揃えなくてはならない
            int seq = labelseq++;
            printf("  mov rax, rsp\n");
            printf("  and rax, 15\n");
            printf("  jnz .L.call.%d\n", seq);
            printf("  mov rax, 0\n");
            printf("  call _%s\n", node->funcname);
            printf("  jmp .L.end.%d\n", seq);
            printf(".L.call.%d:\n", seq);
            printf("  sub rsp, 8\n");
            printf("  mov rax, 0\n");
            printf("  call _%s\n", node->funcname);
            printf("  add rsp, 8\n");
            printf(".L.end.%d:\n", seq);
            printf("  push rax\n");
            return;
        }
        case ND_ADDR:
            gen_addr(node->lhs);
            return;
        case ND_DEREF:
            gen(node->lhs);
            if (node->ty->kind != TY_ARRAY)
                load();
            return;
        case ND_RETURN:
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  jmp .L.return.%s\n", funcname);
            return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    // expression 系
    switch (node->kind) {
        case ND_ADD:
            printf("  add rax, rdi\n");
            break;
        case ND_PTR_ADD:
            printf("  imul rdi, %d\n", node->ty->base->size);
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            printf("  sub rax, rdi\n");
            break;
        case ND_PTR_SUB:
            printf("  imul rdi, %d\n", node->ty->base->size);
            printf("  sub rax, rdi\n");
            break;
        case ND_PTR_DIFF:
            printf("  sub rax, rdi\n");
            printf("  cqo\n");
            printf("  mov rdi, %d\n", node->lhs->ty->base->size);
            printf("  idiv rdi\n");
            break;
        case ND_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case ND_EQ:
            printf("  cmp rax, rdi\n");
            printf("  sete al\n");
            printf("  movzx rax, al\n");
            break;
        case ND_NE:
            printf("  cmp rax, rdi\n");
            printf("  setne al\n");
            printf("  movzx rax, al\n");
            break;
        case ND_LT:
            printf("  cmp rax, rdi\n");
            printf("  setl al\n");
            printf("  movzx rax, al\n");
            break;
        case ND_LE:
            printf("  cmp rax, rdi\n");
            printf("  setle al\n");
            printf("  movzx rax, al\n");
            break;
    }
    printf("  push rax\n");
}

void codegen(Function *prog) {
    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");

    for (Function *fn = prog; fn; fn = fn->next) {
        printf(".global _%s\n", fn->name);
        printf("_%s:\n", fn->name);
        funcname = fn->name;

        // プロローグ
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", fn->stack_size);

        // 引数をスタックにpush
        int i = 0;
        for (VarList *vl = fn->params; vl; vl = vl->next) {
            Var *var = vl->var;
            printf("  mov [rbp-%d], %s\n", var->offset, argreg[i++]);
        }

        // 抽象構文木をを降りながらコード生成
        for (Node *node = fn->node; node; node = node->next)
            gen(node);

        // エピローグ
        printf(".L.return.%s:\n", funcname);
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");

        printf("  ret\n");
    }
}
