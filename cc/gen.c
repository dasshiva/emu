// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "8cc.h"

bool dumpstack = false;
bool dumpsource = true;

static char *REGS[] = {"x0", "x1", "x2", "x3", "x4", "x5"};
#define SREGS REGS
#define MREGS REGS
static int TAB = 8;
static Vector *functions = &EMPTY_VECTOR;
static int stackpos;
static int numgp;
static int numfp;
static FILE *outputfp;
static Map *source_files = &EMPTY_MAP;
static Map *source_lines = &EMPTY_MAP;
static char *last_loc = "";

static void emit_addr(Node *node);
static void emit_expr(Node *node);
static void emit_decl_init(Vector *inits, int off, int totalsize);
static void do_emit_data(Vector *inits, int size, int off, int depth);
static void emit_data(Node *v, int off, int depth);

#define REGAREA_SIZE 176

#define emit(...)        emitf(__LINE__, "\t" __VA_ARGS__)
#define emit_noindent(...)  emitf(__LINE__, __VA_ARGS__)

#ifdef __GNUC__
#define SAVE                                                            \
    int save_hook __attribute__((unused, cleanup(pop_function)));       \
    if (dumpstack)                                                      \
        vec_push(functions, (void *)__func__);

static void pop_function(void *ignore) {
    if (dumpstack)
        vec_pop(functions);
}
#else
#define SAVE
#endif

static char *get_caller_list() {
    Buffer *b = make_buffer();
    for (int i = 0; i < vec_len(functions); i++) {
        if (i > 0)
            buf_printf(b, " -> ");
        buf_printf(b, "%s", vec_get(functions, i));
    }
    buf_write(b, '\0');
    return buf_body(b);
}

void set_output_file(FILE *fp) {
    outputfp = fp;
}

void close_output_file() {
    fclose(outputfp);
}

static void emitf(int line, char *fmt, ...) {
    // Replace "#" with "%%" so that vfprintf prints out "#" as "%".
    char buf[256];
    int i = 0;
    for (char *p = fmt; *p; p++) {
        assert(i < sizeof(buf) - 3);
        if (*p == '#') {
            buf[i++] = '%';
            buf[i++] = '%';
        } else {
            buf[i++] = *p;
        }
    }
    buf[i] = '\0';

    va_list args;
    va_start(args, fmt);
    int col = vfprintf(outputfp, buf, args);
    va_end(args);

    if (dumpstack) {
        for (char *p = fmt; *p; p++)
            if (*p == '\t')
                col += TAB - 1;
        int space = (28 - col) > 0 ? (30 - col) : 2;
        fprintf(outputfp, "%*c %s:%d", space, '#', get_caller_list(), line);
    }
    fprintf(outputfp, "\n");
}

static void emit_nostack(char *fmt, ...) {
    fprintf(outputfp, "\t");
    va_list args;
    va_start(args, fmt);
    vfprintf(outputfp, fmt, args);
    va_end(args);
    fprintf(outputfp, "\n");
}

static char *get_int_reg(Type *ty, char r) {
    assert(r == 'a' || r == 'c');
    return (r == 'a') ? "x0" : "x2";
}

static char *get_load_inst(Type *ty) {
  return "mov";
}

static int align(int n, int m) {
    int rem = n % m;
    return (rem == 0) ? n : n - rem + m;
}

static void push_xmm(int reg) {
    SAVE;
    emit("sub $8, #sp");
    emit("movsd #xmm%d, (sp)", reg);
    stackpos += 8;
}

static void pop_xmm(int reg) {
    SAVE;
    emit("movsd (sp), #xmm%d", reg);
    emit("add $8, sp");
    stackpos -= 8;
    assert(stackpos >= 0);
}

static void push(char *reg) {
    SAVE;
    emit("push %s", reg);
    stackpos += 4;
}

static void pop(char *reg) {
    SAVE;
    emit("pop %s", reg);
    stackpos -= 4;
    assert(stackpos >= 0);
}

static int push_struct(int size) {
    SAVE;
    int aligned = align(size, 8);
    emit("sub %d, sp", aligned);
    emit("mov x2, -8(sp)");
    emit("mov x5, -16(sp)");
    emit("mov x0, x2");
    int i = 0;
    for (; i < size; i += 8) {
        emit("movq %d(x2), x5", i);
        emit("mov x5, %d(sp)", i);
    }
    for (; i < size; i += 4) {
        emit("movl %d(x2), x5", i);
        emit("movl x5d, %d(sp)", i);
    }
    for (; i < size; i++) {
        emit("movb %d(x2), x5", i);
        emit("movb x5b, %d(sp)", i);
    }
    emit("mov -8(sp), x2");
    emit("mov -16(sp), x5");
    stackpos += aligned;
    return aligned;
}

static void maybe_emit_bitshift_load(Type *ty) {
    SAVE;
    if (ty->bitsize <= 0)
        return;
    emit("shr %d, x0", ty->bitoff);
    push("x2");
    emit("mov $0x%lx, x2", (1 << (long)ty->bitsize) - 1);
    emit("and x2, x0");
    ("x2");
}

static void maybe_emit_bitshift_save(Type *ty, char *addr) {
    SAVE;
    if (ty->bitsize <= 0)
        return;
    push("x2");
    push("x4");
    emit("mov $0x%lx, x3", (1 << (long)ty->bitsize) - 1);
    emit("and x3, x0");
    emit("shl %d, x0", ty->bitoff);
    emit("mov %s, %s", addr, get_int_reg(ty, 'c'));
    emit("mov $0x%lx, x3", ~(((1 << (long)ty->bitsize) - 1) << ty->bitoff));
    emit("and x3, x2");
    emit("or x2, x0");
    pop("x4");
    pop("x2");
}

static void emit_gload(Type *ty, char *label, int off) {
    SAVE;
    if (ty->kind == KIND_ARRAY) {
        if (off)
            emit("lea %s+%d(x31), x0", label, off);
        else
            emit("lea %s(x31), x0", label);
        return;
    }
    char *inst = get_load_inst(ty);
    emit("%s %s+%d(x31), x0", inst, label, off);
    maybe_emit_bitshift_load(ty);
}

static void emit_intcast(Type *ty) {
    switch(ty->kind) {
    case KIND_BOOL:
    case KIND_CHAR:
        ty->usig ? emit("movzbq x29, x0") : emit("movsbq x29, x0");
        return;
    case KIND_SHORT:
        ty->usig ? emit("movzwq #ax, x0") : emit("movswq #ax, x0");
        return;
    case KIND_INT:
        emit("mov x0, x0") ;
        return;
    case KIND_LONG:
    case KIND_LLONG:
        return;
    }
}

static void emit_toint(Type *ty) {
    SAVE;
    if (ty->kind == KIND_FLOAT)
        emit("cvttss2si #xmm0, x0");
    else if (ty->kind == KIND_DOUBLE)
        emit("cvttsd2si #xmm0, x0");
}

static void emit_lload(Type *ty, char *base, int off) {
    SAVE;
    if (ty->kind == KIND_ARRAY) {
        emit("lea %d(%s), x0", off, base);
    } else if (ty->kind == KIND_FLOAT) {
        emit("movss %d(%s), #xmm0", off, base);
    } else if (ty->kind == KIND_DOUBLE || ty->kind == KIND_LDOUBLE) {
        emit("movsd %d(%s), #xmm0", off, base);
    } else {
        char *inst = get_load_inst(ty);
        emit("%s %d(%s), x0", inst, off, base);
        maybe_emit_bitshift_load(ty);
    }
}

static void maybe_convert_bool(Type *ty) {
    if (ty->kind == KIND_BOOL) {
        emit("test x0, x0");
        emit("setne x29");
    }
}

static void emit_gsave(char *varname, Type *ty, int off) {
    SAVE;
    assert(ty->kind != KIND_ARRAY);
    maybe_convert_bool(ty);
    char *reg = get_int_reg(ty, 'a');
    char *addr = format("%s+%d(x31)", varname, off);
    maybe_emit_bitshift_save(ty, addr);
    emit("mov %s, %s", reg, addr);
}

static void emit_lsave(Type *ty, int off) {
    SAVE;
    if (ty->kind == KIND_FLOAT) {
        emit("movss #xmm0, %d(x2)", off);
    } else if (ty->kind == KIND_DOUBLE) {
        emit("movsd #xmm0, %d(x2)", off);
    } else {
        maybe_convert_bool(ty);
        char *reg = get_int_reg(ty, 'a');
        char *addr = format("%d(%%x2)", off);
        maybe_emit_bitshift_save(ty, addr);
        emit("mov %s, %s", reg, addr);
    }
}

static void do_emit_assign_deref(Type *ty, int off) {
    SAVE;
    emit("mov (sp), x2");
    char *reg = get_int_reg(ty, 'c');
    if (off)
        emit("mov %s, %d(x0)", reg, off);
    else
        emit("mov %s, (x0)", reg);
    pop("x0");
}

static void emit_assign_deref(Node *var) {
    SAVE;
    push("x0");
    emit_expr(var->operand);
    do_emit_assign_deref(var->operand->ty->ptr, 0);
}

static void emit_pointer_arith(char kind, Node *left, Node *right) {
    SAVE;
    emit_expr(left);
    push("x2");
    push("x0");
    emit_expr(right);
    int size = left->ty->ptr->size;
    if (size > 1)
        emit("imul %d, x0", size);
    emit("mov x0, x2");
    pop("x0");
    switch (kind) {
    case '+': emit("add x2, x0"); break;
    case '-': emit("sub x2, x0"); break;
    default: error("invalid operator '%d'", kind);
    }
    pop("x2");
}

static void emit_zero_filler(int start, int end) {
    SAVE;
    for (; start <= end - 4; start += 4)
        emit("movl $0, %d(x2)", start);
    for (; start < end; start++)
        emit("movb $0, %d(x2)", start);
}

static void ensure_lvar_init(Node *node) {
    SAVE;
    assert(node->kind == AST_LVAR);
    if (node->lvarinit)
        emit_decl_init(node->lvarinit, node->loff, node->ty->size);
    node->lvarinit = NULL;
}

static void emit_assign_struct_ref(Node *struc, Type *field, int off) {
    SAVE;
    switch (struc->kind) {
    case AST_LVAR:
        ensure_lvar_init(struc);
        emit_lsave(field, struc->loff + field->offset + off);
        break;
    case AST_GVAR:
        emit_gsave(struc->glabel, field, field->offset + off);
        break;
    case AST_STRUCT_REF:
        emit_assign_struct_ref(struc->struc, field, off + struc->ty->offset);
        break;
    case AST_DEREF:
        push("x0");
        emit_expr(struc->operand);
        do_emit_assign_deref(field, field->offset + off);
        break;
    default:
        error("internal error: %s", node2s(struc));
    }
}

static void emit_load_struct_ref(Node *struc, Type *field, int off) {
    SAVE;
    switch (struc->kind) {
    case AST_LVAR:
        ensure_lvar_init(struc);
        emit_lload(field, "x2", struc->loff + field->offset + off);
        break;
    case AST_GVAR:
        emit_gload(field, struc->glabel, field->offset + off);
        break;
    case AST_STRUCT_REF:
        emit_load_struct_ref(struc->struc, field, struc->ty->offset + off);
        break;
    case AST_DEREF:
        emit_expr(struc->operand);
        emit_lload(field, "x0", field->offset + off);
        break;
    default:
        error("internal error: %s", node2s(struc));
    }
}

static void emit_store(Node *var) {
    SAVE;
    switch (var->kind) {
    case AST_DEREF: emit_assign_deref(var); break;
    case AST_STRUCT_REF: emit_assign_struct_ref(var->struc, var->ty, 0); break;
    case AST_LVAR:
        ensure_lvar_init(var);
        emit_lsave(var->ty, var->loff);
        break;
    case AST_GVAR: emit_gsave(var->glabel, var->ty, 0); break;
    default: error("internal error");
    }
}

static void emit_to_bool(Type *ty) {
    SAVE;
    if (is_flotype(ty)) {
        push_xmm(1);
        emit("xorpd #xmm1, #xmm1");
        emit("%s #xmm1, #xmm0", (ty->kind == KIND_FLOAT) ? "ucomiss" : "ucomisd");
        emit("setne x29");
        pop_xmm(1);
    } else {
        emit("cmp $0, x0");
        emit("setne x29");
    }
    emit("movzb x29, x0");
}

static void emit_comp(char *inst, char *usiginst, Node *node) {
    SAVE;
    if (is_flotype(node->left->ty)) {
        emit_expr(node->left);
        push_xmm(0);
        emit_expr(node->right);
        pop_xmm(1);
        if (node->left->ty->kind == KIND_FLOAT)
            emit("ucomiss #xmm0, #xmm1");
        else
            emit("ucomisd #xmm0, #xmm1");
    } else {
        emit_expr(node->left);
        push("x0");
        emit_expr(node->right);
        pop("x2");
        int kind = node->left->ty->kind;
        if (kind == KIND_LONG || kind == KIND_LLONG)
          emit("cmp x0, x2");
        else
          emit("cmp x0, x2");
    }
    if (is_flotype(node->left->ty) || node->left->ty->usig)
        emit("%s x29", usiginst);
    else
        emit("%s x29", inst);
    emit("movzb x29, x0");
}

static void emit_binop_int_arith(Node *node) {
    SAVE;
    char *op = NULL;
    switch (node->kind) {
    case '+': op = "add"; break;
    case '-': op = "sub"; break;
    case '*': op = "imul"; break;
    case '^': op = "xor"; break;
    case OP_SAL: op = "sal"; break;
    case OP_SAR: op = "sar"; break;
    case OP_SHR: op = "shr"; break;
    case '/': case '%': break;
    default: error("invalid operator '%d'", node->kind);
    }
    emit_expr(node->left);
    push("x0");
    emit_expr(node->right);
    emit("mov x0, x2");
    pop("x0");
    if (node->kind == '/' || node->kind == '%') {
        if (node->ty->usig) {
          emit("xor #edx, #edx");
          emit("div x2");
        } else {
          emit("cqto");
          emit("idiv x2");
        }
        if (node->kind == '%')
            emit("mov #edx, x0");
    } else if (node->kind == OP_SAL || node->kind == OP_SAR || node->kind == OP_SHR) {
        emit("%s #cl, %s", op, get_int_reg(node->left->ty, 'a'));
    } else {
        emit("%s x2, x0", op);
    }
}

static void emit_binop_float_arith(Node *node) {
    SAVE;
    char *op;
    bool isdouble = (node->ty->kind == KIND_DOUBLE);
    switch (node->kind) {
    case '+': op = (isdouble ? "addsd" : "addss"); break;
    case '-': op = (isdouble ? "subsd" : "subss"); break;
    case '*': op = (isdouble ? "mulsd" : "mulss"); break;
    case '/': op = (isdouble ? "divsd" : "divss"); break;
    default: error("invalid operator '%d'", node->kind);
    }
    emit_expr(node->left);
    push_xmm(0);
    emit_expr(node->right);
    emit("%s #xmm0, #xmm1", (isdouble ? "movsd" : "movss"));
    pop_xmm(0);
    emit("%s #xmm1, #xmm0", op);
}

static void emit_load_convert(Type *to, Type *from) {
    SAVE;
    if (is_inttype(from) && to->kind == KIND_FLOAT)
        emit("cvtsi2ss x0, #xmm0");
    else if (is_inttype(from) && to->kind == KIND_DOUBLE)
        emit("cvtsi2sd x0, #xmm0");
    else if (from->kind == KIND_FLOAT && to->kind == KIND_DOUBLE)
        emit("cvtps2pd #xmm0, #xmm0");
    else if ((from->kind == KIND_DOUBLE || from->kind == KIND_LDOUBLE) && to->kind == KIND_FLOAT)
        emit("cvtpd2ps #xmm0, #xmm0");
    else if (to->kind == KIND_BOOL)
        emit_to_bool(from);
    else if (is_inttype(from) && is_inttype(to))
        emit_intcast(from);
    else if (is_inttype(to))
        emit_toint(from);
}

static void emit_ret() {
    SAVE;
    emit("mov x2, sp");
    pop("x2");
    emit("ret");
}

static void emit_binop(Node *node) {
    SAVE;
    if (node->ty->kind == KIND_PTR) {
        emit_pointer_arith(node->kind, node->left, node->right);
        return;
    }
    switch (node->kind) {
    case '<': emit_comp("setl", "setb", node); return;
    case OP_EQ: emit_comp("sete", "sete", node); return;
    case OP_LE: emit_comp("setle", "setna", node); return;
    case OP_NE: emit_comp("setne", "setne", node); return;
    }
    if (is_inttype(node->ty))
        emit_binop_int_arith(node);
    else if (is_flotype(node->ty))
        emit_binop_float_arith(node);
    else
        error("internal error: %s", node2s(node));
}

static void emit_save_literal(Node *node, Type *totype, int off) {
    switch (totype->kind) {
    case KIND_BOOL:  emit("movb %d, %d(x2)", !!node->ival, off); break;
    case KIND_CHAR:  emit("movb %d, %d(x2)", node->ival, off); break;
    case KIND_SHORT: emit("movw %d, %d(x2)", node->ival, off); break;
    case KIND_INT:   emit("movl %d, %d(x2)", node->ival, off); break;
    case KIND_LONG:
    case KIND_LLONG:
    case KIND_PTR: {
        emit("movl $%lu, %d(x2)", ((uint64_t)node->ival) & ((1L << 32) - 1), off);
        emit("movl $%lu, %d(x2)", ((uint64_t)node->ival) >> 32, off + 4);
        break;
    }
    case KIND_FLOAT: {
        float fval = node->fval;
        emit("movl $%u, %d(x2)", *(uint32_t *)&fval, off);
        break;
    }
    case KIND_DOUBLE:
    case KIND_LDOUBLE: {
        emit("movl $%lu, %d(x2)", *(uint64_t *)&node->fval & ((1L << 32) - 1), off);
        emit("movl $%lu, %d(x2)", *(uint64_t *)&node->fval >> 32, off + 4);
        break;
    }
    default:
        error("internal error: <%s> <%s> <%d>", node2s(node), ty2s(totype), off);
    }
}

static void emit_addr(Node *node) {
    switch (node->kind) {
    case AST_LVAR:
        ensure_lvar_init(node);
        emit("lea %d(x2), x0", node->loff);
        break;
    case AST_GVAR:
        emit("lea %s(x31), x0", node->glabel);
        break;
    case AST_DEREF:
        emit_expr(node->operand);
        break;
    case AST_STRUCT_REF:
        emit_addr(node->struc);
        emit("add %d, x0", node->ty->offset);
        break;
    case AST_FUNCDESG:
        emit("lea %s(x31), x0", node->fname);
        break;
    default:
        error("internal error: %s", node2s(node));
    }
}

static void emit_copy_struct(Node *left, Node *right) {
    push("x2");
    push("r11");
    emit_addr(right);
    emit("mov x0, x2");
    emit_addr(left);
    int i = 0;
    for (; i < left->ty->size; i += 8) {
        emit("movq %d(x2), x5", i);
        emit("movq x5, %d(x0)", i);
    }
    for (; i < left->ty->size; i += 4) {
        emit("movl %d(x2), x5", i);
        emit("movl x5, %d(x0)", i);
    }
    for (; i < left->ty->size; i++) {
        emit("movb %d(x2), x5", i);
        emit("movb x5, %d(x0)", i);
    }
    pop("x11");
    pop("x2");
}

static int cmpinit(const void *x, const void *y) {
    Node *a = *(Node **)x;
    Node *b = *(Node **)y;
    return a->initoff - b->initoff;
}

static void emit_fill_holes(Vector *inits, int off, int totalsize) {
    // If at least one of the fields in a variable are initialized,
    // unspecified fields has to be initialized with 0.
    int len = vec_len(inits);
    Node **buf = malloc(len * sizeof(Node *));
    for (int i = 0; i < len; i++)
        buf[i] = vec_get(inits, i);
    qsort(buf, len, sizeof(Node *), cmpinit);

    int lastend = 0;
    for (int i = 0; i < len; i++) {
        Node *node = buf[i];
        if (lastend < node->initoff)
            emit_zero_filler(lastend + off, node->initoff + off);
        lastend = node->initoff + node->totype->size;
    }
    emit_zero_filler(lastend + off, totalsize + off);
}

static void emit_decl_init(Vector *inits, int off, int totalsize) {
    emit_fill_holes(inits, off, totalsize);
    for (int i = 0; i < vec_len(inits); i++) {
        Node *node = vec_get(inits, i);
        assert(node->kind == AST_INIT);
        bool isbitfield = (node->totype->bitsize > 0);
        if (node->initval->kind == AST_LITERAL && !isbitfield) {
            emit_save_literal(node->initval, node->totype, node->initoff + off);
        } else {
            emit_expr(node->initval);
            emit_lsave(node->totype, node->initoff + off);
        }
    }
}

static void emit_pre_inc_dec(Node *node, char *op) {
    emit_expr(node->operand);
    emit("%s %d, x0", op, node->ty->ptr ? node->ty->ptr->size : 1);
    emit_store(node->operand);
}

static void emit_post_inc_dec(Node *node, char *op) {
    SAVE;
    emit_expr(node->operand);
    push("x0");
    emit("%s %d, x0", op, node->ty->ptr ? node->ty->ptr->size : 1);
    emit_store(node->operand);
    pop("x0");
}

static void set_reg_nums(Vector *args) {
    numgp = numfp = 0;
    for (int i = 0; i < vec_len(args); i++) {
        Node *arg = vec_get(args, i);
        if (is_flotype(arg->ty))
            numfp++;
        else
            numgp++;
    }
}

static void emit_je(char *label) {
    emit("test x0, x0");
    emit("je %s", label);
}

static void emit_label(char *label) {
    emit("%s:", label);
}

static void emit_jmp(char *label) {
    emit("jmp %s", label);
}

static void emit_literal(Node *node) {
    SAVE;
    switch (node->ty->kind) {
    case KIND_BOOL:
    case KIND_CHAR:
    case KIND_SHORT:
        emit("mov $%u, x0", node->ival);
        break;
    case KIND_INT:
        emit("mov $%u, x0", node->ival);
        break;
    case KIND_LONG:
    case KIND_LLONG: {
        emit("mov $%lu, x0", node->ival);
        break;
    }
    case KIND_FLOAT: {
        if (!node->flabel) {
            node->flabel = make_label();
            float fval = node->fval;
            emit_noindent(".data");
            emit_label(node->flabel);
            emit(".long %d", *(uint32_t *)&fval);
            emit_noindent(".text");
        }
        emit("movss %s(x31), #xmm0", node->flabel);
        break;
    }
    case KIND_DOUBLE:
    case KIND_LDOUBLE: {
        if (!node->flabel) {
            node->flabel = make_label();
            emit_noindent(".data");
            emit_label(node->flabel);
            emit(".quad %lu", *(uint64_t *)&node->fval);
            emit_noindent(".text");
        }
        emit("movsd %s(x31), #xmm0", node->flabel);
        break;
    }
    case KIND_ARRAY: {
        if (!node->slabel) {
            node->slabel = make_label();
            emit_noindent(".data");
            emit_label(node->slabel);
            emit(".string \"%s\"", quote_cstring_len(node->sval, node->ty->size - 1));
            emit_noindent(".text");
        }
        emit("lea %s(x31), x0", node->slabel);
        break;
    }
    default:
        error("internal error");
    }
}

static char **split(char *buf) {
    char *p = buf;
    int len = 1;
    while (*p) {
        if (p[0] == '\r' && p[1] == '\n') {
            len++;
            p += 2;
            continue;
        }
        if (p[0] == '\r' || p[0] == '\n')
            len++;
        p++;
    }
    p = buf;
    char **r = malloc(sizeof(char *) * len + 1);
    int i = 0;
    while (*p) {
        if (p[0] == '\r' && p[1] == '\n') {
            p[0] = '\0';
            p += 2;
            r[i++] = p;
            continue;
        }
        if (p[0] == '\r' || p[0] == '\n') {
            p[0] = '\0';
            r[i++] = p + 1;
        }
        p++;
    }
    r[i] = NULL;
    return r;
}

static char **read_source_file(char *file) {
    FILE *fp = fopen(file, "r");
    if (!fp)
        return NULL;
    struct stat st;
    fstat(fileno(fp), &st);
    char *buf = malloc(st.st_size + 1);
    if (fread(buf, 1, st.st_size, fp) != st.st_size) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    buf[st.st_size] = '\0';
    return split(buf);
}

static void maybe_print_source_line(char *file, int line) {
    if (!dumpsource)
        return;
    char **lines = map_get(source_lines, file);
    if (!lines) {
        lines = read_source_file(file);
        if (!lines)
            return;
        map_put(source_lines, file, lines);
    }
    int len = 0;
    for (char **p = lines; *p; p++)
        len++;
    emit_nostack("# %s", lines[line - 1]);
}

static void maybe_print_source_loc(Node *node) {
    if (!node->sourceLoc)
        return;
    char *file = node->sourceLoc->file;
    long fileno = (long)map_get(source_files, file);
    if (!fileno) {
        fileno = map_len(source_files) + 1;
        map_put(source_files, file, (void *)fileno);

    }
    char *loc = format(".loc %ld %d 0", fileno, node->sourceLoc->line);
    if (strcmp(loc, last_loc)) {
        maybe_print_source_line(file, node->sourceLoc->line);
    }
    last_loc = loc;
}

static void emit_lvar(Node *node) {
    SAVE;
    ensure_lvar_init(node);
    emit_lload(node->ty, "x2", node->loff);
}

static void emit_gvar(Node *node) {
    SAVE;
    emit_gload(node->ty, node->glabel, 0);
}

static void emit_builtin_return_address(Node *node) {
    push("r11");
    assert(vec_len(node->args) == 1);
    emit_expr(vec_head(node->args));
    char *loop = make_label();
    char *end = make_label();
    emit("mov x2, x5");
    emit_label(loop);
    emit("test x0, x0");
    emit("jz %s", end);
    emit("mov (x5), x5");
    emit("sub $1, x0");
    emit_jmp(loop);
    emit_label(end);
    emit("mov 8(x5), x0");
    pop("x11");
}

// Set the register class for parameter passing to x0.
// 0 is INTEGER, 1 is SSE, 2 is MEMORY.
static void emit_builtin_reg_class(Node *node) {
    Node *arg = vec_get(node->args, 0);
    assert(arg->ty->kind == KIND_PTR);
    Type *ty = arg->ty->ptr;
    if (ty->kind == KIND_STRUCT)
        emit("mov $2, x0");
    else if (is_flotype(ty))
        emit("mov $1, x0");
    else
        emit("mov $0, x0");
}

static void emit_builtin_va_start(Node *node) {
    SAVE;
    assert(vec_len(node->args) == 1);
    emit_expr(vec_head(node->args));
    push("x2");
    emit("movl %d, (x0)", numgp * 8);
    emit("movl %d, 4(x0)", 48 + numfp * 16);
    emit("lea %d(x2), x2", -REGAREA_SIZE);
    emit("mov x2, 16(x0)");
    pop("x2");
}

static bool maybe_emit_builtin(Node *node) {
    SAVE;
    if (!strcmp("__builtin_return_address", node->fname)) {
        emit_builtin_return_address(node);
        return true;
    }
    if (!strcmp("__builtin_reg_class", node->fname)) {
        emit_builtin_reg_class(node);
        return true;
    }
    if (!strcmp("__builtin_va_start", node->fname)) {
        emit_builtin_va_start(node);
        return true;
    }
    return false;
}

static void classify_args(Vector *ints, Vector *floats, Vector *rest, Vector *args) {
    SAVE;
    int ireg = 0, xreg = 0;
    int imax = 6, xmax = 8;
    for (int i = 0; i < vec_len(args); i++) {
        Node *v = vec_get(args, i);
        if (v->ty->kind == KIND_STRUCT)
            vec_push(rest, v);
        else if (is_flotype(v->ty))
            vec_push((xreg++ < xmax) ? floats : rest, v);
        else
            vec_push((ireg++ < imax) ? ints : rest, v);
    }
}

static void save_arg_regs(int nints, int nfloats) {
    SAVE;
    assert(nints <= 6);
    assert(nfloats <= 8);
    for (int i = 0; i < nints; i++)
        push(REGS[i]);
}

static void restore_arg_regs(int nints, int nfloats) {
    SAVE;
    for (int i = nfloats - 1; i > 0; i--)
        pop_xmm(i);
    for (int i = nints - 1; i >= 0; i--)
        pop(REGS[i]);
}

static int emit_args(Vector *vals) {
    SAVE;
    int r = 0;
    for (int i = 0; i < vec_len(vals); i++) {
        Node *v = vec_get(vals, i);
        if (v->ty->kind == KIND_STRUCT) {
            emit_addr(v);
            r += push_struct(v->ty->size);
        } else if (is_flotype(v->ty)) {
            emit_expr(v);
            push_xmm(0);
            r += 8;
        } else {
            emit_expr(v);
            push("x0");
            r += 8;
        }
    }
    return r;
}

static void pop_int_args(int nints) {
    SAVE;
    for (int i = nints - 1; i >= 0; i--)
        pop(REGS[i]);
}

static void pop_float_args(int nfloats) {
    SAVE;
    for (int i = nfloats - 1; i >= 0; i--)
        pop_xmm(i);
}

static void maybe_booleanize_retval(Type *ty) {
    if (ty->kind == KIND_BOOL) {
        emit("movzx x29, x0");
    }
}

static void emit_func_call(Node *node) {
    SAVE;
    int opos = stackpos;
    bool isptr = (node->kind == AST_FUNCPTR_CALL);
    Type *ftype = isptr ? node->fptr->ty->ptr : node->ftype;

    Vector *ints = make_vector();
    Vector *floats = make_vector();
    Vector *rest = make_vector();
    classify_args(ints, floats, rest, node->args);
    save_arg_regs(vec_len(ints), vec_len(floats));

    bool padding = stackpos % 16;
    if (padding) {
        emit("sub $8, sp");
        stackpos += 8;
    }

    int restsize = emit_args(vec_reverse(rest));
    if (isptr) {
        emit_expr(node->fptr);
        push("x0");
    }
    emit_args(ints);
    emit_args(floats);
    pop_float_args(vec_len(floats));
    pop_int_args(vec_len(ints));

    if (isptr) pop("x11");
    if (ftype->hasva)
        emit("mov $%u, x0", vec_len(floats));

    if (isptr)
        emit("call *x5");
    else
        emit("call %s", node->fname);
    maybe_booleanize_retval(node->ty);
    if (restsize > 0) {
        emit("add %d, sp", restsize);
        stackpos -= restsize;
    }
    if (padding) {
        emit("add $8, sp");
        stackpos -= 8;
    }
    restore_arg_regs(vec_len(ints), vec_len(floats));
    assert(opos == stackpos);
}

static void emit_decl(Node *node) {
    SAVE;
    if (!node->declinit)
        return;
    emit_decl_init(node->declinit, node->declvar->loff, node->declvar->ty->size);
}

static void emit_conv(Node *node) {
    SAVE;
    emit_expr(node->operand);
    emit_load_convert(node->ty, node->operand->ty);
}

static void emit_deref(Node *node) {
    SAVE;
    emit_expr(node->operand);
    emit_lload(node->operand->ty->ptr, "x0", 0);
    emit_load_convert(node->ty, node->operand->ty->ptr);
}

static void emit_ternary(Node *node) {
    SAVE;
    emit_expr(node->cond);
    char *ne = make_label();
    emit_je(ne);
    if (node->then)
        emit_expr(node->then);
    if (node->els) {
        char *end = make_label();
        emit_jmp(end);
        emit_label(ne);
        emit_expr(node->els);
        emit_label(end);
    } else {
        emit_label(ne);
    }
}

static void emit_goto(Node *node) {
    SAVE;
    assert(node->newlabel);
    emit_jmp(node->newlabel);
}

static void emit_return(Node *node) {
    SAVE;
    if (node->retval) {
        emit_expr(node->retval);
        maybe_booleanize_retval(node->retval->ty);
    }
    emit_ret();
}

static void emit_compound_stmt(Node *node) {
    SAVE;
    for (int i = 0; i < vec_len(node->stmts); i++)
        emit_expr(vec_get(node->stmts, i));
}

static void emit_logand(Node *node) {
    SAVE;
    char *end = make_label();
    emit_expr(node->left);
    emit("test x0, x0");
    emit("mov $0, x0");
    emit("je %s", end);
    emit_expr(node->right);
    emit("test x0, x0");
    emit("mov $0, x0");
    emit("je %s", end);
    emit("mov $1, x0");
    emit_label(end);
}

static void emit_logor(Node *node) {
    SAVE;
    char *end = make_label();
    emit_expr(node->left);
    emit("test x0, x0");
    emit("mov $1, x0");
    emit("jne %s", end);
    emit_expr(node->right);
    emit("test x0, x0");
    emit("mov $1, x0");
    emit("jne %s", end);
    emit("mov $0, x0");
    emit_label(end);
}

static void emit_lognot(Node *node) {
    SAVE;
    emit_expr(node->operand);
    emit("cmp $0, x0");
    emit("sete x29");
    emit("movzb x29, x0");
}

static void emit_bitand(Node *node) {
    SAVE;
    emit_expr(node->left);
    push("x0");
    emit_expr(node->right);
    pop("x2");
    emit("and x2, x0");
}

static void emit_bitor(Node *node) {
    SAVE;
    emit_expr(node->left);
    push("x0");
    emit_expr(node->right);
    pop("x2");
    emit("or x2, x0");
}

static void emit_bitnot(Node *node) {
    SAVE;
    emit_expr(node->left);
    emit("not x0");
}

static void emit_cast(Node *node) {
    SAVE;
    emit_expr(node->operand);
    emit_load_convert(node->ty, node->operand->ty);
    return;
}

static void emit_comma(Node *node) {
    SAVE;
    emit_expr(node->left);
    emit_expr(node->right);
}

static void emit_assign(Node *node) {
    SAVE;
    if (node->left->ty->kind == KIND_STRUCT &&
        node->left->ty->size > 8) {
        emit_copy_struct(node->left, node->right);
    } else {
        emit_expr(node->right);
        emit_load_convert(node->ty, node->right->ty);
        emit_store(node->left);
    }
}

static void emit_label_addr(Node *node) {
    SAVE;
    emit("mov $%s, x0", node->newlabel);
}

static void emit_computed_goto(Node *node) {
    SAVE;
    emit_expr(node->operand);
    emit("jmp *x0");
}

static void emit_expr(Node *node) {
    SAVE;
    maybe_print_source_loc(node);
    switch (node->kind) {
    case AST_LITERAL: emit_literal(node); return;
    case AST_LVAR:    emit_lvar(node); return;
    case AST_GVAR:    emit_gvar(node); return;
    case AST_FUNCDESG: emit_addr(node); return;
    case AST_FUNCALL:
        if (maybe_emit_builtin(node))
            return;
        // fall through
    case AST_FUNCPTR_CALL:
        emit_func_call(node);
        return;
    case AST_DECL:    emit_decl(node); return;
    case AST_CONV:    emit_conv(node); return;
    case AST_ADDR:    emit_addr(node->operand); return;
    case AST_DEREF:   emit_deref(node); return;
    case AST_IF:
    case AST_TERNARY:
        emit_ternary(node);
        return;
    case AST_GOTO:    emit_goto(node); return;
    case AST_LABEL:
        if (node->newlabel)
            emit_label(node->newlabel);
        return;
    case AST_RETURN:  emit_return(node); return;
    case AST_COMPOUND_STMT: emit_compound_stmt(node); return;
    case AST_STRUCT_REF:
        emit_load_struct_ref(node->struc, node->ty, 0);
        return;
    case OP_PRE_INC:   emit_pre_inc_dec(node, "add"); return;
    case OP_PRE_DEC:   emit_pre_inc_dec(node, "sub"); return;
    case OP_POST_INC:  emit_post_inc_dec(node, "add"); return;
    case OP_POST_DEC:  emit_post_inc_dec(node, "sub"); return;
    case '!': emit_lognot(node); return;
    case '&': emit_bitand(node); return;
    case '|': emit_bitor(node); return;
    case '~': emit_bitnot(node); return;
    case OP_LOGAND: emit_logand(node); return;
    case OP_LOGOR:  emit_logor(node); return;
    case OP_CAST:   emit_cast(node); return;
    case ',': emit_comma(node); return;
    case '=': emit_assign(node); return;
    case OP_LABEL_ADDR: emit_label_addr(node); return;
    case AST_COMPUTED_GOTO: emit_computed_goto(node); return;
    default:
        emit_binop(node);
    }
}

static void emit_zero(int size) {
    SAVE;
    for (; size >= 8; size -= 8) emit(".quad 0");
    for (; size >= 4; size -= 4) emit(".long 0");
    for (; size > 0; size--)     emit(".byte 0");
}

static void emit_padding(Node *node, int off) {
    SAVE;
    int diff = node->initoff - off;
    assert(diff >= 0);
    emit_zero(diff);
}

static void emit_data_addr(Node *operand, int depth) {
    switch (operand->kind) {
    case AST_LVAR: {
        char *label = make_label();
        emit(".data %d", depth + 1);
        emit_label(label);
        do_emit_data(operand->lvarinit, operand->ty->size, 0, depth + 1);
        emit(".data %d", depth);
        emit(".quad %s", label);
        return;
    }
    case AST_GVAR:
        emit(".quad %s", operand->glabel);
        return;
    default:
        error("internal error");
    }
}

static void emit_data_charptr(char *s, int depth) {
    char *label = make_label();
    emit(".data %d", depth + 1);
    emit_label(label);
    emit(".string \"%s\"", quote_cstring(s));
    emit(".data %d", depth);
    emit(".quad %s", label);
}

static void emit_data_primtype(Type *ty, Node *val, int depth) {
    switch (ty->kind) {
    case KIND_FLOAT: {
        float f = val->fval;
        emit(".long %d", *(uint32_t *)&f);
        break;
    }
    case KIND_DOUBLE:
        emit(".quad %ld", *(uint64_t *)&val->fval);
        break;
    case KIND_BOOL:
        emit(".byte %d", !!eval_intexpr(val, NULL));
        break;
    case KIND_CHAR:
        emit(".byte %d", eval_intexpr(val, NULL));
        break;
    case KIND_SHORT:
        emit(".short %d", eval_intexpr(val, NULL));
        break;
    case KIND_INT:
        emit(".long %d", eval_intexpr(val, NULL));
        break;
    case KIND_LONG:
    case KIND_LLONG:
    case KIND_PTR:
        if (val->kind == OP_LABEL_ADDR) {
            emit(".quad %s", val->newlabel);
            break;
        }
        bool is_char_ptr = (val->operand->ty->kind == KIND_ARRAY && val->operand->ty->ptr->kind == KIND_CHAR);
        if (is_char_ptr) {
            emit_data_charptr(val->operand->sval, depth);
        } else if (val->kind == AST_GVAR) {
            emit(".quad %s", val->glabel);
        } else {
            Node *base = NULL;
            int v = eval_intexpr(val, &base);
            if (base == NULL) {
                emit(".quad %u", v);
                break;
            }
            Type *ty = base->ty;
            if (base->kind == AST_CONV || base->kind == AST_ADDR)
                base = base->operand;
            if (base->kind != AST_GVAR)
                error("global variable expected, but got %s", node2s(base));
            assert(ty->ptr);
            emit(".quad %s+%u", base->glabel, v * ty->ptr->size);
        }
        break;
    default:
        error("don't know how to handle\n  <%s>\n  <%s>", ty2s(ty), node2s(val));
    }
}

static void do_emit_data(Vector *inits, int size, int off, int depth) {
    SAVE;
    for (int i = 0; i < vec_len(inits) && 0 < size; i++) {
        Node *node = vec_get(inits, i);
        Node *v = node->initval;
        emit_padding(node, off);
        if (node->totype->bitsize > 0) {
            assert(node->totype->bitoff == 0);
            long data = eval_intexpr(v, NULL);
            Type *totype = node->totype;
            for (i++ ; i < vec_len(inits); i++) {
                node = vec_get(inits, i);
                if (node->totype->bitsize <= 0) {
                    break;
                }
                v = node->initval;
                totype = node->totype;
                data |= ((((long)1 << totype->bitsize) - 1) & eval_intexpr(v, NULL)) << totype->bitoff;
            }
            emit_data_primtype(totype, &(Node){ AST_LITERAL, totype, .ival = data }, depth);
            off += totype->size;
            size -= totype->size;
            if (i == vec_len(inits))
                break;
        } else {
            off += node->totype->size;
            size -= node->totype->size;
        }
        if (v->kind == AST_ADDR) {
            emit_data_addr(v->operand, depth);
            continue;
        }
        if (v->kind == AST_LVAR && v->lvarinit) {
            do_emit_data(v->lvarinit, v->ty->size, 0, depth);
            continue;
        }
        emit_data_primtype(node->totype, node->initval, depth);
    }
    emit_zero(size);
}

static void emit_data(Node *v, int off, int depth) {
    SAVE;
    emit(".data %d", depth);
    if (!v->declvar->ty->isstatic)
        emit_noindent(".global %s", v->declvar->glabel);
    emit_noindent("%s:", v->declvar->glabel);
    do_emit_data(v->declinit, v->declvar->ty->size, off, depth);
}

static void emit_bss(Node *v) {
    SAVE;
    emit(".data");
    if (!v->declvar->ty->isstatic)
        emit(".global %s", v->declvar->glabel);
    emit(".lcomm %s, %d", v->declvar->glabel, v->declvar->ty->size);
}

static void emit_global_var(Node *v) {
    SAVE;
    if (v->declinit)
        emit_data(v, 0, 0);
    else
        emit_bss(v);
}

static int emit_regsave_area() {
    emit("sub %d, sp", REGAREA_SIZE);
    emit("mov x3, (sp)");
    emit("mov x7, 8(sp)");
    emit("mov x4, 16(sp)");
    emit("mov x2, 24(sp)");
    emit("mov x7, 32(sp)");
    emit("mov x6, 40(sp)");
    emit("movaps #xmm0, 48(sp)");
    emit("movaps #xmm1, 64(sp)");
    emit("movaps #xmm2, 80(sp)");
    emit("movaps #xmm3, 96(sp)");
    emit("movaps #xmm4, 112(sp)");
    emit("movaps #xmm5, 128(sp)");
    emit("movaps #xmm6, 144(sp)");
    emit("movaps #xmm7, 160(sp)");
    return REGAREA_SIZE;
}

static void push_func_params(Vector *params, int off) {
    int ireg = 0;
    int xreg = 0;
    int arg = 2;
    for (int i = 0; i < vec_len(params); i++) {
        Node *v = vec_get(params, i);
        if (v->ty->kind == KIND_STRUCT) {
            emit("lea %d(x2), x0", arg * 8);
            int size = push_struct(v->ty->size);
            off -= size;
            arg += size / 8;
        } else if (is_flotype(v->ty)) {
            if (xreg >= 8) {
                emit("mov %d(x2), x0", arg++ * 8);
                push("x0");
            } else {
                push_xmm(xreg++);
            }
            off -= 8;
        } else {
            if (ireg >= 6) {
                if (v->ty->kind == KIND_BOOL) {
                    emit("mov %d(x2), x29", arg++ * 8);
                    emit("movzb x29, x0");
                } else {
                    emit("mov %d(x2), x0", arg++ * 8);
                }
                push("x0");
            } else {
                if (v->ty->kind == KIND_BOOL)
                    emit("movzb %s, %s", SREGS[ireg], MREGS[ireg]);
                push(REGS[ireg++]);
            }
            off -= 8;
        }
        v->loff = off;
    }
}

static void emit_func_prologue(Node *func) {
    SAVE;
    emit(".text");
    if (!func->ty->isstatic)
        emit_noindent(".global %s", func->fname);
    emit_noindent("%s:", func->fname);
    emit("nop");
    push("x2");
    emit("mov sp, x2");
    int off = 0;
    if (func->ty->hasva) {
        set_reg_nums(func->params);
        off -= emit_regsave_area();
    }
    push_func_params(func->params, off);
    off -= vec_len(func->params) * 8;

    int localarea = 0;
    for (int i = 0; i < vec_len(func->localvars); i++) {
        Node *v = vec_get(func->localvars, i);
        int size = align(v->ty->size, 8);
        assert(size % 8 == 0);
        off -= size;
        v->loff = off;
        localarea += size;
    }
    if (localarea) {
        emit("sub %d, sp", localarea);
        stackpos += localarea;
    }
}

void emit_toplevel(Node *v) {
    stackpos = 8;
    if (v->kind == AST_FUNC) {
        emit_func_prologue(v);
        emit_expr(v->body);
        // emit_ret();
    } else if (v->kind == AST_DECL) {
        emit_global_var(v);
    } else {
        error("internal error");
    }
}
