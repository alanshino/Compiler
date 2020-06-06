#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory.h>

int token;            // current token
char *src;            // pointer to source code string
char *old_src;
int line;             // line number
int tktype;           // get token type
int poolsize;         // default size

// TOKENS AND CLASSES (OPERATORS LAST AND IN PRECEDENCE ORDER)
enum {
    NUM = 128, FUN, SYS, GLO, LOC, ID,
    CHAR, ELSE, VOID, IF, INT, RETURN, SIZEOF, WHILE,
    ASSIGN, COND, LOR, LAN, OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD, INC, DEC, BRAK
};

int token_val;        // value of current token (mainly for number)

enum { Char = 1, Int, Ptr };      // support data type

struct symbols {
    int token;
    int hash;       // hash code
    char *name;     // tag string
    int scope;      // function or var(global or local)
    int type;       // data type(int or char or pointer)
    int value;      // store var value
    int gloclass;   // if the same name with global var
    int glotype;
    int glovalue;
    int char_number;
    struct symbols *next;
};

struct symbols *current_id,              // current parsed ID
               *symbol_head,             // symbol table
               *symbol_step,
               *current_func,            // current function position
               *current_function;

struct keyword_symbols {
    int token;
    int hash;
    char *name;
};

struct keyword_symbols key_symbols[8];
//struct keyword_symbols *current_keysymbols;
void print_var_name(struct symbols *current);
int parameter;

void keyword()
{
    int hash = 0;
    int i = 0;
    int id;
    char *last_pos;
    src = "char else void if int return sizeof while";
    id = CHAR;
    last_pos = src;
    while (*src) {
        if (*src != ' ') {
            hash = hash * 18 + *src;
        } else {
            key_symbols[i].token = id;
            key_symbols[i].hash = hash;
            key_symbols[i].name = last_pos;
            last_pos = src + 1;
            hash = 0;
            i++;
            id++;
        }
        src++;
    }
    key_symbols[i].token = id;
    key_symbols[i].hash = hash;
    key_symbols[i].name = last_pos + 1;
    return ;
}

void next()
{
    int hash;
    int i;
    char *last_pos;
    while ((token = *src)) {
        src++;
        if (token == '\n') {
            ++line;
        } else if (token == ' ') {
            ;
        } else if (token == '#') {
            // skip macro, because we will not support it
            while (*src != 0 && *src != '\n') {
                src++;
            }
        } else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')) {
            last_pos = src - 1;
            hash = token;
            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')) {
                hash = hash * 18 + *src;
                src++;
            }
            i = 0;
            while (i < 8) {
                if (key_symbols[i].hash == hash && !memcmp(key_symbols[i].name, last_pos, src - last_pos)) {
                    //found one, return
                    token = key_symbols[i].token;
                    return;
                }
                i++;
            }
            if (symbol_head) {
                current_id = symbol_head;
                while (current_id) {
                    if (current_id->hash == hash && !memcmp(current_id->name, last_pos, src - last_pos)) {
                        token = current_id->token;
                        return ;
                    }
                    current_id = current_id->next;
                }
            }
            // new symbol ID
            if (symbol_head == NULL) {
                symbol_head = current_id = malloc(sizeof(struct symbols));
                symbol_head->hash = hash;
                symbol_head->name = last_pos;
                symbol_head->char_number = src - last_pos;
                symbol_head->scope = 0;
                token = symbol_head->token = ID;
                symbol_head->next = NULL;
                current_id->type = 0;
                symbol_step = symbol_head;
            } else {
                current_id = malloc(sizeof(struct symbols));
                symbol_step->next = current_id;
                current_id->next = NULL;
                symbol_step = current_id;
                current_id->name = last_pos;
                current_id->char_number = src - last_pos;
                current_id->hash = hash;
                current_id->type = 0;
                current_id->scope = 0;
                token = current_id->token = ID;
            }
            return ;
        } else if (token >= '0' && token <= '9') {
            token_val = token - '0';
            while (*src >= '0' && *src <= '9') {
                token_val = token_val*10 + *src++ - '0';
            }
            token = NUM;
            return;
        } else if (token == '/') {
            if (*src == '/') {
                // skip comments
                while (*src != 0 && *src != '\n') {
                    ++src;
                }
            } else {
                // divide operator
                token = DIV;
                return;
            }
        } else if (token == '=') {
            // parse '==' and '='
            if (*src == '=') {
                src++;
                token = EQ;
            } else {
                token = ASSIGN;
            }
            return;
        } else if (token == '+') {
            // parse '+' and '++'
            if (*src == '+') {
                src++;
                token = INC;
            } else {
                token = ADD;
            }
            return;
        } else if (token == '-') {
            // parse '-' and '--'
            if (*src == '-') {
                src ++;
                token = DEC;
            } else {
                token = SUB;
            }
            return;
        } else if (token == '!') {
            // parse '!='
            if (*src == '=') {
                src++;
                token = NE;
            }
            return;
        } else if (token == '<') {
            // parse '<=', '<<' or '<'
            if (*src == '=') {
                src++;
                token = LE;
            } else if (*src == '<') {
                src++;
                token = SHL;
            } else {
                token = LT;
            }
            return;
        } else if (token == '>') {
            // parse '>=', '>>' or '>'
            if (*src == '=') {
                src++;
                token = GE;
            } else if (*src == '>') {
                src++;
                token = SHR;
            } else {
                token = GT;
            }
            return;
        } else if (token == '|') {
            // parse '|' or '||'
            if (*src == '|') {
                src++;
                token = LOR;
            } else {
                token = OR;
            }
            return;
        } else if (token == '&') {
            // parse '&' and '&&'
            if (*src == '&') {
                src++;
                token = LAN;
            } else {
                token = AND;
            }
            return;
        } else if (token == '^') {
            token = XOR;
            return;
        } else if (token == '%') {
            token = MOD;
            return;
        } else if (token == '*') {
            token = MUL;
            return;
        } else if (token == '[') {
            token = BRAK;
            return;
        } else if (token == '?') {
            token = COND;
            return;
        } else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':') {
            // directly return the character as token;
            return;
        }
    }
    return ;
}

void match(int tk)
{
    if (token == tk) {
        next();
    } else {
        printf("%d: expected token: %d\n", line, tk);
        exit(-1);
    }
}

void statement()
{
    while (token != '}') {
        if (token == INT || token == CHAR) {
            printf("%d: declaration error\n", line);
            exit(1);
        }
        if (token == ID) {  // Lvalue
            if (!(current_id->scope)) {
                printf("%d: error use undeclared variable\n", line);
                exit(1);
            }
            match(ID);
            if (token == ASSIGN) { // Assignment
                match(ASSIGN);
                if (token == NUM) {
                    current_id->value = token_val;
                    match(NUM);
                } else if (token == ID) {
                    current_id->value = current_id->value;
                    match(ID);
                } else {
                    printf("%d: assignment error\n", line);
                    exit(1);
                }
            } else if (token == INC) {
                current_id->value++;
                match(INC);
            } else if (token == DEC) {
                current_id->value--;
                match(DEC);
            }
        }
        match(';');
    }
    return ;
}

void local_declaration()
{
    while (token == INT || token == CHAR) {
        tktype = (token == INT)?Int:Char;
        match(token);
        if (token == ID) {
            parameter++;
            if (current_id->scope) {
                printf("%d: duplicate local declaration\n", line);
                exit(1);
            }
            current_id->type = tktype;
            current_id->scope = LOC;
            match(ID);
        } else {
            printf("%d: function parameter declaration error\n", line);
            exit(1);
        }
        while (token == ',') {
            match(',');
            if (token == ID) {
                if (current_id->scope) {
                    printf("%d: duplicate local declaration\n", line);
                    exit(1);
                }
                current_id->type = tktype;
                current_id->scope = LOC;
                match(ID);
                parameter++;
            } else {
                printf("%d: declaration error\n", line);
                exit(1);
            }
        }
        match(';');
    }
    return ;
}

void function_parameter()
{
    while (token != ')') {
        if (token == INT || token == CHAR) {
            tktype = (token == INT)?Int:Char;
            match(token);
            if (token == ID) {
                parameter++;
                if (current_id->scope) {
                    printf("%d: duplicate local declaration\n", line);
                    exit(1);
                }
                current_id->type = tktype;
                current_id->scope = LOC;
                match(ID);
            } else {
                printf("%d: function parameter declaration error\n", line);
                exit(1);
            }
        }
        if (token == ',') {
            match(',');
            if (token == INT || token == CHAR) {
                tktype = (token == INT)?Int:Char;
                match(token);
            } else if (token == ID) {
                if (current_id->scope) {
                    printf("%d: duplicate local declaration\n", line);
                    exit(1);
                }
                current_id->scope = LOC;
                current_id->type = tktype;
                match(ID);
                parameter++;
            } else {
                printf("%d: function parameter declaration error\n", line);
                exit(1);
            }
        } else {
            if (token != ')') {
                printf("%d: expected declaration specifiers or '...' before ')'\n", line);
                exit(1);
            }
        }
    }
    return ;
}

void function_declaration()            // function declaration
{
    parameter = 0;
    match('(');
    function_parameter();
    match(')');
    match('{');
    local_declaration();
    current_function->value = parameter;
    parameter = 0;
    statement();
    match('}');
    current_function->scope = FUN;
    return ;
}

void global_declaration()              // global variable declaration
{
    match(token);
    do {
        if (token == ID) {
            if (current_id->scope) {
                printf("%d: duplicate global declaration\n", line);
                exit(-1);
            }
            current_id->type = tktype;
            current_id->scope = GLO;
            match(ID);
            if (token == '(') {
                current_function = current_id;
                function_declaration();
                return ;
            }
            if (token == ',') {
                match(',');
            } else if (token != ';') {
                printf("%d: error expected '=', ',', ';', 'asm' or '__attribute__'\n", line);
                exit(1);
            }
        } else {
            printf("%d: useless type name in empty declaration\n", line);
            exit(1);
        }
    } while (token != ';');
    match(';');
    tktype = 0;
    return ;
}

void program()
{
    // get next token
    next();
    while (token > 0) {
        switch (token) {
        case INT: case CHAR: case VOID:
            tktype = (token == INT)?Int:Char;
            global_declaration();
            break;
        case ID:
            statement();
            break;
        default:
            printf("%d: syntax error\n", line);
            exit(1);
        }
    }
}

int main(int argc, char **argv)
{
    int i, fd;
    line = 1;

    argv++;
    poolsize = 256 * 1024;
    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }
    keyword();

    if (!(src = old_src = malloc(poolsize))) {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }
    // read the source file
    if ((i = read(fd, src, poolsize-1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0;    // add EOF character
    close(fd);
    program();

    current_func = symbol_head;
    while (current_func) {   // check variable token hash type name and value
        printf("var token = %d\nvar hash = %d\nvar type = %d\nvar value = %d\nvar scope = %d\n",
                current_func->token, current_func->hash, current_func->type, current_func->value, current_func->scope);
        print_var_name(current_func);
        printf("------------------------------------------------------------\n");
        current_func = current_func->next;
    }

    #if 0 // 0
    for (int i = 0; i < 8; i++) {
        printf("%d %s\n", key_symbols[i].hash, key_symbols[i].name);
    }
    #endif
    current_func = symbol_head;
    while (current_func) {
        current_function = current_func;
        current_func = current_func->next;
        free(current_function);
    }
    return 0;
}

void print_var_name(struct symbols *current)
{
    printf("var name = ");
    for (int i = 0; i < current->char_number; i++) {
        printf("%c", current->name[i]);
    }
    printf("\n");
}
