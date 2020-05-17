#include "9cc.h"


int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "引数の個数が正しくありません\n");
        return 1;
    }

    // トークナイズしてパースする
    user_input = argv[1];
    token = tokenize();
    Function *prog = program();

    for (Function *fn = prog; fn; fn = fn->next) {
        // ローカル変数にオフセットを設定する
        int offset = 0;
        for (Var *var = locals; var; var = var->next) {
            offset += 8;
            var->offset = offset;
        }
        fn->stack_size = offset;
    }

    codegen(prog);

    return 0;
}
