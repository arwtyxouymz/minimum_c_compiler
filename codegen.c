#include "9cc.h"

static int labelseq = 1;

static void gen_addr(Node *node) {
    if (node->kind != ND_VAR) {
        error("代入の左辺値が変数ではありません");
    }
    printf("  lea rax, [rbp-%d]\n", node->var->offset);
    printf("  push rax\n");
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
        case ND_NUM:
            printf("  push %d\n", node->val);
            return;
        case ND_VAR:
            gen_addr(node);
            load();
            return;
        case ND_ASSIGN:
            gen_addr(node->lhs);
            gen(node->rhs);
            store();
            return;
        case ND_EXPR_STMT:
            gen(node->lhs);
            printf("  add rsp, 8\n");
            return;
        case ND_IF: {
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
        case ND_RETURN:
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  jmp .L.return\n");
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
        case ND_SUB:
            printf("  sub rax, rdi\n");
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
    printf(".global _main\n");
    printf("_main:\n");

    // プロローグ
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", prog->stack_size);

    // 抽象構文木を降りながらコード生成
    for (Node *node = prog->node; node; node = node->next)
        gen(node);

    // エピローグ
    printf(".L.return:\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}
