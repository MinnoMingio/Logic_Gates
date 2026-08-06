// FAI UN PROGRAMMA FIGO CON MMAP (MAGARI FILE .txt e .pdf e .exe)
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>

#define OBJ_INT  1
#define OBJ_STR  2
#define OBJ_LIST 3
#define OBJ_FUNC 4
#define OBJ_VAR  5

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
    OP_MATH,
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

typedef struct OperatorTable
{
    Operator **ops;
    int count;
} OperatorTable;

typedef struct CircuitObj 
{
    int type;
    union
    {
        int i;
        char *str;
        Symbol *sys;
        Operator *op;
        Wire *wire;
        Transistor *transistor;
        struct List
        {
            CircuitObj **objs;
            int count;
        } list;
    };
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
    OperatorTable *opTable;
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

    transistor->name = name;
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
            int len = strlen(obj->str) + 6; // "str: " + stringa + '\0'
            result = malloc(len);
            snprintf(result, len, "str: %s", obj->str);
            break;
        }
        case OBJ_LIST:
            size_t cap = 64;
            int _len = 0;
            result = malloc(cap);
            if (!result) return NULL;

            // Prefisso iniziale
            _len += snprintf(result, cap, "list: [");

            for (size_t i = 0; i < obj->list.count; i++)
            {
                // Chiamata ricorsiva per l'elemento corrente
                char *item_str = return_print_objects(obj->list.objs[i]);
                if (!item_str) continue;

                size_t item_len = strlen(item_str);
                // Separatore ", " tra gli elementi
                size_t needed = _len + item_len + (i > 0 ? 2 : 0) + 2; // +2 per "]" e '\0'

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
                    _len += 2;
                }

                strcat(result, item_str);
                _len += item_len;
                free(item_str); // Liberiamo la stringa temporanea del singolo elemento
            }

            // Chiusura della parentesi quadra
            if (_len + 2 > cap)
            {
                result = realloc(result, _len + 2);
            }
            strcat(result, "]");
            break;
        case OBJ_FUNC:
        {
            int len = strlen(obj->sys->name) + 7; // "func: " + nome + '\0'
            result = malloc(len);
            snprintf(result, len, "func: %s", obj->sys->name);
            break;
        }
        case OBJ_VAR:
            int len = strlen(obj->sys->name) + 7; // "func: " + nome + '\0'
            result = malloc(len);
            snprintf(result, len, "var: %s", obj->sys->name);
            break;
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
            printf("int: %d\n", obj->i);
            break;
        case OBJ_STR:
            printf("str: %s\n", obj->str);
            break;
        case OBJ_LIST:
            printf("list: %d\n", obj->list.count);
            for(int i = 0; i < obj->list.count; i++)
            {
                printf("   %d - ", i);
                print_objects(obj->list.objs[i]);
            }
            break;
        case OBJ_FUNC:
            printf("func: %s\n", obj->sys->name);
            break;
        case OBJ_VAR:
            printf("var: %s\n", obj->sys->name);
        case 0:
            printf("SEPARATO\n");
        default:
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
        Wire **input_wires = transistor->input_wire;

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

Transistor *create_OR_Gate(Circuit *circuit, Wire **input_wire)
{
    Transistor *transistor1 = create_transistor("OR_1", 1);
    Transistor *transistor2 = create_transistor("OR_1", 1);
    
    update_transistor(transistor1, 1, 0);
    update_transistor(transistor2, 1, 0);

    Wire *wire1_2 = create_wire(circuit, "2_3", 0);

    transistor1->input_wire[0] = input_wire[0];
    transistor1->output_wire = wire1_2;

    transistor2->input_wire[0] = input_wire[1];
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

    Wire *mid = create_wire(circuit, "mid", 0); 
    Wire *out = create_wire(circuit, "out", 0);

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

Transistor *create_NOT_Gate(Circuit *circuit, Wire *input_wire)
{
    Transistor *transistor1 = create_transistor("NOT", 1);
    
    update_transistor(transistor1, 0, 1);

    Wire *wire1 = create_wire(circuit, "1", 0);

    transistor1->input_wire[0] = input_wire;
    transistor1->output_wire = wire1;

    append_t_to_c(circuit, transistor1);

    return transistor1;
}

Transistor *create_NOR_Gate(Circuit *circuit, Wire **input_wire)
{
    Transistor *or_gate = create_OR_Gate(circuit, input_wire);
    return create_NOT_Gate(circuit, or_gate->output_wire);
}

Transistor *create_NAND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b)
{
    Transistor *and_gate = create_AND_Gate(circuit, input_wire, a, b);
    return create_NOT_Gate(circuit, and_gate->output_wire);
}

Transistor *create_XOR_Gate(Circuit *circuit, Wire **input_wire)
{
    //quindi prende i due segnali (0,0) o (1,1) e in pratica (0, 0)
    //restituisce 1 mentre (1,1) restituisce zero. L'ultimo NOR
    //vedendo un segnale unito di (1,0) restituisce 0, mentre invece
    //se fossero (1,0) sin dall'inizio, allora il nor di (1,0)
    //restituisce 0 e poi Il NOR dei valori invertiti (0,1) restituisce
    //comunque zero e allora all'ultimo NOR arrivano due zeri (0,0) e restituirà 1.
    //Quindi se sono diversi restituisce 1 mentre se sono uguali restituisce zero.

    Wire *a = input_wire[0];
    Wire *b = input_wire[1];
    // Passo 1: Invertiamo i singoli segnali
    Transistor *not_a_wire = create_NOT_Gate(circuit, a);
    Transistor *not_b_wire = create_NOT_Gate(circuit, b);

    // Passo 2: NOR tra A e B (è vero solo se A=0 e B=0)
    Transistor *nor_ab = create_NOR_Gate(circuit, input_wire);
    
    // Passo 3: NOR tra NOT_A e NOT_B (è vero solo se A=1 e B=1)
    Wire **not_a_b_wires = malloc(sizeof(Wire*) * 2); 
    not_a_b_wires[0] = not_a_wire->output_wire;
    not_a_b_wires[1] = not_b_wire->output_wire;
    Transistor *nor_not_ab = create_NOR_Gate(circuit, not_a_b_wires);
   
    free(not_a_b_wires); 
    
    // Passo 4: Se è VERO che (A=0, B=0) oppure è VERO che (A=1, B=1), 
    // l'XOR deve dare 0. Il NOR finale fa proprio questo controllo!
    Wire **nor_ab_nor_not_ab = malloc(sizeof(Wire*) * 2);
    nor_ab_nor_not_ab[0] = nor_ab->output_wire;
    nor_ab_nor_not_ab[1] = nor_not_ab->output_wire;

    Transistor *xor = create_NOR_Gate(circuit, nor_ab_nor_not_ab);     
    xor->name = "XOR";
    free(nor_ab_nor_not_ab);
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
    Wire **inputs = malloc(sizeof(Wire*) * 2);
    inputs[0] = a;
    inputs[1] = b;

    // 1. Somma: XOR tra A e B
    Transistor *sum_gate = create_XOR_Gate(circuit, inputs);

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
    Transistor *not_gate = create_NOT_Gate(circuit, loop_wire);

    // Colleghiamo l'uscita del NOT direttamente al cavo di ingresso
    not_gate->output_wire = loop_wire; 
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

CircuitObj *parse_obj(Circuit *circuit, char **token, char *start)
{
    CircuitObj *circuitObj = malloc(sizeof(CircuitObj));
    circuitObj->type = 0;

    char *end = *token;

    int length = end - start;
    char *final_token = malloc(length + 1);
    memcpy(final_token, start, length);
    final_token[length] = '\0';
    
    Symbol *sys = NULL;
    for (int i = 0; i < circuit->context->symTable->sym_count; i++)
    {
        if(circuit->context->symTable->sym_list[i]->type == SYM_FUNC)
        {
            if (strcmp(final_token, circuit->context->symTable->sym_list[i]->name) == 0)
            {
                circuitObj->type = OBJ_FUNC;
                sys = circuit->context->symTable->sym_list[i];
                circuitObj->sys = sys;
                break;
            }
        }
    }
    if (strcmp(final_token, "var") == 0)
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

        int found = 0;
        sys = find_var_by_name(circuit, var_name); 

        if(sys == NULL)
        {
            sys = create_symbol(var_name, NULL, SYM_VAR, 1);

            CircuitObj *var_value = parse_obj(circuit, token, start);
            sys->data = var_value;
            print_objects(var_value);

            append_sym_to_table(circuit, sys);

            circuitObj->type = OBJ_VAR;
            circuitObj->sys = sys;
        }else
        {
            circuitObj->type = OBJ_VAR;
            circuitObj->sys = sys;
        }
    }
    else if (isdigit(final_token[0]))
    {
        circuitObj->type = OBJ_INT;
        circuitObj->i = atoi(final_token);
    }else if(final_token[0] == '"' && final_token[length-1] == '"')
    {
        circuitObj->type = OBJ_STR;
        int new_length = length - 2;
        circuitObj->str = malloc(new_length + 1);
        memcpy(circuitObj->str, start + 1, new_length);
        circuitObj->str[new_length] = '\0';
    }
    else if (circuitObj->type == 0)
    {
        Symbol *sys = find_var_by_name(circuit, final_token);
        if(sys == NULL)
        {
            circuitObj->type = OBJ_STR;
            circuitObj->str = final_token;
        }
        else
        {
            circuitObj->type = OBJ_VAR;
            circuitObj->sys = sys;
        }
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
            CircuitObj *circuitObj = malloc(sizeof(CircuitObj));
            circuitObj->type = 0;

            while(isspace(token[0]) && token[0] != '\n' && token[0] != ';') token++;

            if(token[0] == '\0') break;
            if(token[0] == '\n') break;
            if(token[0] == ';') break;

            int is_list = 0;

            if (token[0] == '[')
            {
                is_list = 1;
                printf("START: %s\n", token);
                char *new_start = token;
                while (token[0] != ']')
                {
                    token++;
                }
                token++;    
                printf("END: %s\n", token);
                char *new_end = token;
                printf("%c", new_end[0]);
                circuitObj->type = OBJ_LIST;
                circuitObj->list.objs = NULL;
                circuitObj->list.count = 0;

                int new_length = new_end - new_start;
                char *list_txt = malloc(new_length + 1);
                memcpy(list_txt, new_start + 1, new_length);
                list_txt[new_length] = '\0';
                parse(circuit, circuitObj, list_txt, 0);
            }

            if(!is_list)
            {
                char *start = token;

                while (isgraph(token[0]) && token[0] != ';')
                    token++;

                circuitObj = parse_obj(circuit, &token, start);

                // final_string = realloc(final_string, sizeof(char*) * (count+2));
                // final_string[count] = final_token;
            }
            
            if(circuitObj != NULL)
            {
                //rows[row_count]->content->list.objs = realloc(rows[row_count]->content->list.objs, sizeof(CircuitObj*) * (element_count+2));
                //rows[row_count]->row = row_count+1;
                //rows[row_count]->content->list.objs[element_count] = circuitObj;
                //rows[row_count]->content->list.objs[element_count+1] = NULL; // NULL terminator

                append_obj_to_list(dest, circuitObj);   
                append_obj_to_list(rows[row_count]->content, circuitObj);
            }
            
            element_count++;
        }
        if(add_separator)
        {
            CircuitObj *circuitObj = malloc(sizeof(CircuitObj));
            circuitObj->type = 0;
            append_obj_to_list(circuit->context->stack, circuitObj);
        }

        if(token[0] == ';' || token[0] == '\n') token++;
        if(token[0] != ';' && element_count > 0) row_count++;
        printf("\n");
    }
    //final_string[count] = NULL;
    rows[row_count] = NULL;
    printf("\n\nend\n");
    return rows; 
}

void execute_parsing(Circuit *circuit)
{
    CircuitObj **stack = circuit->context->stack->list.objs;
    int stack_count = circuit->context->stack->list.count;

    for(int i = 0; i < stack_count; i++)
    {
        CircuitObj *currentToken = stack[i];
        Symbol *currentFunc = NULL;
        
        printf("\n");
        for(int j = 0; j < circuit->context->symTable->sym_count; j++)
        {
            if(currentToken->type == OBJ_FUNC)
            {
                if(strcmp(currentToken->sys->name, circuit->context->symTable->sym_list[j]->name) == 0)
                {
                    printf("TROVATA! (%s): ", circuit->context->symTable->sym_list[j]->name);
                    currentFunc = circuit->context->symTable->sym_list[j];
                    //params = malloc(sizeof(char *) * (currentFunc->param_count + 1));
                }

                CircuitObj *params = malloc(sizeof(CircuitObj));

                params->list.objs = NULL;
                params->list.count = 0;
                int param_size = 0;

                int k = i + 1;
                while (currentFunc != NULL && stack[k] != NULL && stack[k]->type != 0 && stack[k]->type != OBJ_FUNC)
                {
                    param_size++;
                    printf("PARAM %d: %s %s ", k - i, return_print_objects(stack[k]), stack[k+1]->type != 0 && stack[k+1] != NULL && stack[k+1]->type != 0 && stack[k+1]->type != OBJ_FUNC? "-" : "" );
                    append_obj_to_list(params, stack[k]);
                    // params->list.objs[param_size - 1] = currentToken;
                    // params->list.objs[param_size] = NULL;

                    k++;
                }
                if (currentFunc != NULL)
                {
                    ((CircuitFunc)currentFunc->data)(circuit, params, param_size);
                    i = k - 1;
                    break;
                }
            }

            if(currentToken->type == OBJ_VAR)
            {
                if(strcmp(currentToken->sys->name, circuit->context->symTable->sym_list[j]->name) == 0)
                {
                    printf("TROVATA! (%s): ", circuit->context->symTable->sym_list[j]->name);
                    currentFunc = circuit->context->symTable->sym_list[j];
                    //params = malloc(sizeof(char *) * (currentFunc->param_count + 1));
                }
            }
        }
    }
}

void *Or_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Wire *a_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[1]->str, 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;

    Transistor *and_gate = create_OR_Gate(circuit, input_wires);
}

void *And_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Wire *input_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *a_wire = create_wire(circuit, params->list.objs[1]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[2]->str, 0);

    Transistor *and_gate = create_AND_Gate(circuit, input_wire, a_wire, b_wire);
}

void *Not_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Wire *input_wire = create_wire(circuit, params->list.objs[0]->str, 0);

    Transistor *and_gate = create_NOT_Gate(circuit, input_wire);
}

void *Nor_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Wire *a_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[1]->str, 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;

    Transistor *and_gate = create_NOR_Gate(circuit, input_wires);
}

void *Nand_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Wire *input_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *a_wire = create_wire(circuit, params->list.objs[1]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[2]->str, 0);

    Transistor *and_gate = create_NAND_Gate(circuit, input_wire, a_wire, b_wire);
}

void *Xor_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    printf("XOR params: %s e %s\n", params->list.objs[0]->str, params->list.objs[1]->str);
    if (params == NULL || params->list.count < 2)
    {
        printf("[XOR_WRAP ERROR] Numero di parametri insufficiente! Ricevuti: %d\n", 
                params ? params->list.count : 0);
        return NULL;
    }

    printf("\n");
    
    Wire *a_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[1]->str, 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;

    Transistor *xor_gate = create_XOR_Gate(circuit, input_wires);
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


void *Update_Transistor_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Transistor *transistor = find_transistor_by_name(circuit, params->list.objs[0]->str);
    Wire **input_wires = create_wire_list(circuit, "Input_Wires", 0, 2);

    Wire *input_1 = create_wire(circuit, params->list.objs[1]->list.objs[0]->str, 0);
    Wire *input_2 = create_wire(circuit, params->list.objs[1]->list.objs[2]->str, 0);
    input_wires[0] = input_1;
    input_wires[1] = input_2;

    Wire *output_wire = create_wire(circuit, params->list.objs[2]->list.objs[0]->str, params->list.objs[1]->list.objs[1]->i);
    Wire *control_wire = create_wire(circuit, params->list.objs[3]->list.objs[0]->str, params->list.objs[1]->list.objs[1]->i);;

    if(transistor != NULL)
    {
        printf("\n E' LUIIIIIIIIII");
        transistor->input_wire = input_wires;
        transistor->output_wire = output_wire;
        transistor->control_wire = control_wire;
    }
}

void *Update_Wire_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    Wire *wire = find_wire_by_name(circuit, params->list.objs[0]->str);
    int power = 0;
    int driven = 0;
   
    if(wire != NULL)
    {
        if(params->list.objs[1] != NULL) {power = params->list.objs[1]->i;} else {power = wire->power;}
        if(params->list.objs[2] != NULL) {driven = params->list.objs[2]->i;} else {driven = wire->driven;}
        wire->driven = driven;
        wire->power = power;
    } 

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

    circuit->context->opTable = malloc(sizeof(OperatorTable));    
    circuit->context->opTable->ops = NULL;
    circuit->context->opTable->count = 0;

    circuit->context->stack = malloc(sizeof(CircuitObj));
    circuit->context->stack->list.objs = NULL;
    circuit->context->stack->list.count = 0;

    return circuit;
}

int main(void)
{
    Circuit *circuit = create_circuit();

    append_sym_to_table(circuit, create_symbol("OR", Or_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("AND", And_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("NOT", Not_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("NOR", Nor_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("NAND", Nand_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("XOR", Xor_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("UPDATE_W", Update_Wire_Wrap, SYM_FUNC, 3));
    append_sym_to_table(circuit, create_symbol("UPDATE_T", Update_Transistor_Wrap, SYM_FUNC, 3));

    printf("\n\n\n");
    RowParser **string_list = parse(circuit, circuit->context->stack, " [1 2 3] \"ciaooo\" var in1 XOR xor_a xor_b; UPDATE_W xor_a 1 0; UPDATE_W xor_b 1 0; UPDATE_T \"XOR\" [in1 1 in2 2] [out 0] [ctrl 1];", 1);

    printf("\nSTART\n");
    for(int i = 0; i < circuit->context->stack->list.count; i++)
    {
        print_objects(circuit->context->stack->list.objs[i]);
    }
    printf("FINE\n");
    execute_parsing(circuit);
    run_circuit(circuit);
    return 0;
}