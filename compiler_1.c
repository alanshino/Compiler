#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
// Example (*((void(*)())0))();
void see_identifier_info();
void start_program();
void match(int );
int same_name(char *name1, char *name2, int size_str);
void scanner();
void expression(int);
void statement();
void function_body();
int eval();
void see_identifier_info();
void global_declaration();
void function_declaration();
void function_parameter();
/*

+------------------+
|    stack   |     |      high address
|    ...     v     |
|                  |
|                  |
|                  |
|                  |
|    ...     ^     |
|    heap    |     |
+------------------+
| bss  segment     |
+------------------+
| data segment     |
+------------------+
| text segment     |      low address
+------------------+

*/

/* use on virtual machine */
int *text;            // text segment
int *old_text;        // for dump text segment
int *stack;           // stack
char *data;           // data segment
// virtual machine registers
int *pc;              // program counter point to next instruction address
int *bp;              // base pointer, point to the stack when call function
int *sp;              // stack pointer
int ax;               // register (to safe the expression return value)
int cycle;            // cycle

// support instructions
enum { LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN, READ, CLOS, PRTF, MALC, EXIT  };

// use on compiler
int token;            // current token (scanner output)
char *src;            // pointer to source code string
char *old_src;        // pointer to dump source code string
int poolsize;         // default size of text/data/stack
int line;             // line number

// tokens and classes (operators last and in precedence order)
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// types of variable/function
enum { CHAR, INT, PTR };

// type of declaration.
enum {Global, Local};

/*
    Symbol table:
----+-----+----+----+----+-----+-----+-----+------+------+----
 .. |token|hash|name|type|class|value|btype|bclass|bvalue| ..
----+-----+----+----+----+-----+-----+-----+------+------+----
    |<---       one single identifier                --->|

*/

struct identifier {
    int Token;                     // token of identifier
    int Hash;                      // the hash number use to compare identifier
    char *Name;                    // the identifier name
    int Id_class;                  // the identifier class(Global or local)
    int Type;                      // the identifier data type(int char or pointer)
    int Value;                     // the value of identifier if function save function address
    int Bclass;                    // use to save global var message
    int Btype;                     // . . .
    int Bvalue;                    // . . .
    int char_number;               // symbol name alphabet count
    struct identifier *Next;       // pointer to the next identifier(Linkin List)
};

int token_val;                              // value of current token (mainly for number)
struct identifier *current_id;              // current parsed ID
struct identifier *symbols;                 // symbol table
struct identifier *previous;                // use in linkin list
int basetype;    // the type of a declaration, make it global for convenience
int expr_type;   // the type of an expression
struct identifier *idmain;
// function frame
//
// 0: arg 1
// 1: arg 2
// 2: arg 3
// 3: return address
// 4: old bp pointer  <- index_of_bp
// 5: local var 1
// 6: local var 2
int index_of_bp;  // index of bp pointer on stack

int debug;        // print the executed instructions
int assembly;     // print out the assembly and source
int info;

/*

2 + 3 - 4 * 5
// after step 1, 2
|      |
+------+
| 3    |   |      |
+------+   +------+
| 2    |   | +    |
+------+   +------+

// after step 4
|      |   |      |
+------+   +------+
| 5    |   | -    |
+------+   +------+

// after step 7
|      |
+------+
| 5    |
+------+   +------+
| 4    |   | *    |
+------+   +------+
| 5    |   | -    |
+------+   +------+

*/


/* compare two string same = 0*/
int same_name(char *name1, char *name2, int size_str)
{
    while (size_str) {
        size_str--;
        if (*name1 == *name2) {
            name1++;
            name2++;
        } else {
            return 1;
        }
    }
    return 0;
}

/*
                   +-------+                      +--------+
-- source code --> | lexer | --> token stream --> | parser | --> assembly
                   +-------+                      +--------+
*/


/* scanner the source code string*/
void scanner()
{
    char *last_pos;
    int hash;
    while (token = *src) {
        ++src;
        if (token == ' ') {
            ;
        } else if (token == '\n') {
            if (assembly) {
                // print compile info
                printf("%d: %.*s", line, src-old_src, old_src);
                old_src = src;

                while (old_text < text) {
                    printf("%8.4s", & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                                      "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                                      "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT"[*++old_text * 5]);

                    if (*old_text <= ADJ)
                        printf(" %d\n", *++old_text);
                    else
                        printf("\n");
                }
            }
            ++line;
        } else if (token == '#') {
            // skip macro, because we will not support it
            while (*src != 0 && *src != '\n') {
                src++;
            }
        } else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')) {
            // parse identifier
            last_pos = src - 1;
            hash = token;

            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')) {
                hash = hash * 147 + *src;
                src++;
            }

            // look for existing identifier, linear search
            current_id = symbols;
            if (!current_id) {
                current_id = symbols = previous = calloc(1, sizeof(struct identifier));
                current_id->Next = NULL;
                // store new ID
                current_id->Name = last_pos;
                current_id->Hash = hash;
                token = current_id->Token = Id;
                current_id->char_number = src - last_pos;
            } else {
                while (current_id && current_id->Token) {
                    if (current_id->Hash == hash && !same_name(current_id->Name, last_pos, src - last_pos)) {
                        //found one, return
                        token = current_id->Token;
                        return;
                    }
                    current_id = current_id->Next;
                }
                current_id = calloc(1, sizeof(struct identifier));
                previous->Next = current_id;
                current_id->Name = last_pos;
                current_id->Hash = hash;
                token = current_id->Token = Id;
                current_id->char_number = src - last_pos;
                previous = previous->Next;
            }
            return;
        } else if (token >= '0' && token <= '9') {
            // parse number, three kinds: dec(123) hex(0x123) oct(017)
            token_val = token - '0';
            if (token_val > 0) {
                // dec, starts with [1-9]
                while (*src >= '0' && *src <= '9') {
                    token_val = token_val*10 + *src++ - '0';
                }
            } else {
                // starts with number 0
                if (*src == 'x' || *src == 'X') {
                    //hex
                    token = *++src;
                    while ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F')) {
                        token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
                        token = *++src;
                    }
                } else {
                    // oct
                    while (*src >= '0' && *src <= '7') {
                        token_val = token_val*8 + *src++ - '0';
                    }
                }
            }

            token = Num;
            return;
        } else if (token == '/') {
            if (*src == '/') {
                // skip comments
                while (*src != 0 && *src != '\n') {
                    ++src;
                }
            } else {
                // divide operator
                token = Div;
                return;
            }
        } else if (token == '"' || token == '\'') {
            // parse string literal, currently, the only supported escape
            // character is '\n', store the string literal into data.
            last_pos = data;
            while (*src != 0 && *src != token) {
                token_val = *src++;
                if (token_val == '\\') {
                    // escape character
                    token_val = *src++;
                    if (token_val == 'n') {
                        token_val = '\n';
                    }
                }

                if (token == '"') {
                    *data++ = token_val;
                }
            }

            src++;
            // if it is a single character, return Num token
            if (token == '"') {
                token_val = (int)last_pos;
            } else {
                token = Num;
            }

            return;
        } else if (token == '=') {
            // parse '==' and '='
            if (*src == '=') {
                src ++;
                token = Eq;
            } else {
                token = Assign;
            }
            return;
        } else if (token == '+') {
            // parse '+' and '++'
            if (*src == '+') {
                src ++;
                token = Inc;
            } else {
                token = Add;
            }
            return;
        } else if (token == '-') {
            // parse '-' and '--'
            if (*src == '-') {
                src ++;
                token = Dec;
            } else {
                token = Sub;
            }
            return;
        } else if (token == '!') {
            // parse '!='
            if (*src == '=') {
                src++;
                token = Ne;
            }
            return;
        } else if (token == '<') {
            // parse '<=', '<<' or '<'
            if (*src == '=') {
                src ++;
                token = Le;
            } else if (*src == '<') {
                src ++;
                token = Shl;
            } else {
                token = Lt;
            }
            return;
        } else if (token == '>') {
            // parse '>=', '>>' or '>'
            if (*src == '=') {
                src ++;
                token = Ge;
            } else if (*src == '>') {
                src ++;
                token = Shr;
            } else {
                token = Gt;
            }
            return;
        } else if (token == '|') {
            // parse '|' or '||'
            if (*src == '|') {
                src ++;
                token = Lor;
            } else {
                token = Or;
            }
            return;
        } else if (token == '&') {
            // parse '&' and '&&'
            if (*src == '&') {
                src ++;
                token = Lan;
            } else {
                token = And;
            }
            return;
        } else if (token == '^') {
            token = Xor;
            return;
        } else if (token == '%') {
            token = Mod;
            return;
        } else if (token == '*') {
            token = Mul;
            return;
        } else if (token == '[') {
            token = Brak;
            return;
        } else if (token == '?') {
            token = Cond;
            return;
        } else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':') {
            // directly return the character as token;
            return;
        }
    }
    return ;
}

/*
    <expr> ::= <expr> + <term>
         | <expr> - <term>
         | <term>

    <term> ::= <term> * <factor>
             | <term> / <factor>
             | <factor>

    <factor> ::= ( <expr> )
               | Num

*/
void expression(int level)
{
    // expressions have various format.
    // but majorly can be divided into two parts: unit and operator
    // for example `(char) *a[10] = (int *) func(b > 0 ? 10 : 20);
    // `a[10]` is an unit while `*` is an operator.
    // `func(...)` in total is an unit.
    // so we should first parse those unit and unary operators
    // and then the binary ones
    //
    // also the expression can be in the following types:
    //
    // 1. unit_unary ::= unit | unit unary_op | unary_op unit
    // 2. expr ::= unit_unary (bin_op unit_unary ...)

    // unit_unary()
    struct identifier *id;
    int tmp;
    int *addr;
    {
        if (!token) {
            printf("%d: unexpected token EOF of expression\n", line);
            exit(-1);
        }
        if (token == Num) {
            match(Num);

            // emit code
            *++text = IMM;
            *++text = token_val;
            expr_type = INT;
        }
        else if (token == '"') {
            // continous string "abc" "abc"


            // emit code
            *++text = IMM;
            *++text = token_val;

            match('"');
            // store the rest strings
            while (token == '"') {
                match('"');
            }

            // append the end of string character '\0', all the data are default
            // to 0, so just move data one position forward.
            data = (char *)(((int)data + sizeof(int)) & (-sizeof(int)));
            expr_type = PTR;
        }
        else if (token == Sizeof) {
            // sizeof is actually an unary operator
            // now only `sizeof(int)`, `sizeof(char)` and `sizeof(*...)` are
            // supported.
            match(Sizeof);
            match('(');
            expr_type = INT;

            if (token == Int) {
                match(Int);
            } else if (token == Char) {
                match(Char);
                expr_type = CHAR;
            }

            while (token == Mul) {
                match(Mul);
                expr_type = expr_type + PTR;
            }

            match(')');

            // emit code
            *++text = IMM;
            *++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);

            expr_type = INT;
        }
        else if (token == Id) {
            // there are several type when occurs to Id
            // but this is unit, so it can only be
            // 1. function call
            // 2. Enum variable
            // 3. global/local variable
            match(Id);

            id = current_id;

            if (token == '(') {
                // function call
                match('(');

                // pass in arguments
                tmp = 0; // number of arguments
                while (token != ')') {
                    expression(Assign);
                    *++text = PUSH;
                    tmp ++;

                    if (token == ',') {
                        match(',');
                    }

                }
                match(')');

                // emit code
                if (id->Id_class == Sys) {
                    // system functions
                    *++text = id->Value;
                }
                else if (id->Id_class == Fun) {
                    // function call
                    *++text = CALL;
                    *++text = id->Value;
                }
                else {
                    printf("%d: bad function call\n", line);
                    exit(-1);
                }

                // clean the stack for arguments
                if (tmp > 0) {
                    *++text = ADJ;
                    *++text = tmp;
                }
                expr_type = id->Type;
            }
            else if (id->Id_class == Num) {
                // enum variable
                *++text = IMM;
                *++text = id->Value;
                expr_type = INT;
            }
            else {
                // variable
                if (id->Id_class == Loc) {
                    *++text = LEA;
                    *++text = index_of_bp - id->Value;
                }
                else if (id->Id_class == Glo) {
                    *++text = IMM;
                    *++text = id->Value;
                }
                else {
                    printf("%d: undefined variable\n", line);
                    exit(-1);
                }

                // emit code, default behaviour is to load the value of the
                // address which is stored in `ax`
                expr_type = id->Type;
                *++text = (expr_type == CHAR) ? LC : LI;
            }
        }
        else if (token == '(') {
            // cast or parenthesis
            match('(');
            if (token == Int || token == Char) {
                tmp = (token == Char) ? CHAR : INT; // cast type
                match(token);
                while (token == Mul) {
                    match(Mul);
                    tmp = tmp + PTR;
                }

                match(')');

                expression(Inc); // cast has precedence as Inc(++)

                expr_type  = tmp;
            } else {
                // normal parenthesis
                expression(Assign);
                match(')');
            }
        }
        else if (token == Mul) {
            // dereference *<addr>
            match(Mul);
            expression(Inc); // dereference has the same precedence as Inc(++)

            if (expr_type >= PTR) {
                expr_type = expr_type - PTR;
            } else {
                printf("%d: bad dereference\n", line);
                exit(-1);
            }

            *++text = (expr_type == CHAR) ? LC : LI;
        }
        else if (token == And) {
            // get the address of
            match(And);
            expression(Inc); // get the address of
            if (*text == LC || *text == LI) {
                text --;
            } else {
                printf("%d: bad address of\n", line);
                exit(-1);
            }

            expr_type = expr_type + PTR;
        }
        else if (token == '!') {
            // not
            match('!');
            expression(Inc);

            // emit code, use <expr> == 0
            *++text = PUSH;
            *++text = IMM;
            *++text = 0;
            *++text = EQ;

            expr_type = INT;
        }
        else if (token == '~') {
            // bitwise not
            match('~');
            expression(Inc);

            // emit code, use <expr> XOR -1
            *++text = PUSH;
            *++text = IMM;
            *++text = -1;
            *++text = XOR;

            expr_type = INT;
        }
        else if (token == Add) {
            // +var, do nothing
            match(Add);
            expression(Inc);

            expr_type = INT;
        }
        else if (token == Sub) {
            // -var
            match(Sub);

            if (token == Num) {
                *++text = IMM;
                *++text = -token_val;
                match(Num);
            } else {

                *++text = IMM;
                *++text = -1;
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
            }

            expr_type = INT;
        }
        else if (token == Inc || token == Dec) {
            tmp = token;
            match(token);
            expression(Inc);
            if (*text == LC) {
                *text = PUSH;  // to duplicate the address
                *++text = LC;
            } else if (*text == LI) {
                *text = PUSH;
                *++text = LI;
            } else {
                printf("%d: bad lvalue of pre-increment\n", line);
                exit(-1);
            }
            *++text = PUSH;
            *++text = IMM;
            *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text = (tmp == Inc) ? ADD : SUB;
            *++text = (expr_type == CHAR) ? SC : SI;
        }
        else {
            printf("%d: bad expression\n", line);
            exit(-1);
        }
    }

    // binary operator and postfix operators.
    {
        while (token >= level) {
            // handle according to current operator's precedence
            tmp = expr_type;
            if (token == Assign) {
                // var = expr;
                match(Assign);
                if (*text == LC || *text == LI) {
                    *text = PUSH; // save the lvalue's pointer
                } else {
                    printf("%d: bad lvalue in assignment\n", line);
                    exit(-1);
                }
                expression(Assign);

                expr_type = tmp;
                *++text = (expr_type == CHAR) ? SC : SI;
            }
            else if (token == Cond) {
                // expr ? a : b;
                match(Cond);
                *++text = JZ;
                addr = ++text;
                expression(Assign);
                if (token == ':') {
                    match(':');
                } else {
                    printf("%d: missing colon in conditional\n", line);
                    exit(-1);
                }
                *addr = (int)(text + 3);
                *++text = JMP;
                addr = ++text;
                expression(Cond);
                *addr = (int)(text + 1);
            }
            else if (token == Lor) {
                // logic or
                match(Lor);
                *++text = JNZ;
                addr = ++text;
                expression(Lan);
                *addr = (int)(text + 1);
                expr_type = INT;
            }
            else if (token == Lan) {
                // logic and
                match(Lan);
                *++text = JZ;
                addr = ++text;
                expression(Or);
                *addr = (int)(text + 1);
                expr_type = INT;
            }
            else if (token == Or) {
                // bitwise or
                match(Or);
                *++text = PUSH;
                expression(Xor);
                *++text = OR;
                expr_type = INT;
            }
            else if (token == Xor) {
                // bitwise xor
                match(Xor);
                *++text = PUSH;
                expression(And);
                *++text = XOR;
                expr_type = INT;
            }
            else if (token == And) {
                // bitwise and
                match(And);
                *++text = PUSH;
                expression(Eq);
                *++text = AND;
                expr_type = INT;
            }
            else if (token == Eq) {
                // equal ==
                match(Eq);
                *++text = PUSH;
                expression(Ne);
                *++text = EQ;
                expr_type = INT;
            }
            else if (token == Ne) {
                // not equal !=
                match(Ne);
                *++text = PUSH;
                expression(Lt);
                *++text = NE;
                expr_type = INT;
            }
            else if (token == Lt) {
                // less than
                match(Lt);
                *++text = PUSH;
                expression(Shl);
                *++text = LT;
                expr_type = INT;
            }
            else if (token == Gt) {
                // greater than
                match(Gt);
                *++text = PUSH;
                expression(Shl);
                *++text = GT;
                expr_type = INT;
            }
            else if (token == Le) {
                // less than or equal to
                match(Le);
                *++text = PUSH;
                expression(Shl);
                *++text = LE;
                expr_type = INT;
            }
            else if (token == Ge) {
                // greater than or equal to
                match(Ge);
                *++text = PUSH;
                expression(Shl);
                *++text = GE;
                expr_type = INT;
            }
            else if (token == Shl) {
                // shift left
                match(Shl);
                *++text = PUSH;
                expression(Add);
                *++text = SHL;
                expr_type = INT;
            }
            else if (token == Shr) {
                // shift right
                match(Shr);
                *++text = PUSH;
                expression(Add);
                *++text = SHR;
                expr_type = INT;
            }
            else if (token == Add) {
                // add
                match(Add);
                *++text = PUSH;
                expression(Mul);

                expr_type = tmp;
                if (expr_type > PTR) {
                    // pointer type, and not `char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                *++text = ADD;
            }
            else if (token == Sub) {
                // sub
                match(Sub);
                *++text = PUSH;
                expression(Mul);
                if (tmp > PTR && tmp == expr_type) {
                    // pointer subtraction
                    *++text = SUB;
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = DIV;
                    expr_type = INT;
                } else if (tmp > PTR) {
                    // pointer movement
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                    *++text = SUB;
                    expr_type = tmp;
                } else {
                    // numeral subtraction
                    *++text = SUB;
                    expr_type = tmp;
                }
            }
            else if (token == Mul) {
                // multiply
                match(Mul);
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
                expr_type = tmp;
            }
            else if (token == Div) {
                // divide
                match(Div);
                *++text = PUSH;
                expression(Inc);
                *++text = DIV;
                expr_type = tmp;
            }
            else if (token == Mod) {
                // Modulo
                match(Mod);
                *++text = PUSH;
                expression(Inc);
                *++text = MOD;
                expr_type = tmp;
            }
            else if (token == Inc || token == Dec) {
                // postfix inc(++) and dec(--)
                // we will increase the value to the variable and decrease it
                // on `ax` to get its original value.
                if (*text == LI) {
                    *text = PUSH;
                    *++text = LI;
                }
                else if (*text == LC) {
                    *text = PUSH;
                    *++text = LC;
                }
                else {
                    printf("%d: bad value in increment\n", line);
                    exit(-1);
                }

                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? ADD : SUB;
                *++text = (expr_type == CHAR) ? SC : SI;
                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? SUB : ADD;
                match(token);
            }
            else if (token == Brak) {
                // array access var[xx]
                match(Brak);
                *++text = PUSH;
                expression(Assign);
                match(']');

                if (tmp > PTR) {
                    // pointer, `not char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                else if (tmp < PTR) {
                    printf("%d: pointer type expected\n", line);
                    exit(-1);
                }
                expr_type = tmp - PTR;
                *++text = ADD;
                *++text = (expr_type == CHAR) ? LC : LI;
            }
            else {
                printf("%d: compiler error, token = %d\n", line, token);
                exit(-1);
            }
        }
    }
}

void statement()
{
    // there are 8 kinds of statements here:
    // 1. if (...) <statement> [else <statement>]
    // 2. while (...) <statement>
    // 3. { <statement> }
    // 4. return xxx;
    // 5. <empty statement>;
    // 6. expression; (expression end with semicolon)

    int *a, *b; // bess for branch control

    if (token == If) {
        // if (...) <statement> [else <statement>]
        //
        //   if (...)           <cond>
        //                      JZ a
        //     <statement>      <statement>
        //   else:              JMP b
        // a:
        //     <statement>      <statement>
        // b:                   b:
        //
        //
        match(If);
        match('(');
        expression(Assign);  // parse condition
        match(')');

        // emit code for if
        *++text = JZ;
        b = ++text;

        statement();         // parse statement
        if (token == Else) { // parse else
            match(Else);

            // emit code for JMP B
            *b = (int)(text + 3);
            *++text = JMP;
            b = ++text;

            statement();
        }

        *b = (int)(text + 1);
    }
    else if (token == While) {
        //
        // a:                     a:
        //    while (<cond>)        <cond>
        //                          JZ b
        //     <statement>          <statement>
        //                          JMP a
        // b:                     b:
        match(While);

        a = text + 1;

        match('(');
        expression(Assign);
        match(')');

        *++text = JZ;
        b = ++text;

        statement();

        *++text = JMP;
        *++text = (int)a;
        *b = (int)(text + 1);
    }
    else if (token == '{') {
        // { <statement> ... }
        match('{');

        while (token != '}') {
            statement();
        }

        match('}');
    }
    else if (token == Return) {
        // return [expression];
        match(Return);

        if (token != ';') {
            expression(Assign);
        }

        match(';');

        // emit code for return
        *++text = LEV;
    }
    else if (token == ';') {
        // empty statement
        match(';');
    }
    else {
        // a = b; or function_call();
        expression(Assign);
        match(';');
    }
}


int eval()
{
    int op, *tmp;
    cycle = 0;
    while (1) {
        op = *pc++; // get next operation code
        cycle++;
        // print debug info
        if (debug) {
            printf("%d> %.4s", cycle,
                   & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                   "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                   "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT"[op * 5]);
            if (op <= ADJ)
                printf(" %d\n", *pc);
            else
                printf("\n");
        }

        if (op == IMM)       {ax = *pc++;}                                     // load immediate value to ax
        else if (op == LC)   {ax = *(char *)ax;}                               // load character to ax, address in ax
        else if (op == LI)   {ax = *(int *)ax;}                                // load integer to ax, address in ax
        else if (op == SC)   {ax = *(char *)*sp++ = ax;}                       // save character to address, value in ax, address on stack
        else if (op == SI)   {*(int *)*sp++ = ax;}                             // save integer to address, value in ax, address on stack
        else if (op == PUSH) {*--sp = ax;}                                     // push the value of ax onto the stack
        else if (op == JMP)  {pc = (int *)*pc;}                                // jump to the address
        else if (op == JZ)   {pc = ax ? pc + 1 : (int *)*pc;}                   // jump if ax is zero
        else if (op == JNZ)  {pc = ax ? (int *)*pc : pc + 1;}                   // jump if ax is zero
        else if (op == CALL) {*--sp = (int)(pc+1); pc = (int *)*pc;}           // call subroutine
        //else if (op == RET)  {pc = (int *)*sp++;}                              // return from subroutine;
        else if (op == ENT)  {*--sp = (int)bp; bp = sp; sp = sp - *pc++;}      // make new stack frame
        else if (op == ADJ)  {sp = sp + *pc++;}                                // add esp, <size>
        else if (op == LEV)  {sp = bp; bp = (int *)*sp++; pc = (int *)*sp++;}  // restore call frame and PC
        else if (op == ENT)  {*--sp = (int)bp; bp = sp; sp = sp - *pc++;}      // make new stack frame
        else if (op == ADJ)  {sp = sp + *pc++;}                                // add esp, <size>
        else if (op == LEV)  {sp = bp; bp = (int *)*sp++; pc = (int *)*sp++;}  // restore call frame and PC
        else if (op == LEA)  {ax = (int)(bp + *pc++);}                         // load address for arguments.

        else if (op == OR)  ax = *sp++ | ax;
        else if (op == XOR) ax = *sp++ ^ ax;
        else if (op == AND) ax = *sp++ & ax;
        else if (op == EQ)  ax = *sp++ == ax;
        else if (op == NE)  ax = *sp++ != ax;
        else if (op == LT)  ax = *sp++ < ax;
        else if (op == LE)  ax = *sp++ <= ax;
        else if (op == GT)  ax = *sp++ >  ax;
        else if (op == GE)  ax = *sp++ >= ax;
        else if (op == SHL) ax = *sp++ << ax;
        else if (op == SHR) ax = *sp++ >> ax;
        else if (op == ADD) ax = *sp++ + ax;
        else if (op == SUB) ax = *sp++ - ax;
        else if (op == MUL) ax = *sp++ * ax;
        else if (op == DIV) ax = *sp++ / ax;
        else if (op == MOD) ax = *sp++ % ax;


        else if (op == EXIT) { printf("exit(%d)", *sp); return *sp;}
        else if (op == OPEN) { ax = open((char *)sp[1], sp[0]); }
        else if (op == CLOS) { ax = close(*sp);}
        else if (op == READ) { ax = read(sp[2], (char *)sp[1], *sp); }
        else if (op == PRTF) { tmp = sp + pc[1]; ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); }
        else if (op == MALC) { ax = (int)malloc(*sp);}
        //else if (op == MSET) { ax = (int)memset((char *)sp[2], sp[1], *sp);}
        //else if (op == MCMP) { ax = memcmp((char *)sp[2], (char *)sp[1], *sp);}
        else {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
    return 0;
}


void function_body()
{
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }

    int pos_local; // position of local variables on the stack.
    int type;
    pos_local = index_of_bp;

    while (token == Int || token == Char) {
        // local variable declaration, just like global ones.
        basetype = (token == Int) ? INT : CHAR;
        match(token);

        while (token != ';') {
            type = basetype;
            while (token == Mul) {
                match(Mul);
                type = type + PTR;
            }

            if (token != Id) {
                // invalid declaration
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }
            if (current_id->Id_class == Loc) {
                // identifier exists
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }
            match(Id);

            // store the local variable
            current_id->Bclass = current_id->Id_class; current_id->Id_class  = Loc;
            current_id->Btype  = current_id->Type;  current_id->Type   = type;
            current_id->Bvalue = current_id->Value; current_id->Value  = ++pos_local;   // index of current parameter

            if (token == ',') {
                match(',');
            }
        }
        match(';');
    }

    // save the stack size for local variables
    *++text = ENT;
    *++text = pos_local - index_of_bp;

    // statements
    while (token != '}') {
        statement();
    }

    // emit code for leaving the sub function
    *++text = LEV;
}

void function_parameter()
{
    int type;
    int params;
    params = 0;
    while (token != ')') {
        // int name, ...
        type = INT;
        if (token == Int) {
            match(Int);
        } else if (token == Char) {
            type = CHAR;
            match(Char);
        }

        // pointer type
        while (token == Mul) {
            match(Mul);
            type = type + PTR;
        }

        // parameter name
        if (token != Id) {
            printf("%d: bad parameter declaration\n", line);
            exit(-1);
        }
        if (current_id->Id_class == Loc) {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }

        match(Id);
        // store the local variable
        current_id->Bclass = current_id->Id_class; current_id->Id_class  = Loc;
        current_id->Btype  = current_id->Type;  current_id->Type = type;
        current_id->Bvalue = current_id->Value; current_id->Value  = params++;   // index of current parameter

        if (token == ',') {
            match(',');
        }
    }
    index_of_bp = params+1;
}

void function_declaration()
{
    // type func_name (...) {...}
    //               | this part

    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();
    //match('}');

    // unwind local variable declarations for all local variables.
    current_id = symbols;
    while (current_id && current_id->Token) {
        if (current_id->Id_class == Loc) {
            current_id->Id_class = current_id->Bclass;
            current_id->Type  = current_id->Btype;
            current_id->Value = current_id->Bvalue;
        }
        current_id = current_id->Next;
    }
}

void global_declaration()
{
    int type;
    basetype = INT;
    // parse type information
    if (token == Int) {
        match(Int);
    } else if (token == Char) {
        match(Char);
        basetype = CHAR;
    }
    // parse the comma seperated variable declaration.
    while (token != ';' && token != '}') {
        type = basetype;
        // parse pointer type, note that there may exist `int ****x;`
        while (token == Mul) {
            match(Mul);
            type = type + PTR;
        }

        if (token != Id) {
            // invalid declaration
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }
        if (current_id->Id_class) {
            // identifier exists
            printf("%d: duplicate global declaration\n", line);
            exit(-1);
        }
        match(Id);
        current_id->Type = type;

        if (token == '(') {
            current_id->Id_class = Fun;
            current_id->Value = (int)(text + 1); // the memory address of function
            function_declaration();
        } else {
            // variable declaration
            current_id->Id_class = Glo; // global variable
            current_id->Value = (int)data; // assign memory address
            data = data + sizeof(int);
        }

        if (token == ',') {
            match(',');
        }
    }
    scanner();
}

void match(int tk)
{
    if (token == tk) {
        scanner();
    } else {
        printf("%d: expected token: %d\n", line, tk);
        exit(-1);
    }
}


void start_program()
{
    scanner();
    while (token > 0) {
        global_declaration();
    }
    return ;
}

int main(int argc, char **argv)
{
    int i, file;
    int *tmp;
    argc--;
    argv++;

    poolsize = 256 * 1024;    // arbitrary size
    line = 1;

    // parse arguments
    if (argc > 0 && **argv == '-' && (*argv)[1] == 'S') {
        assembly = 1;
        --argc;
        ++argv;
    }
    if (argc > 0 && **argv == '-' && (*argv)[1] == 'g') {
        debug = 1;
        --argc;
        ++argv;
    }
    if (argc > 0 && **argv == '-' && (*argv)[1] == 'i') {
        info = 1;
        --argc;
        ++argv;
    }

    if (argc < 1) {
        printf("usage: xc [-s] [-d] file ...\n");
        return -1;
    }

    if ((file = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }


    // allocate memory for virtual machine
    if (!(text = old_text = calloc(1, poolsize))) {
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    if (!(data = calloc(1, poolsize))) {
        printf("could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = calloc(1, poolsize))) {
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }

    bp = sp = (int *)((int)stack + poolsize);
    ax = 0;

    old_text = text;

    src = "char else enum if int return sizeof while "
          "open read close printf malloc exit void main";

    i = Char;
    while (i <= While) {
        scanner();
        current_id->Token = i++;
    }

    i = OPEN;
    while (i <= EXIT) {
        scanner();
        current_id->Id_class = Sys;
        current_id->Type = INT;
        current_id->Value = i++;
    }

    scanner(); current_id->Token = Char; // handle void type
    scanner(); idmain = current_id; // keep track of main

    #if 0
    i = 0;
    text[i++] = IMM;
    text[i++] = 10;
    text[i++] = PUSH;
    text[i++] = IMM;
    text[i++] = 20;
    text[i++] = ADD;
    text[i++] = PUSH;
    text[i++] = EXIT;

    pc = text;
    #endif

    if (!(src = old_src = malloc(poolsize))) {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }

    // read the source file
    if ((i = read(file, src, poolsize-1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    *(src+i) = 0;           // add EOF(NULL) character
    close(file);

    start_program();
    if (!(pc = (int *)idmain->Value)) {
        printf("main() not defined\n");
        return -1;
    }

    // setup stack
    sp = (int *)((int)stack + poolsize);
    *--sp = EXIT; // call exit if main returns
    *--sp = PUSH; tmp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmp;
    if (info) {
        see_identifier_info();
    }
    return eval();
    return 0;
}

void see_identifier_info()
{
    int data_type;
    int point_to;
    int i;
    current_id = symbols;
    printf("Variable token Variable type Variable name  \n");
    while (current_id) {
        printf("%d             ", current_id->Token);
        if (!current_id->Type) {
            printf("char ");
        } else if (current_id->Type == 1) {
            printf("int ");
        } else if (current_id->Type > 1) {
            point_to = current_id->Type / 2;
            data_type = current_id->Type % 2;
            switch (data_type) {
            case 0:
                printf("char ");
                break;
            case 1:
                printf("int ");
                break;
            }
            for (i = 0; i < point_to; i++) {
                printf("*");
            }
        }
        printf("            ");
        for (i = 0; i < current_id->char_number; i++) {
            printf("%c", current_id->Name[i]);
        }
        printf("\n");
        current_id = current_id->Next;
    }
    return ;
}
