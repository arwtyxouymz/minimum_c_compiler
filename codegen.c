#include "9cc.h"

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
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

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
    for (Node *node = prog->node; node; node = node->next) {
        gen(node);
        printf("  pop rax\n");
    }

    // エピローグ
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}
