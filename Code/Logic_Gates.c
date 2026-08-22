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
#define OBJ_WIRE  5
#define OBJ_FUNC  6
#define OBJ_VAR   7
#define OBJ_OP    8
#define OBJ_DOT   9
#define OBJ_INDEX 10
#define OBJ_VOID  11

#define READ_ONLY  0
#define READ_WRITE 1

typedef struct Circuit Circuit;
typedef struct Transistor Transistor;
typedef struct Wire Wire;
typedef struct Symbol Symbol;
typedef struct CircuitObj CircuitObj;

typedef void* (*CircuitFunc)(Circuit *circuit, CircuitObj *obj_list, int count);

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

typedef struct Operator
{
    char *name;
    OpType type;
} Operator;

//typedef struct OperatorTable
//{
    //Operator **ops;
    //int count;
//} OperatorTable;

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
        int *i;
        char **str;
        Symbol **sys;
        Operator *op;
        Wire **wire;
        Transistor **transistor;
        Circuit *circuit;
        struct List
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

Wire *create_wire(Circuit *circuit, char *name, int power);
Transistor *create_transistor(char *name, int wires);

CircuitObj *create_obj()
{
    CircuitObj *obj = malloc(sizeof(CircuitObj));
    obj->type = 0;
    obj->mode = READ_WRITE;

    obj->i = NULL;
    obj->str = NULL;
    obj->sys = NULL;
    obj->list.objs = NULL;
    obj->list.count = 0; 

    obj->props = NULL;
    obj->prop_count = 0;
  
    obj->prop.left = NULL;
    obj->prop.name = NULL;
    
    obj->bracket.index = NULL; 
    obj->bracket.left = NULL;
    
    obj->ref_count = 1;
    return obj;
}

CircuitObj *create_int_obj(int i)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_INT;
    obj->i = malloc(sizeof(int*));
    *(obj->i) = i;
    return obj;
}

CircuitObj *create_str_obj(int length)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_STR;
    obj->str = malloc(sizeof(char*));
    *(obj->str) = malloc(length+1);
    (*obj->str)[length-1] = '\0';
    return obj;
}

CircuitObj *create_list_obj()
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_LIST;
    obj->list.objs = NULL;
    obj->list.count = 0;
    return obj;
}

CircuitObj *create_wire_obj(Circuit *circuit, char *name, int power)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_WIRE;
    obj->wire = malloc(sizeof(Wire*));
    *(obj->wire) = create_wire(circuit, name, power);
    return obj;
}

CircuitObj *create_tran_obj(char *name, int wires)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_TRAN;
    obj->transistor = malloc(sizeof(Transistor*));
    *(obj->transistor) = create_transistor(name, wires);
    return obj;
}

Symbol *create_symbol(char *name, void *data, SymType type, int param_count);

CircuitObj *create_sym_obj(Symbol *sym)
{
    CircuitObj *obj = create_obj();
    //Symbol *sym = create_symbol(name, data, type, param_count);
    switch (sym->type)
    {
    case SYM_FUNC:
        obj->type = OBJ_FUNC;
        break;
    case SYM_VAR:
        obj->type = OBJ_VAR;
        break;
    default:
        break;
    }
    obj->sys = malloc(sizeof(Symbol*));
    *(obj->sys) = sym; 
    return obj;
}

/*
void add_prop(CircuitObj *circuitObj, CircuitObj *prop, char *name)
{
    int length = circuitObj->prop_count;
    circuitObj->props = realloc(circuitObj->props, sizeof(ObjProp*) * (length+1));
    circuitObj->props[length] = malloc(sizeof(ObjProp));
    circuitObj->props[length]->name = strdup(name);
    circuitObj->props[length]->prop = malloc(sizeof(CircuitObj*));

    (*circuitObj->props[length]->prop) = malloc(sizeof(CircuitObj));

    switch(prop->type)
    {
    case OBJ_INT:
        (*circuitObj->props[length]->prop)->type = OBJ_INT;
        (*circuitObj->props[length]->prop)->i = prop->i;
        break;
    case OBJ_STR:
        (*circuitObj->props[length]->prop)->type = OBJ_STR;
        //(*circuitObj->props[length]->prop)->str = wire->name;
        (*circuitObj->props[length]->prop)->str = prop->str;
        break;
    case OBJ_LIST:
        (*circuitObj->props[length]->prop)->type = OBJ_LIST;
        (*circuitObj->props[length]->prop)->list.objs = NULL;
        (*circuitObj->props[length]->prop)->list.count = 0;
        (*circuitObj->props[length]->prop)->list = prop->list;
        break;
    case OBJ_WIRE:
        (*circuitObj->props[length]->prop)->type = OBJ_WIRE;
        (*circuitObj->props[length]->prop)->wire = circuitObj->wire;
        break;
    case OBJ_TRAN:
        (*circuitObj->props[length]->prop)->type = OBJ_TRAN;
        (*circuitObj->props[length]->prop)->transistor = circuitObj->transistor;
        break;

    default:
        break;
    }

    circuitObj->prop_count = length+1;
}
*/

void add_prop(CircuitObj *circuitObj, CircuitObj *prop, char *name)
{
    int length = circuitObj->prop_count;
    circuitObj->props = realloc(circuitObj->props, sizeof(ObjProp*) * (length+1));
    circuitObj->props[length] = malloc(sizeof(ObjProp));
    circuitObj->props[length]->name = strdup(name);
    circuitObj->props[length]->prop = prop;

    circuitObj->prop_count = length+1;
}

/*
void add_int_prop(CircuitObj *circuitObj, int **i, char *name)
{
    int length = circuitObj->prop_count;
    circuitObj->props = realloc(circuitObj->props, sizeof(ObjProp*) * (length+1));
    circuitObj->props[length] = malloc(sizeof(ObjProp));
    circuitObj->props[length]->name = strdup(name);
    circuitObj->props[length]->prop = malloc(sizeof(CircuitObj*));

    (*circuitObj->props[length]->prop) = malloc(sizeof(CircuitObj));

    (*circuitObj->props[length]->prop)->type = OBJ_INT;
    (*circuitObj->props[length]->prop)->i = prop->i;
    
    circuitObj->prop_count = length+1;
}
*/

Operator *create_op(char *name, OpType type)
{
    Operator *op = malloc(sizeof(Operator));
    op->name = strdup(name);
    op->type = type;
    return op;
}

Symbol *create_symbol(char *name, void *data, SymType type, int param_count)
{
    Symbol *sys = malloc(sizeof(Symbol));
    sys->name = strdup(name);
    sys->data = data;
    sys->param_count = param_count;
    sys->type = type;
    return sys;
}

void append_sym_to_table(Circuit *circuit, Symbol *sym)
{
    int len = circuit->context->symTable->sym_count + 1;

    Symbol ***function_list = &(circuit->context->symTable->sym_list);

    *function_list = realloc(*function_list, sizeof(Symbol*) * (len));

    //(*function_list)[len-1] = malloc(sizeof(Symbol));
    //(*function_list)[len-1]->name = strdup(name);
    //(*function_list)[len-1]->param_count = param_count;
    //(*function_list)[len-1]->data = data;
    //(*function_list)[len-1]->type = type;

    (*function_list)[len-1] = sym;

    circuit->context->symTable->sym_count++;
}

void append_wire_to_circuit(Circuit *circuit, Wire *wire)
{
    int len = circuit->wire_count + 1;
    circuit->wires = realloc(circuit->wires, sizeof(Wire*) * len);
    circuit->wires[len-1] = wire; 
    circuit->wire_count++;
}

Wire *initialize_wire(char *name, int power)
{
    Wire *wire = malloc(sizeof(*wire));
    wire->name = name;
    wire->power = power;
    wire->next_power = power;
    wire->driven = 1;
    return wire;
}

Wire *create_wire(Circuit *circuit, char *name, int power)
{
    Wire *wire = initialize_wire(name, power);
    append_wire_to_circuit(circuit, wire);
    return wire;
}

Wire **create_wire_list(Circuit *circuit, char *name, int power, int count)
{
    Wire **wire_list = malloc(sizeof(*wire_list) * count);

    for(int i = 0; i < count; i++){
        Wire *wire = initialize_wire(name, power);
        wire_list[i] = wire;
        append_wire_to_circuit(circuit, wire);
    }

    return wire_list;
}

Transistor *create_transistor(char *name, int wires)
{
    Transistor *transistor = malloc(sizeof(*transistor));

    transistor->name = strdup(name);
    transistor->state = 0;
    transistor->count = 0;

    if(wires == 0)
    {
        transistor->input_wire = NULL;
    }
    else if(wires>0)
    {
        transistor->count = wires;
        transistor->input_wire = malloc(sizeof(Wire *) * wires);
    }

    transistor->output_wire = NULL;
    transistor->control_wire = NULL;

    return transistor;
}

void update_transistor(Transistor *transistor, int state, int invert)
{
    transistor->state = state;
    transistor->invert = invert;
}

void append_t_to_c(Circuit *circuit, Transistor *transistor)
{
    int length = circuit->transistor_count;

    circuit->transistors = realloc(circuit->transistors, sizeof(Transistor*) * (length + 1)); 
    circuit->transistors[length] = transistor;
    
    circuit->transistor_count++;
}

CircuitObj *get_prop_value(CircuitObj *dot_obj, char *prop_name);
void *print_objects(CircuitObj *obj);

char *return_print_objects(CircuitObj *obj)
{
    char *result = NULL;

    switch (obj->type)
    {
        case OBJ_INT:
        {
            result = malloc(32); // abbastanza per "int: " + un numero
            snprintf(result, 32, "int: %d", obj->i);
            break;
        }
        case OBJ_STR:
        {
            int len = strlen(*(obj->str)) + 6; // "str: " + stringa + '\0'
            result = malloc(len);
            snprintf(result, len, "str: %s", obj->str);
            break;
        }
        case OBJ_WIRE:
        {
            // Formato: "wire: <name> (power: <val>)"
            const char *name = (*(obj->wire) && (*obj->wire)->name) ? (*obj->wire)->name : "(unnamed)";
            int power = obj->wire ? (*obj->wire)->power : 0;
            int len = strlen(name) + 32;
            result = malloc(len);
            snprintf(result, len, "wire: %s (power: %d)", name, power);
            break;
        }
        case OBJ_TRAN:
        {
            // Formato: "transistor: <name> (out: <val>)"
            const char *name = (obj->transistor && (*obj->transistor)->name) ? (*obj->transistor)->name : "(unnamed)";
            int out_power = (obj->transistor && (*obj->transistor)->output_wire) ? (*obj->transistor)->output_wire->power : -1;
            int len = strlen(name) + 36;
            result = malloc(len);
            snprintf(result, len, "transistor: %s (out: %d)", name, out_power);
            break;
        }
        case OBJ_LIST:
        {
            int cap = 64;
            int len = 0;
            result = malloc(cap);
            if (!result) return NULL;

            // Prefisso iniziale
            len += snprintf(result, cap, "list: [");

            for (int i = 0; i < obj->list.count; i++)
            {
                // Chiamata ricorsiva per l'elemento corrente
                char *item_str = return_print_objects(obj->list.objs[i]);
                if (!item_str) continue;

                size_t item_len = strlen(item_str);
                // Separatore ", " tra gli elementi
                size_t needed = len + item_len + (i > 0 ? 2 : 0) + 2; // +2 per "]" e '\0'

                // Ridimensionamento dinamico del buffer se necessario
                if (needed > cap)
                {
                    cap = needed * 2;
                    char *new_res = realloc(result, cap);
                    if (!new_res)
                    {
                        free(item_str);
                        free(result);
                        return NULL;
                    }
                    result = new_res;
                }

                // Aggiunta della virgola e spazio se non è il primo elemento
                if (i > 0)
                {
                    strcat(result, ", ");
                    len += 2;
                }

                strcat(result, item_str);
                len += item_len;
                free(item_str); // Liberiamo la stringa temporanea del singolo elemento
            }

            // Chiusura della parentesi quadra
            if (len + 2 > cap)
            {
                result = realloc(result, len + 2);
            }
            strcat(result, "]");
            break;
        }
        case OBJ_FUNC:
        {
            int len = strlen((*obj->sys)->name) + 7; // "func: " + nome + '\0'
            result = malloc(len);
            snprintf(result, len, "func: %s", (*obj->sys)->name);
            break;
        }
        case OBJ_VAR:
        {
            int len = strlen((*obj->sys)->name) + 7; // "func: " + nome + '\0'
            result = malloc(len);
            snprintf(result, len, "var: %s", (*obj->sys)->name);
            break;
        }
        case OBJ_DOT:
        {
            result = return_print_objects(get_prop_value(obj->prop.left, obj->prop.name));
            break;
        }
        case OBJ_OP:
        {
            int len = strlen(obj->op->name) + 5;
            result = malloc(len);
            snprintf(result, len, "op: %s", obj->op->name);
            break;
        }
        case 0:
        {
            result = malloc(10);
            snprintf(result, 10, "SEPARATO");
            break;
        }
       default:
        {
            result = malloc(16);
            snprintf(result, 16, "SCONOSCIUTO");
            break;
        }
    }

    return result;
}

void *print_objects(CircuitObj *obj)
{
    switch (obj->type)
    {
        case OBJ_INT:
            printf("int: %d\n", *obj->i);
            break;
        case OBJ_STR:
            printf("str: %s\n", *obj->str);
            break;
        case OBJ_LIST:
            printf("list: %d\n", obj->list.count);
            for(int i = 0; i < obj->list.count; i++)
            {
                printf("   %d - ", i);
                printf("\t");
                print_objects(obj->list.objs[i]);
            }
            break;
        case OBJ_TRAN:
            printf("transistor: %s\n", (*obj->transistor)->name);
            break;
        case OBJ_WIRE:
            printf("wire: %s\n", (*obj->wire)->name);
            break;
        case OBJ_FUNC:
            printf("func: %s\n", (*obj->sys)->name);
            break;
        case OBJ_VAR:
            //printf("var: %s, Value: %s\n", obj->sys->name, return_print_objects(obj->sys->data));
            printf("var: %s - ", (*obj->sys)->name);
            printf("VALUE: ");
            if((*obj->sys)->data != NULL) print_objects((CircuitObj*)(*(obj->sys))->data);
            else printf("\n");
            break;
        case OBJ_OP:
            printf("op: %s\n", obj->op->name);
            break;
        case OBJ_DOT:
            printf("attribute: %s\n", obj->prop.name != NULL ? obj->prop.name : "none");
            printf("left prop: ");
            if(obj->prop.left != NULL)
            print_objects(obj->prop.left);
            break;
        case OBJ_INDEX:
            printf("Index: ");
            if(obj->bracket.index != NULL)
            {
                if(obj->bracket.index->i != NULL)
                {
                    printf("%d", *obj->bracket.index->i);
                }
            }
            printf("\n");
            printf("left element: ");
            if(obj->bracket.left != NULL)
            print_objects(obj->bracket.left);
            printf("\n");
            break;

        case 0:
            printf("SEPARATO\n");
            break;
        default:
            printf("SCONOSCIUTO\n");
            break;
    }
}

/*
void append_t_to_t(Transistor *from, Transistor *to)
{
    int length = from->count;

    from->next_transistors = realloc(from->next_transistors, sizeof(Transistor*) * (length + 1)); 
    from->next_transistors[length] = to;
    
    from->count++;
}
*/

void update_next_t(Circuit *circuit)
{
    for(int w = 0; w < circuit->wire_count; w++)
    {
        circuit->wires[w]->next_power = circuit->wires[w]->driven ? 0 : circuit->wires[w]->power;
    }

    for(int i = 0; i < circuit->transistor_count; i++)
    {
        Transistor *transistor = circuit->transistors[i];
        //Wire **input_wires = transistor->input_wire;

        if(transistor->control_wire != NULL)
        {
            transistor->state = transistor->control_wire->power;      
        }        

        int current_state = transistor->state + (transistor->invert ? (transistor->state ? -1 : 1) : 0);

        for(int j = 0; j < transistor->count; j++)
        {
            int current_power = transistor->input_wire[j]->power + (transistor->invert ? (transistor->input_wire[j]->power ? -1 : 1) : 0);

            if (current_state == 1 && current_power == 1 && transistor->output_wire != NULL)
            {
                transistor->output_wire->next_power = 1;
            }
        }
    }
}

int commit(Circuit *circuit)
{
    int changed = 0;
    for(int i = 0; i < circuit->wire_count; i++)
    {
        Wire *wire = circuit->wires[i];
        if(wire->power != wire->next_power) changed = 1;
        wire->power = wire->next_power;
    }
    return changed;
}

void print_transistors(Circuit *circuit)
{
    for(int i = 0; i < circuit->transistor_count; i++)
    {
        Transistor *transistor = circuit->transistors[i];
        printf("%s wire out p: %d and gate state is %s \n", transistor->name, transistor->output_wire->power, transistor->state ? "open" : "closed");
    }
}

int run_parallel_circuit(Circuit *circuit, int max_count)
{
    printf("\n");
    for(int i = 0; i < max_count; i++)
    {
        update_next_t(circuit);
        int changed = commit(circuit);

        printf("Cicle %d:\n", i+1);
        print_transistors(circuit);
        if(!changed) return i;
    }
    return max_count;
}

void run_circuit(Circuit *circuit)
{
    circuit->state = 1;
    int tries = run_parallel_circuit(circuit, 100);
    printf("Tot tries: %d\n", tries);
}

Transistor *create_OR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire)
{
    Transistor *transistor1 = create_transistor("OR_1", 1);
    Transistor *transistor2 = create_transistor("OR_1", 1);
    
    update_transistor(transistor1, 1, 0);
    update_transistor(transistor2, 1, 0);

    Wire *wire1_2 = create_wire(circuit, "or_out", 0);

    transistor1->input_wire[0] = a_wire;
    transistor1->output_wire = wire1_2;

    transistor2->input_wire[0] = b_wire;
    transistor2->output_wire = wire1_2;

    append_t_to_c(circuit, transistor1);
    append_t_to_c(circuit, transistor2);

    return transistor2;
}

Transistor *create_AND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b)
{
    Transistor *transistor1 = create_transistor("AND_1", 1);
    Transistor *transistor2 = create_transistor("AND_2", 1);
    
    update_transistor(transistor1, 1, 0);
    update_transistor(transistor2, 1, 0);

    Wire *mid = create_wire(circuit, "and_mid", 0); 
    Wire *out = create_wire(circuit, "and_out", 0);

    transistor1->input_wire[0] = input_wire;
    transistor1->output_wire = mid;
    transistor1->control_wire = a;

    transistor2->input_wire[0] = transistor1->output_wire;
    transistor2->output_wire = out;
    transistor2->control_wire = b;

    append_t_to_c(circuit, transistor1);
    append_t_to_c(circuit, transistor2);

    return transistor2;
}

Transistor *create_NOT_Gate(Circuit *circuit, Wire *input_wire, Wire *output_wire)
{
    Transistor *transistor1 = create_transistor("NOT", 1);
    
    update_transistor(transistor1, 0, 1);

    if(output_wire == NULL)
    {
        output_wire = create_wire(circuit, "not_out", 0);
    }

    transistor1->input_wire[0] = input_wire;
    transistor1->output_wire = output_wire;

    append_t_to_c(circuit, transistor1);

    return transistor1;
}

Transistor *create_NOR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire)
{
    Transistor *or_gate = create_OR_Gate(circuit, a_wire, b_wire);
    return create_NOT_Gate(circuit, or_gate->output_wire, NULL);
}

Transistor *create_NAND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b)
{
    Transistor *and_gate = create_AND_Gate(circuit, input_wire, a, b);
    return create_NOT_Gate(circuit, and_gate->output_wire, NULL);
}

Transistor *create_XOR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire)
{
    //quindi prende i due segnali (0,0) o (1,1) e in pratica (0, 0)
    //restituisce 1 mentre (1,1) restituisce zero. L'ultimo NOR
    //vedendo un segnale unito di (1,0) restituisce 0, mentre invece
    //se fossero (1,0) sin dall'inizio, allora il nor di (1,0)
    //restituisce 0 e poi Il NOR dei valori invertiti (0,1) restituisce
    //comunque zero e allora all'ultimo NOR arrivano due zeri (0,0) e restituirà 1.
    //Quindi se sono diversi restituisce 1 mentre se sono uguali restituisce zero.

    Wire *a = a_wire;
    Wire *b = b_wire;
    // Passo 1: Invertiamo i singoli segnali
    Transistor *not_a_wire = create_NOT_Gate(circuit, a, NULL);
    Transistor *not_b_wire = create_NOT_Gate(circuit, b, NULL);

    // Passo 2: NOR tra A e B (è vero solo se A=0 e B=0)
    Transistor *nor_ab = create_NOR_Gate(circuit, a, b);
    
    // Passo 3: NOR tra NOT_A e NOT_B (è vero solo se A=1 e B=1)
    //Wire **not_a_b_wires = malloc(sizeof(Wire*) * 2); 
    //not_a_b_wires[0] = not_a_wire->output_wire;
    //not_a_b_wires[1] = not_b_wire->output_wire;
    Transistor *nor_not_ab = create_NOR_Gate(circuit, not_a_wire->output_wire, not_b_wire->output_wire);
   
    //free(not_a_b_wires); 
    
    // Passo 4: Se è VERO che (A=0, B=0) oppure è VERO che (A=1, B=1), 
    // l'XOR deve dare 0. Il NOR finale fa proprio questo controllo!
    //Wire **nor_ab_nor_not_ab = malloc(sizeof(Wire*) * 2);
    //nor_ab_nor_not_ab[0] = nor_ab->output_wire;
    //nor_ab_nor_not_ab[1] = nor_not_ab->output_wire;

    Transistor *xor = create_NOR_Gate(circuit, nor_ab->output_wire, nor_not_ab->output_wire);     
    xor->name = "XOR";
    //free(nor_ab_nor_not_ab);
    return xor;
}

void create_Half_Adder(Circuit *circuit, int input_a, int input_b)
{
    // ---------- HALF ADDER ---------- //
    Wire *vcc = create_wire(circuit, "VCC", 1);
    vcc->driven = 0;

    //char numeri[6];
    //fgets(numeri, sizeof(numeri), stdin);
    //int input_a = numeri[0] - 48; // oppure - '0'
    //int input_b = numeri[2] - 48;

    // Ingressi del circuito: A = 1, B = 1 (1 + 1 = 2, in binario: Sum=0, Carry=1)
    Wire *a = create_wire(circuit, "A", input_a); a->driven = 0;
    Wire *b = create_wire(circuit, "B", input_b); b->driven = 0;

    // Preparazione array fili per le porte a 2 ingressi
    //Wire **inputs = malloc(sizeof(Wire*) * 2);
    //inputs[0] = a;
    //inputs[1] = b;

    // 1. Somma: XOR tra A e B
    Transistor *sum_gate = create_XOR_Gate(circuit, a, b);

    // 2. Riporto: AND tra A e B (alimentata da VCC, controllata da A e B)
    Transistor *carry_gate = create_AND_Gate(circuit, vcc, a, b);

    //free(inputs);
}

void create_Clock_Cicle(Circuit *circuit)
{
    // 1. Creiamo un cavo d'ingresso indipendente con valore 0
    Wire *loop_wire = create_wire(circuit, "loop_wire", 0);
    loop_wire->driven = 1; // Permettiamo al transistor di cambiarlo

    // 2. Creiamo una porta NOT che legge da loop_wire e scrive su loop_wire!
    Transistor *not_gate = create_NOT_Gate(circuit, loop_wire, loop_wire);

    // Colleghiamo l'uscita del NOT direttamente al cavo di ingresso
    // Sorgente di alimentazione sempre attiva (VCC)
}

Symbol *find_function_by_name(Circuit *circuit, char *name)
{
    for(int i = 0; i < circuit->context->symTable->sym_count; i++)
    {
        if(circuit->context->symTable->sym_list[i]->type == SYM_FUNC)
        {
            if(strcmp(circuit->context->symTable->sym_list[i]->name, name) == 0)
            {
                return circuit->context->symTable->sym_list[i];
            }
        }
    }
    return NULL;
}

Symbol *find_var_by_name(Circuit *circuit, char *name)
{
    for(int i = 0; i < circuit->context->symTable->sym_count; i++)
    {
        if(circuit->context->symTable->sym_list[i]->type == SYM_VAR)
        {
            if(strcmp(circuit->context->symTable->sym_list[i]->name, name) == 0)
            {
                return circuit->context->symTable->sym_list[i];
            }
        }
    }
    return NULL;
}

char *create_token(char *start, size_t length)
{
    char *final_token = NULL;

    final_token = malloc(length + 1);
    // memset(final_token, start, end - start);
    memcpy(final_token, start, length);
    final_token[length] = '\0';

    return final_token;
}

void append_obj_to_list(CircuitObj *list_obj, CircuitObj *obj)
{
    int length = list_obj->list.count;

    list_obj->list.objs = realloc(list_obj->list.objs, sizeof(CircuitObj*) * (length+1));
    list_obj->list.objs[length] = obj;

    list_obj->list.count++; 
}

RowParser **parse(Circuit *circuit, CircuitObj *dest, char *txt, int add_separator);
CircuitObj *get_actual_value(CircuitObj *obj);

CircuitObj *parse_obj(Circuit *circuit, char **token, char *start, int is_list)
{
    CircuitObj *circuitObj = create_obj(); //= create_obj();// malloc(sizeof(CircuitObj));

    char *end = *token;

    int length = end - start;
    char *final_token = malloc(length + 1);
    memcpy(final_token, start, length);

    final_token[length] = '\0';
    printf("FINAL TOKEN: %s\n", final_token);
    

    Symbol *sym = NULL;
    for (int i = 0; i < circuit->context->symTable->sym_count; i++)
    {
        if(circuit->context->symTable->sym_list[i]->type == SYM_FUNC)
        {
            if (strcmp(final_token, circuit->context->symTable->sym_list[i]->name) == 0)
            {
                sym = circuit->context->symTable->sym_list[i];
                circuitObj = create_sym_obj(sym);
                break;
            }
        }else if(circuit->context->symTable->sym_list[i]->type == SYM_VAR)
        {
            if (strcmp(final_token, circuit->context->symTable->sym_list[i]->name) == 0)
            {
                sym = circuit->context->symTable->sym_list[i];
                circuitObj = create_sym_obj(sym);
                break;
            }
        }
    }
    if(strcmp(final_token, "=") == 0)
    {
        Operator *op = create_op(final_token, OP_ASSIGN);
        circuitObj = create_obj();
        circuitObj->type = OBJ_OP;
        circuitObj->op = op;
    }
    else if (strcmp(final_token, "+") == 0)
    {
        Operator *op = create_op(final_token, OP_ADD);
        circuitObj = create_obj();
        circuitObj->type = OBJ_OP;
        circuitObj->op = op;
    }
    else if (strcmp(final_token, "var") == 0)
    {
        while (isspace((*token)[0]) && (*token)[0] != '\n' && (*token)[0] != ';')
            (*token)++;
        char *start = *token;
        while (isgraph(*(token)[0]) && (*token)[0] != ';')
            (*token)++;
        char *end = *token;

        int length = end - start;

        char *var_name = malloc(length + 1);
        memcpy(var_name, start, length);
        var_name[length] = '\0';

        sym = find_var_by_name(circuit, var_name); 

        if(sym == NULL)
        {
            sym = create_symbol(var_name, NULL, SYM_VAR, 1);
            append_sym_to_table(circuit, sym);

            circuitObj = create_sym_obj(sym); 
        }else
        {
            printf("VAR GIA' ESISTENTE\n");
            circuitObj = create_sym_obj(sym); 
        }
    }
    else if (isdigit(final_token[0]))
    {
        circuitObj = create_int_obj(atoi(final_token)); 
    }else if((*token)[0] == '"')
    {
        (*token)++;
        start = *token;
        while((*token)[0] != '"')
        {
            (*token)++;
        }
        end = *token;
        (*token)++;

        length = end - start;
        int new_length = length;
        circuitObj = create_str_obj(new_length+1);
        memcpy(*circuitObj->str, start, new_length);
        
    }
    else if(final_token[0] == '.')
    {
        circuitObj->type = OBJ_DOT;

        length = length - 1;
        circuitObj->prop.name = malloc(length+1);
        memcpy(circuitObj->prop.name, start+1, length);
        circuitObj->prop.name[length] = '\0';
    }
    else if ((*token)[0] == '[' && is_list)
    {
        printf("\n|------------------------INIZIO LISTA------------------------------|\n");
        printf("INIZIO LISTA\n%s\n", *token);
        printf("START: %s", start);
        //printf("PREVIOUS: %s\n", previous);

        (*token)++;
        char *start = *token;
        int bracket_balance = 1;
        int brack_offset = 0;
        //(*token)++;
        printf("PRIMA DEL WHILE\n%s\n", *token);
        while ((*token)[0] != '\0' && bracket_balance > 0)
        {
            if ((*token)[0] == '[')
            {
                brack_offset = 1;
                bracket_balance++;
            }
            if ((*token)[0] == ']')
            {
                bracket_balance--;
            }
            if (bracket_balance > 0)
                (*token)++;
        }
        char *end = *token;
        //if((*token)[0] == ']')
        //while(isspace(token[0]) && token[0] != '\n' && token[0] != ';') token++;

        printf("DOPO IL WHILE\n%s\n", *token);
        circuitObj->type = OBJ_LIST;
        circuitObj->list.objs = NULL;
        circuitObj->list.count = 0;

        int new_length = end - start; // Esclusa l'ultima quadra
        char *list_txt = malloc(new_length + 1);
        // int offset = 1 + brack_offset;
        memcpy(list_txt, start, new_length);
        list_txt[new_length] = '\0';
        printf("PARSING QUESTO/I ELEMENTO: %s\n", list_txt);
        parse(circuit, circuitObj, list_txt, 0); // metto ciruitObj come secondo parametro in quanto e una cazzo di lista e quindi niente
        free(list_txt);

        printf("TOKEN 0: %c, TOKEN 1: %c\n", (*token)[0], (*token)[1]);
        if ((*token)[0] == ']')
            (*token)++;
        printf("TOKEN: %s\n", *token);

        printf("\n|------------------------FINE LISTA------------------------------|\n");
    }
    else if((*token)[0] == '[' && !is_list)
    {
        printf("\n|INDEX|\n");
        printf("FINAL: %s\n", *token);
        //printf("PREVIOUS: %s\n", previous);
        circuitObj = create_obj();
        circuitObj->type = OBJ_INDEX;

        char *index_txt = malloc(sizeof(char)+1); 
        memcpy(index_txt, (*token)+1, 1);
        index_txt[1] = '\0';
        circuitObj->bracket.index = create_int_obj(atoi(index_txt));
        free(index_txt);
        printf("\nindex: %d\n", *circuitObj->bracket.index->i);
    }
    else if (circuitObj->type == 0)
    {
        circuitObj->type = OBJ_STR;
        circuitObj = create_str_obj(length+1);
        *circuitObj->str = final_token;
    }

    return circuitObj;
}

RowParser **parse(Circuit *circuit, CircuitObj *dest, char *txt, int add_separator)
{
    if(txt == NULL || txt[0] == '\0') return NULL;

    char *token = txt;
    RowParser **rows = NULL;
    circuit->context->rows = rows;

    int row_count = 0;
    int element_count = 0;
    while(token[0] != '\0')
    {
        element_count = 0;

        rows = realloc(rows, sizeof(RowParser*) * (row_count + 2));
        rows[row_count] = malloc(sizeof(RowParser));
        rows[row_count]->row = row_count+1;
        rows[row_count]->content = malloc(sizeof(CircuitObj));
        rows[row_count]->content->list.objs = NULL;
        rows[row_count]->content->list.count = 0;

        while(token[0] != ';' && token[0] != '\n' && token[0] != '\0')
        {
            CircuitObj *circuitObj = create_obj();

            while(isspace(token[0]) && token[0] != '\n' && token[0] != ';') token++;

            if(token[0] == '\0') break;
            if(token[0] == '\n') break;
            if(token[0] == ';') break;

            CircuitObj *previous_obj = NULL;
            if(dest->list.count > 0)
            {
                previous_obj = dest->list.objs[dest->list.count - 1];
            }
            if(previous_obj == NULL)
            {
                printf("WHAAT");
            }
            else
            {
                
            }

            if (token[0] == '\"')
            {
                circuitObj = parse_obj(circuit, &token, token, 0);
                append_obj_to_list(dest, circuitObj);
            }
            // if(token[0] == ']') token++;
            char *start = token;
            // char *end = token;

            CircuitObj *left_obj = NULL;
            while (isgraph(token[0]) && token[0] != ';')
            {
                char *previous = token - 1;
                
                if(token[0] == '[' && !isgraph(previous[0])) break;
                if (token[0] == '.' || (token[0] == '[' && isgraph(previous[0])))
                {
                    if (dest->list.count > 0)
                    {
                        previous_obj = dest->list.objs[dest->list.count - 1];
                    }
                    printf("GUAAAAA: %s\n", token);
                    // parsa quello a sinistra
                    // parsa quello a destra

                    if (left_obj == NULL && token[0] == '.')
                    {
                        left_obj = parse_obj(circuit, &token, start, 0);
                        append_obj_to_list(dest, left_obj);
                    }
                    else if (left_obj != NULL || (token[0] == '[' && isgraph(previous[0])))
                    {
                        CircuitObj *next = parse_obj(circuit, &token, start, 0);

                        switch (next->type)
                        {
                        case OBJ_DOT:
                            next->prop.left = malloc(sizeof(CircuitObj));
                            // circuitObj->prop.left = left_obj;
                            memcpy(next->prop.left, left_obj, sizeof(CircuitObj));
                            break;
                        case OBJ_INDEX:
                            next->bracket.left = malloc(sizeof(CircuitObj));
                            if(previous_obj != NULL && previous_obj->type == OBJ_LIST)
                                left_obj = previous_obj;
                            // circuitObj->bracket.left = left_obj;
                            memcpy(next->bracket.left, left_obj, sizeof(CircuitObj));
                            break;
                        default:
                            break;
                        }

                        append_obj_to_list(dest, next);
                        left_obj = next;
                    }

                    start = token;
                }
                
                token++;
            }

            if (circuitObj->type == 0 && left_obj != NULL)
            {
                circuitObj = parse_obj(circuit, &token, start, 0);
                if (dest->list.count > 0)
                {
                    previous_obj = dest->list.objs[dest->list.count - 1];
                }
                switch (circuitObj->type)
                {
                case OBJ_DOT:
                    printf("DOT\n");
                    circuitObj->prop.left = malloc(sizeof(CircuitObj));
                    // circuitObj->prop.left = left_obj;
                    memcpy(circuitObj->prop.left, left_obj, sizeof(CircuitObj));
                    break;
                case OBJ_INDEX:
                    printf("INDEX\n");
                    circuitObj->bracket.left = malloc(sizeof(CircuitObj));
                    if (previous_obj != NULL && previous_obj->type == OBJ_LIST)
                        left_obj = previous_obj;
                    // circuitObj->bracket.left = left_obj;
                    memcpy(circuitObj->bracket.left, left_obj, sizeof(CircuitObj));
                    break;
                default:
                    break;
                }

                append_obj_to_list(dest, circuitObj);
            }

            if (circuitObj->type == 0 && left_obj == NULL)
            {
                char *previous = token-1;
                if (token[0] == '[' && !isgraph(previous[0]))
                {
                    printf("LISTAAA!: %s\n", token);
                    circuitObj = parse_obj(circuit, &token, token, 1);
                    append_obj_to_list(dest, circuitObj);
                    add_separator = 0;
                }
                else
                {
                    circuitObj = parse_obj(circuit, &token, start, 0);
                    append_obj_to_list(dest, circuitObj);
                }
            }

            if(circuitObj != NULL)
            {
                //append_obj_to_list(dest, circuitObj);
                append_obj_to_list(rows[row_count]->content, circuitObj);
            }
            
            element_count++;
        }
        if(add_separator && element_count > 0)
        {
            CircuitObj *circuitObj = create_obj();
            append_obj_to_list(circuit->context->stack, circuitObj);
        }

        if(token[0] == ';' || token[0] == '\n') token++;
        if(token[0] != ';' && element_count > 0) row_count++;
    }
    rows[row_count] = NULL;
    return rows; 
}

CircuitObj *get_actual_value(CircuitObj *obj);

CircuitObj *search_prop(CircuitObj *obj, char *prop_name)
{
    for(int i = 0; i < obj->prop_count; i++)
    {
        if(strcmp(obj->props[i]->name, prop_name) == 0)
        {
            return obj->props[i]->prop;
        }
    }
    return NULL;
}

CircuitObj *get_prop_value(CircuitObj *dot_obj, char *prop_name)
{
    CircuitObj *real_obj = get_actual_value(dot_obj);
    return search_prop(real_obj, prop_name);
}

CircuitObj *get_actual_value(CircuitObj *obj)
{
    if(obj == NULL) return NULL;
    // Se è una variabile e ha un valore associato in 'data', restituiamo il suo valore
    if (obj != NULL && obj->type == OBJ_VAR)
    {
        if(obj->sys != NULL && (*obj->sys)->data != NULL)
        return get_actual_value((CircuitObj *)(*obj->sys)->data); 
    }
    if (obj->type == OBJ_DOT)
    {
        CircuitObj *left = obj->prop.left;

        return get_prop_value(left, obj->prop.name);
    }
    if (obj->type == OBJ_INDEX)
    {
        CircuitObj *left = get_actual_value(obj->bracket.left);
        return left->list.objs[*obj->bracket.index->i];
    }
    // Altrimenti è già un valore letterale (es. un INT o una STR) e lo restituiamo così com'è
    return obj;
}

CircuitObj *execute_func(Circuit *circuit, CircuitObj **stack, CircuitObj *currentToken, int i, int print)
{
    if(print) printf("FUNC TROVATA! (%s): ", (*currentToken->sys)->name);

    Symbol *currentFunc = NULL;
    currentFunc = *currentToken->sys;

    CircuitObj *params = malloc(sizeof(CircuitObj));
    params->type = OBJ_LIST;

    params->list.objs = NULL;
    params->list.count = 0;
    int param_size = 0;

    //int prop_index = i+1;
    int k = i + 1;
    while (currentFunc != NULL && stack[k] != NULL && stack[k]->type != 0 && stack[k]->type != OBJ_FUNC)
    {
        //print_objects(currentToken);

        //while (stack[k+1]->type == OBJ_DOT || stack[k+1]->type == OBJ_INDEX)
        //{
            //k++;
            //i++;
        //}
        

        /*
        if(stack[k+1]->type == OBJ_DOT || stack[k+1]->type == OBJ_INDEX)
        {
            //prop_index = i;
            while(stack[k+1]->type == OBJ_DOT || stack[k+1]->type == OBJ_INDEX)
            {
                if(stack[k+1]->type == OBJ_DOT)
                {
                    stack[k + 1]->prop.left = get_actual_value(stack[k]);
                    k++;
                    i++;
                }
                if(stack[k+1]->type == OBJ_INDEX)
                {
                    stack[k + 1]->bracket.left = get_actual_value(stack[k]);
                    k++;
                    i++;
                }
            }
            
            continue;
        }
        */

        param_size++;
        //printf("PARAM %d: %s %s ", k - i, return_print_objects(stack[k]), stack[k + 1]->type != 0 && stack[k + 1] != NULL && stack[k + 1]->type != 0 && stack[k + 1]->type != OBJ_FUNC ? "-" : "");
        append_obj_to_list(params, get_actual_value(stack[k]));
        // params->list.objs[param_size - 1] = currentToken;
        // params->list.objs[param_size] = NULL;

        k++;
    }
    if (currentFunc != NULL)
    {
        CircuitObj *returned_obj = ((CircuitFunc)currentFunc->data)(circuit, params, param_size);
        //if(print) printf("\n");
        free(params); // Devi liberare anche i figli della list obj
        if(print) printf("------------------------------------------\n");
        return returned_obj;
    }else
    {
        printf("\n");
        return NULL;
    }
}

void parse_getters(CircuitObj *currentToken, CircuitObj *previousToken, int current_i, int stack_count)
{
    //if (currentToken->type == OBJ_DOT)
    //{
        //currentToken->prop.left = get_actual_value(previousToken);
    //}
    //if (currentToken->type == OBJ_INDEX)
    //{
        //currentToken->bracket.left = get_actual_value(previousToken);
    //}

    /*
    if ((next != NULL) && (next->type == OBJ_DOT || next->type == OBJ_INDEX))
    {
        int k = i + 1;
        CircuitObj *left_obj = stack[i];
        while (stack[k]->type == OBJ_DOT || stack[k]->type == OBJ_INDEX)
        {
            if (stack[k]->type == OBJ_DOT)
            {
                stack[k]->prop.left = get_actual_value(left_obj);
                left_obj = get_actual_value(stack[k]);
                k++;
            }
            if (stack[k]->type == OBJ_INDEX)
            {
                stack[k]->bracket.left = get_actual_value(left_obj);
                left_obj = get_actual_value(stack[k]);
                k++;
            }
        }
        currentToken = get_actual_value(left_obj);
    }
    */
}

void execute_parsing(Circuit *circuit)
{
    CircuitObj **stack = circuit->context->stack->list.objs;
    int stack_count = circuit->context->stack->list.count;

    for(int i = 0; i < stack_count; i++)
    {
        if(stack[i+1]->type == OBJ_DOT || stack[i+1]->type == OBJ_INDEX)
        {
            while (stack[i+1]->type == OBJ_DOT || stack[i+1]->type == OBJ_INDEX)
            {
                i++;
            }
            continue;
        }
        CircuitObj *currentToken = stack[i];
        CircuitObj *previous = NULL; 
        CircuitObj *next = NULL;
        if(i > 0) previous = stack[i-1];
        if(i < stack_count-1) next = stack[i+1];

        //parse_getters(currentToken, previous);
        if (currentToken->type == OBJ_OP)
        {
            switch (currentToken->op->type)
            {
            case OP_ASSIGN:
            {
                //CircuitObj *
                previous = stack[i - 1];
                
                //CircuitObj *next = stack[i + 1];
                //CircuitObj *
                next = get_actual_value(next);

                if (i > 0 && i < stack_count - 1 && previous != NULL && next != NULL)
                {
                    if (previous->type == OBJ_VAR)
                    {
                        previous->mode = READ_ONLY;
                        next->mode = READ_ONLY;

                        if(next->type == OBJ_FUNC)
                        {
                            if(stack[i+2]->type == 0)
                            {
                                (*previous->sys)->data = next;
                            }else
                            {
                                (*previous->sys)->data = execute_func(circuit, stack, next, i+1, 0);
                            }                        
                        }else
                        {
                            (*previous->sys)->data = next;
                        }
                    }
                    if (previous->type == OBJ_DOT)
                    {
                        CircuitObj **prop_addr = NULL; 
                        CircuitObj *dot_obj = get_actual_value(previous->prop.left);
                        for (int i = 0; i < dot_obj->prop_count; i++)
                        {
                            if (strcmp(dot_obj->props[i]->name, previous->prop.name) == 0)
                            {
                                prop_addr = &dot_obj->props[i]->prop;
                            }
                        }
                        if((*prop_addr)->type != next->type) 
                        {
                            printf("TYPE MISMATCH!\n");
                            return;
                        }
                        if (next->type == OBJ_FUNC)
                        {
                            if(stack[i+2]->type == 0)
                            {
                                *(prop_addr) = next;
                            }else
                            {
                                *(prop_addr) = execute_func(circuit, stack, next, i+1, 0);
                            }                        
                        }else
                        {
                            //*(prop_addr) = next; // PROBABILMENTE QUA DEVI ACCEDERE AL TIPO CORRETTO ATTRAVERSO INDIRIZZO (DAL DOPPIO PUNTATORE) E CAMBIARLO
                            {
                                CircuitObj *prop_obj = *prop_addr;
                                switch (prop_obj->type)
                                {
                                case OBJ_STR:
                                    *(prop_obj->str) = *(next->str);
                                    break;
                                case OBJ_INT:
                                    *(prop_obj->i) = *(next->i);
                                    break;
                                default:
                                    break;
                                }
                            }
                        }
                    }
                    if (previous->type == OBJ_INDEX)
                    {
                        CircuitObj **prop_addr = NULL; 
                        CircuitObj *bracket_obj = get_actual_value(previous->bracket.left);
                        //print_objects(bracket_obj);

                        prop_addr = &bracket_obj->list.objs[*previous->bracket.index->i];
                        //print_objects(*prop_addr);
                        //if((*prop_addr)->type != next->type) 
                        //{
                            //printf("TYPE MISMATCH!\n");
                            //return;
                        //}
                        if (next->type == OBJ_FUNC)
                        {
                            if(stack[i+2]->type == 0)
                            {
                                *(prop_addr) = next;
                            }else
                            {
                                *(prop_addr) = execute_func(circuit, stack, next, i+1, 0);
                            }                        
                        }else
                        {
                            //*(prop_addr) = next; // PROBABILMENTE QUA DEVI ACCEDERE AL TIPO CORRETTO ATTRAVERSO INDIRIZZO (DAL DOPPIO PUNTATORE) E CAMBIARLO
                            {
                                CircuitObj *prop_obj = *prop_addr;
                                switch (prop_obj->type)
                                {
                                case OBJ_STR:
                                    *(prop_obj->str) = *(next->str);
                                    break;
                                case OBJ_INT:
                                    *(prop_obj->i) = *(next->i);
                                    break;
                                default:
                                    break;
                                }
                            }
                        }
                    }
                }
                // prendi obj prima e obj dopo
                // controlla che esistano
                // controlla che obj prima sia una var
                // assegna al data della var il obj dopo
                //printf("VAR VALUE: %s", return_print_objects(previous->sys->data));
                i++;
                break;

            }
            case OP_ADD:
                if (i > 0 && i < stack_count - 1)
                {
                    CircuitObj *left = get_actual_value(stack[i - 1]);
                    CircuitObj *right = get_actual_value(stack[i + 1]);

                    if (left != NULL && right != NULL && left->type == OBJ_INT && right->type == OBJ_INT)
                    {
                        // 1. Calcola la somma e crea il nuovo oggetto
                        int sum_val = *(left->i) + *(right->i);
                        CircuitObj *result_obj = create_int_obj(sum_val);

                        // 2. Sostituisci il primo elemento con il risultato
                        stack[i - 1] = result_obj;

                        // 3. Compatta lo stack eliminando l'operatore e il secondo operando
                        for (int k = i; k < stack_count - 2; k++)
                        {
                            stack[k] = stack[k + 2];
                        }

                        // 4. Aggiorna i contatori dello stack
                        circuit->context->stack->list.count -= 2;
                        stack_count -= 2;

                        // 5. Riporta l'indice indietro di uno per bilanciare il ciclo
                        i--;
                    }
                    else
                    {
                        printf("ERRORE: Operandi non validi per la somma (+)\n");
                    }
                }
                break;
            default:
                break;
            }
        }
        
        currentToken = stack[i];
        /*
        if ((next != NULL) && (next->type == OBJ_DOT || next->type == OBJ_INDEX))
        {
            int k = i + 1;
            CircuitObj *left_obj = stack[i];
            while (stack[k]->type == OBJ_DOT || stack[k]->type == OBJ_INDEX)
            {
                if (stack[k]->type == OBJ_DOT)
                {
                    stack[k]->prop.left = get_actual_value(left_obj);
                    left_obj = get_actual_value(stack[k]);
                    k++;
                }
                if (stack[k]->type == OBJ_INDEX)
                {
                    stack[k]->bracket.left = get_actual_value(left_obj);
                    left_obj = get_actual_value(stack[k]);
                    k++;
                }
            }
            currentToken = get_actual_value(left_obj);
        }
        */
        if(currentToken != NULL && currentToken->type == OBJ_VAR)
        {
            CircuitObj *innerObj = get_actual_value(stack[i]);

            if(i < stack_count - 1 && stack[i+1]->type != OBJ_OP) innerObj->mode = READ_WRITE;
            currentToken = innerObj;
        }

        switch (currentToken->type)
        {
            case OBJ_FUNC:
                if(currentToken->mode != READ_WRITE) break;
                CircuitObj *returned_obj = execute_func(circuit, stack, currentToken, i, 1);
                if(returned_obj == NULL) {printf("OBJ: \"%s\" CRASHED!\n", return_print_objects(currentToken)); return;}
                break;
            
            default:
                break;
        } 
    }

    /*
    for(int i = 0; i < stack_count; i++)
    {
        CircuitObj *currentToken = stack[i];
        CircuitObj *nextToken = NULL;
        if(i + 1 < stack_count-1) nextToken = stack[i+1];

        if(nextToken != NULL && nextToken->type == OBJ_DOT)
        {
            currentToken = get_actual_value(nextToken);
        }
        else if(currentToken != NULL && currentToken->type == OBJ_VAR)
        {
            CircuitObj *innerObj = get_actual_value(stack[i]);

            if(i < stack_count - 1 && stack[i+1]->type != OBJ_OP) innerObj->mode = READ_WRITE;
            currentToken = innerObj;
        }

        switch (currentToken->type)
        {
            case OBJ_FUNC:
                if(currentToken->mode != READ_WRITE) break;
                CircuitObj *returned_obj = execute_func(circuit, stack, currentToken, i, 1);
                if(returned_obj == NULL) {printf("OBJ: \"%s\" CRASHED!\n", return_print_objects(currentToken)); return;}
                break;
            
            default:
                break;
        } 
    }
    */
}

Wire *find_wire_by_name(Circuit *circuit, char *name);

Wire **wires_by_params(Circuit *circuit, CircuitObj *params, int count)
{
    Wire **wire_list = create_wire_list(circuit, "wire_list", 0, count);
    for(int i = 0; i < count; i++)
    {
        char *name = *params->list.objs[i]->str;
        Wire *wire = find_wire_by_name(circuit, name);
        if(wire == NULL)
        {
            wire = create_wire(circuit, name, 0);
        }
        wire_list[i] = wire;
    }
    return wire_list;
}

void add_wire_props(CircuitObj *circuitObj)
{
    CircuitObj *name_prop = create_obj();
    name_prop->type = OBJ_STR;
    name_prop->str = &((*circuitObj->wire)->name);
    add_prop(circuitObj, name_prop, "name");

    CircuitObj *power_prop = create_obj();
    power_prop->type = OBJ_INT;
    power_prop->i = &((*circuitObj->wire)->power);
    add_prop(circuitObj, power_prop, "power");
}

void *add_tran_props(CircuitObj *circuitObj)
{
    CircuitObj *name_prop = create_obj();
    name_prop->type = OBJ_STR;
    name_prop->str = &((*circuitObj->transistor)->name);
    add_prop(circuitObj, name_prop, "name");
   
    CircuitObj *state_prop = create_obj();
    state_prop->type = OBJ_INT;
    state_prop->i = &((*circuitObj->transistor)->state);
    add_prop(circuitObj, state_prop, "state");

    //CircuitObj *in_wire = create_obj();
    //in_wire->type = OBJ_WIRE;
    //in_wire->wire = &((*circuitObj->transistor)->input_wire);
    //add_prop(circuitObj, in_wire, "input_wire");

    CircuitObj *in_wire = create_list_obj();
    for(int i = 0; i < (*circuitObj->transistor)->count; i++)
    {
        CircuitObj *wire_obj = create_obj();
        wire_obj->type = OBJ_WIRE;
        wire_obj->wire = &((*circuitObj->transistor)->input_wire[i]);

        add_wire_props(wire_obj);
        append_obj_to_list(in_wire, wire_obj);
    }
    add_prop(circuitObj, in_wire, "input_wires");

    CircuitObj *out_wire = create_obj();
    out_wire->type = OBJ_WIRE;
    out_wire->wire = &((*circuitObj->transistor)->output_wire);
    add_prop(circuitObj, out_wire, "out_wire");

    CircuitObj *control_wire = create_obj();
    control_wire->type = OBJ_WIRE;
    control_wire->wire = &((*circuitObj->transistor)->control_wire);
    add_prop(circuitObj, control_wire, "control_wire");

    //char *name;
    //int state; // if it's closed or open
    //int invert;
    //struct Wire **input_wire;
    //struct Wire *output_wire;
    //struct Wire *control_wire;
    //int count;

    // NUOVO: registra le props sul wire object stesso
    add_wire_props(out_wire); 
    add_wire_props(control_wire);
}

CircuitObj *Or_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    /*
    Wire *a_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[1]->str, 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;
    */
    CircuitObj *circuitObj = create_tran_obj("OR_TRAN", 1);//malloc(sizeof(CircuitObj));
    //Wire **wire_list = wires_by_params(circuit, params, count);
    Wire *a_wire = *params->list.objs[0]->wire;
    Wire *b_wire = *params->list.objs[1]->wire;

    Transistor *or_gate = create_OR_Gate(circuit, a_wire, b_wire);

    circuitObj->type = OBJ_TRAN;
    *circuitObj->transistor = or_gate;
    add_tran_props(circuitObj);

    return circuitObj;
}

CircuitObj *And_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_tran_obj("AND_TRAN", 1);
    /*
    char *input_name = params->list.objs[0]->str;
    char *a_name = params->list.objs[1]->str;
    char *b_name = params->list.objs[2]->str;
    Wire *input_wire = find_wire_by_name(circuit, input_name);
    Wire *a_wire = find_wire_by_name(circuit, a_name);
    Wire *b_wire = find_wire_by_name(circuit, b_name);
    
    if(input_wire == NULL)
    {
        input_wire = create_wire(circuit, input_name, 0);
    }
    if(a_wire == NULL)
    {
        a_wire = create_wire(circuit, a_name, 0);
    }
    if(b_wire = NULL)
    {
        b_wire = create_wire(circuit, b_name, 0);
    }
    */
    //Wire **wire_list = wires_by_params(circuit, params, count);
    Wire *input_wire = *params->list.objs[0]->wire;
    Wire *a_wire = *params->list.objs[1]->wire;
    Wire *b_wire = *params->list.objs[2]->wire;

    Transistor *and_gate = create_AND_Gate(circuit, input_wire, a_wire, b_wire);

    circuitObj->type = OBJ_TRAN;
    *circuitObj->transistor = and_gate;
    add_tran_props(circuitObj);

    return circuitObj;
}

CircuitObj *Not_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_tran_obj("NOT_TRAN", 1);
    Wire **wire_list = wires_by_params(circuit, params, count);

    Transistor *and_gate = create_NOT_Gate(circuit, wire_list[0], wire_list[1]);

    circuitObj->type = OBJ_TRAN;
    *circuitObj->transistor = and_gate;
    add_tran_props(circuitObj);

    return circuitObj;
}

CircuitObj *Nor_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_tran_obj("NOR_TRAN", 1);
    /*
    Wire *a_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[1]->str, 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;
    */

    //Wire **wire_list = wires_by_params(circuit, params, count);
    Wire *a_wire = *params->list.objs[0]->wire;
    Wire *b_wire = *params->list.objs[1]->wire;

    Transistor *nor_gate = create_NOR_Gate(circuit, a_wire, b_wire);

    circuitObj->type = OBJ_TRAN;
    *circuitObj->transistor = nor_gate;
    add_tran_props(circuitObj);

    return circuitObj;
}

CircuitObj *Nand_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_tran_obj("NAND_TRAN", 1);
    /*
    Wire *input_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *a_wire = create_wire(circuit, params->list.objs[1]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[2]->str, 0);
    */

    Wire **wire_list = wires_by_params(circuit, params, count);

    Transistor *nand_gate = create_NAND_Gate(circuit, wire_list[0], wire_list[1], wire_list[2]);

    circuitObj->type = OBJ_TRAN;
    *circuitObj->transistor = nand_gate;
    add_tran_props(circuitObj);

    return circuitObj;
}

CircuitObj *Xor_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_tran_obj("XOR_TRAN", 1);
    
    printf("XOR params: %s e %s\n", params->list.objs[0]->str, params->list.objs[1]->str);
    if (params == NULL || params->list.count < 2)
    {
        printf("[XOR_WRAP ERROR] Numero di parametri insufficiente! Ricevuti: %d\n", 
                params ? params->list.count : 0);
        return NULL;
    }

    printf("\n");
    /* 
    Wire *a_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[1]->str, 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;
    */

    //Wire **wire_list = wires_by_params(circuit, params, count);
    Wire *a_wire = *params->list.objs[0]->wire;
    Wire *b_wire = *params->list.objs[1]->wire;

    Transistor *xor_gate = create_XOR_Gate(circuit, a_wire, b_wire);

    circuitObj->type = OBJ_TRAN;
    *circuitObj->transistor = xor_gate;
    add_tran_props(circuitObj);

    return circuitObj;
}

Transistor *find_transistor_by_name(Circuit *circuit, char *name)
{
    for(int i = 0; i < circuit->transistor_count; i++)
    {
        if(strcmp(circuit->transistors[i]->name, name) == 0)
        {
            return circuit->transistors[i];
        }
    }
    return NULL;
}

Wire *find_wire_by_name(Circuit *circuit, char *name)
{
    for(int i = 0; i < circuit->wire_count; i++)
    {
        if(strcmp(circuit->wires[i]->name, name) == 0)
        {
            return circuit->wires[i];
        }
    }
    return NULL;
}


/*
void *Update_Transistor_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Transistor *transistor = find_transistor_by_name(circuit, params->list.objs[0]->str);
    Wire **input_wires = create_wire_list(circuit, "Input_Wires", 0, 2);

    Wire *input_1 = find_wire_by_name(circuit, params->list.objs[1]->list.objs[0]->str);
    if(input_1 == NULL) input_1 = create_wire(circuit, params->list.objs[1]->list.objs[0]->str, 0);
    Wire *input_2 = find_wire_by_name(circuit, params->list.objs[2]->list.objs[0]->str);
    if(input_2 == NULL) input_2 = create_wire(circuit, params->list.objs[2]->list.objs[0]->str, 0);
    input_wires[0] = input_1;
    input_wires[1] = input_2;

    Wire *output_wire = find_wire_by_name(circuit, params->list.objs[2]->list.objs[0]->str);
    if(output_wire == NULL) output_wire = create_wire(circuit, params->list.objs[2]->list.objs[0]->str, params->list.objs[1]->list.objs[1]->i);
    Wire *control_wire = find_wire_by_name(circuit, params->list.objs[3]->list.objs[0]->str);
    if(control_wire == NULL) control_wire = create_wire(circuit, params->list.objs[3]->list.objs[0]->str, params->list.objs[1]->list.objs[1]->i);;

    if(transistor != NULL)
    {
        transistor->input_wire = input_wires;
        transistor->output_wire = output_wire;
        transistor->control_wire = control_wire;
    }
}
*/

void *Update_Wire_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Wire *wire = find_wire_by_name(circuit, *params->list.objs[0]->str);
    int power = 0;
    int driven = 0;
   
    if(wire != NULL)
    {
        if(params->list.objs[1] != NULL) {power = *params->list.objs[1]->i;} else {power = wire->power;}
        if(params->list.objs[2] != NULL) {driven = *params->list.objs[2]->i;} else {driven = wire->driven;}
        wire->driven = driven;
        wire->power = power;
    } 

}

CircuitObj *Wire_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    char *name = strdup(*params->list.objs[0]->str);
    int power = *params->list.objs[1]->i;
    int driven = *params->list.objs[2]->i;

    CircuitObj *circuitObj = create_wire_obj(circuit, name, power);
    (*circuitObj->wire)->driven = driven; 
     
    add_wire_props(circuitObj);
    //CircuitObj *name_prop = create_obj();
    //name_prop->type = OBJ_STR;
    //name_prop->str = &((*circuitObj->wire)->name);
    //add_prop(circuitObj, name_prop, "name");
    
    //CircuitObj *power_prop = create_obj();
    //power_prop->type = OBJ_INT;
    //power_prop->i = &((*circuitObj->wire)->power);
    //add_prop(circuitObj, power_prop, "power");
   
    /* 
    circuitObj->props = malloc(sizeof(ObjProp*));
    circuitObj->props[0] = malloc(sizeof(ObjProp));
    circuitObj->props[0]->name = "name";
    circuitObj->props[0]->prop = malloc(sizeof(CircuitObj*));

    (*circuitObj->props[0]->prop) = malloc(sizeof(CircuitObj));
    (*circuitObj->props[0]->prop)->type = OBJ_STR;
    (*circuitObj->props[0]->prop)->str = circuitObj->wire->name;
    circuitObj->prop_count = 1;
    */

    return circuitObj;
}

CircuitObj *Print_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    for(int i = 0; i < count; i++)
    {
        CircuitObj *obj = get_actual_value(params->list.objs[i]);
        if(obj == NULL) {printf("OBJ NULLO\n"); return NULL;}
        printf("\n");
        if(obj->type == OBJ_WIRE)
        {
            printf(">> WIRE '%s' -> Power: %d\n", (*obj->wire)->name, (*obj->wire)->power);
        }
        else if(obj->type == OBJ_TRAN)
        {
            printf(">> GATE '%s' -> Out Power: %d\n", (*obj->transistor)->name, (*obj->transistor)->output_wire ? (*obj->transistor)->output_wire->power : -1);
        }
        else if(obj->type == OBJ_STR)
        {
            printf(">> STR -> %s\n", *obj->str);
        }
        else if(obj->type == OBJ_INT)
        {
            printf(">> INT -> %d\n", *obj->i);
        }
        else if(obj->type == OBJ_LIST)
        {
            printf(">> ");
            print_objects(obj);
            //Print_Wrap(circuit, obj, obj->list.count); 
        }
    }
    CircuitObj *void_obj = malloc(sizeof(CircuitObj));
    void_obj->type = OBJ_VOID;
    return void_obj;
}

Circuit *create_circuit()
{
    Circuit *circuit = malloc(sizeof(*circuit));

    circuit->state = 0;
    circuit->transistors = NULL; // Setto a NUll, ovvero a un puntatore void che punta a 0
    circuit->transistor_count = 0;

    circuit->wires = NULL;    
    circuit->wire_count = 0;
    circuit->context = malloc(sizeof(Context));  
    circuit->context->symTable = malloc(sizeof(SymbolTable));
    circuit->context->symTable->sym_list = NULL;
    circuit->context->symTable->sym_count = 0;

    //circuit->context->opTable = malloc(sizeof(OperatorTable));    
    //circuit->context->opTable->ops = NULL;
    //circuit->context->opTable->count = 0;

    circuit->context->stack = malloc(sizeof(CircuitObj));
    circuit->context->stack->list.objs = NULL;
    circuit->context->stack->list.count = 0;

    return circuit;
}

CircuitObj *Run_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    CircuitObj *void_obj = malloc(sizeof(CircuitObj));
    void_obj->type = OBJ_VOID;
    run_circuit(circuit);
    return void_obj;
}

int main(void)
{
    Circuit *circuit = create_circuit();

    append_sym_to_table(circuit, create_symbol("OR", Or_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("AND", And_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("NOT", Not_Wrap, SYM_FUNC, 2));
    append_sym_to_table(circuit, create_symbol("NOR", Nor_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("NAND", Nand_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("XOR", Xor_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("UPDATE_W", Update_Wire_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("WIRE", Wire_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("PRINT", Print_Wrap, SYM_FUNC, 1));
    append_sym_to_table(circuit, create_symbol("RUN", Run_Wrap, SYM_FUNC, 0));

    FILE *fp = fopen("circuit.txt", "rb");
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);

    char *circuit_txt = malloc(file_size+1);
    fseek(fp, 0, SEEK_SET);

    fread(circuit_txt, file_size, 1, fp);
    circuit_txt[file_size] = '\0';
    fclose(fp);
    
    RowParser **string_list = parse(circuit, circuit->context->stack, circuit_txt,1);

    
    printf("----------");
    printf("\nSTART - PRIMA\n");
    for(int i = 0; i < circuit->context->stack->list.count; i++)
    {
        printf("\n");
        printf("%d) --- \n", i+1);
        print_objects(circuit->context->stack->list.objs[i]);
    }
    printf("FINE\n");
    

    execute_parsing(circuit);

    
    printf("----------");
    printf("\nSTART - DOPO\n");
    for(int i = 0; i < circuit->context->stack->list.count; i++)
    {
        print_objects(circuit->context->stack->list.objs[i]);
    }
    printf("FINE\n");
    

    return 0;
}