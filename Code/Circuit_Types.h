#ifndef CIRCUIT_TYPES_H
#define CIRCUIT_TYPES_H

// FAI UN PROGRAMMA FIGO CON MMAP (MAGARI FILE .txt e .pdf e .exe)
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>

#define OBJ_INT   1
#define OBJ_STR   2
#define OBJ_LIST  3
#define OBJ_TRAN  4
#define OBJ_GATE  5
#define OBJ_WIRE  6
#define OBJ_FUNC  7
#define OBJ_VAR   8
#define OBJ_OP    9
#define OBJ_DOT   10
#define OBJ_INDEX 11
#define OBJ_BOOL  12
#define OBJ_BLOCK 13
#define OBJ_IF    14
#define OBJ_VOID  15

#define READ_ONLY  0
#define READ_WRITE 1

typedef struct Circuit Circuit;
typedef struct Context Context;
typedef struct Gate Gate;
typedef struct Transistor Transistor;
typedef struct Wire Wire;
typedef struct Symbol Symbol;
typedef struct CircuitObj CircuitObj;

typedef void* (*CircuitFunc)(Context *context, CircuitObj *obj_list, int count);

typedef enum
{
    SYM_FUNC,
    SYM_VAR,
} SymType;

typedef enum
{
    OP_ASSIGN,
    OP_ADD,
    OP_SUB,
    OP_MOL,
    OP_DIV,
} OpType;

typedef struct Wire{
    char *name;
    int power;
    int next_power;
    int driven; // 1 == driven by transistor || 0 == always on or always off
} Wire;

typedef struct Transistor{
    char *name;
    int state; // if it's closed or open
    int invert;
    struct Wire **input_wire;
    struct Wire *output_wire;
    struct Wire *control_wire;
    int count;
} Transistor;

typedef struct Gate
{
    int type; // 0 usa trans e 1 usa gate
    char *name;
    Wire *a;
    Wire *b;
    Wire *output;
    union 
    {
        Transistor **transistor_list;
        struct Gate **gate_list;
    };
    int count;
} Gate;

typedef struct Operator
{
    char *name;
    CircuitObj *left;
    CircuitObj *right;
    int prec;
    OpType type;
} Operator;

typedef struct ObjProp
{
    char *name;
    CircuitObj *prop; // doppio puntatore in quanto voglio l'indirizzo di memoria dell'oggetto che stiamo puntando!
} ObjProp;

typedef struct CircuitObj 
{
    int type;
    int mode;
    union
    {
        // PROVO A METTERE UN * IN PIU' DI FRONTE AD OGNI TIPO PER POTER PASSARE IL TIPO ATTRAVERSO L'INDIRIZZO E NON SOLO VALORE (COSI' DA FACILITARE LE PROPS).
        int *i; // sia per operazioni sia per bool;
        char **str;
        Symbol **sys;
        Operator *op;
        Wire **wire;
        Transistor **transistor;
        Gate **gate;
        Circuit *circuit;
        struct List // sia per liste che per blocchi
        {
            CircuitObj **objs;
            int count;
        } list;
        struct Dot
        {
            CircuitObj *left;
            char *name;
        } prop;
        struct Bracket
        {
            CircuitObj *left;
            CircuitObj *index;
        } bracket;
        struct If
        {
            int line;
            CircuitObj *statement;     
        } condition;
    };
    ObjProp **props;
    int prop_count;
    int ref_count;
} CircuitObj;

typedef struct RowParser
{
    int row;
    CircuitObj *content;
} RowParser;

typedef struct Symbol 
{
    char *name;
    void *data;
    int param_count;
    CircuitObj *param_list;
    CircuitObj *obj_returned;
    SymType type;
} Symbol;

typedef struct SymbolTable
{
    Symbol **sym_list;
    int sym_count;
} SymbolTable;

typedef struct Context
{
    CircuitObj *stack;
    SymbolTable *symTable;
    //OperatorTable *opTable;
    RowParser **rows;
} Context;

typedef struct Circuit{
    int state;
    Context *context;
    Transistor **transistors;
    Wire **wires;
    int transistor_count;
    int wire_count;
} Circuit;

Circuit *create_circuit();
void run_circuit(Circuit *circuit);
Wire *create_wire(Circuit *circuit, char *name, int power);
Transistor *create_transistor(char *name, int wires);
Gate *create_gate(char *name, int type);
Gate *create_OR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire);
Gate *create_AND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b);
Gate *create_NOT_Gate(Circuit *circuit, Wire *input_wire, Wire *output_wire);
Gate *create_NOR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire);
Gate *create_NAND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b);
Gate *create_XOR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire);

#endif // CIRCUIT_TYPES_H