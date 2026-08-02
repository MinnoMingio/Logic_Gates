// FAI UN PROGRAMMA FIGO CON MMAP (MAGARI FILE .txt e .pdf e .exe)
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>

typedef struct Circuit Circuit;
typedef struct Transistor Transistor;
typedef struct Wire Wire;

typedef void* (*CircuitFunc)(Circuit *circuit, char **param, int count);

typedef struct RowParser
{
    int row;
    char **content;
} RowParser;

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

typedef struct CircuitObj
{
    int type;
    union
    {
        Wire *wire;
        Transistor *transistor;
        struct list
        {
            CircuitObj **objs;
        };
    };
} CircuitObj;

typedef struct GateFunction 
{
    char *name;
    int param_count;
    CircuitFunc func;
} GateFunction;

typedef struct FunctionTable
{
    GateFunction **funcs;
    int funcs_count;
} FunctionTable;


typedef struct Circuit{
    int state;
    FunctionTable *functionTable;
    Transistor **transistors;
    Wire **wires;
    int transistor_count;
    int wire_count;
} Circuit;

void append_func_to_table(Circuit *circuit, char *name, int param_count, CircuitFunc func)
{
    int len = circuit->functionTable->funcs_count + 1;

    GateFunction ***function_list = &(circuit->functionTable->funcs);

    *function_list = realloc(*function_list, sizeof(GateFunction*) * (len));

    (*function_list)[len-1] = malloc(sizeof(GateFunction));
    (*function_list)[len-1]->name = strdup(name);
    (*function_list)[len-1]->param_count = param_count;
    (*function_list)[len-1]->func = func;

    circuit->functionTable->funcs_count++;
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

RowParser **parse(char *txt)
{
    if(txt == NULL || txt[0] == '\0') return NULL;

    char *token = txt;
    char *final_token;
    RowParser **rows = NULL;

    int row_count = 0;
    int element_count = 0;
    while(token[0] != '\0')
    {
        element_count = 0;

        rows = realloc(rows, sizeof(RowParser*) * (row_count + 2));
        rows[row_count] = malloc(sizeof(RowParser));
        rows[row_count]->row = row_count+1;
        rows[row_count]->content = NULL;     

        while(token[0] != ';' && token[0] != '\n' && token[0] != '\0')
        {
            while(isspace(token[0]) && token[0] != '\n' && token[0] != ';') token++;

            if(token[0] == '\0') break;
            if(token[0] == '\n') break;
            if(token[0] == ';') break;

            char *start = token; 

            while(isgraph(token[0]) && token[0] != ';') token++;

            char *end = token;

            int length = end - start;
            final_token = malloc(length + 1);
            //memset(final_token, start, end - start);
            memcpy(final_token, start, length);
            final_token[length] = '\0';

            //final_string = realloc(final_string, sizeof(char*) * (count+2));
            //final_string[count] = final_token;
            rows[row_count]->content = realloc(rows[row_count]->content, sizeof(char*) * (element_count+2));
            rows[row_count]->row = row_count+1;
            rows[row_count]->content[element_count] = final_token;
            rows[row_count]->content[element_count+1] = NULL; // NULL terminator

            printf("%s\n", rows[row_count]->content[element_count]);
            element_count++;
        }
        if(token[0] == ';' || token[0] == '\n') token++;
        if(token[0] != ';' && element_count > 0) row_count++;
        printf("\n");
    }
    //final_string[count] = NULL;
    rows[row_count] = NULL;
    printf("\n\nend");
    return rows;
}

void execute_parsing(Circuit *circuit, RowParser **string_list)
{
    int count = 0;
    int row = 0;

    while(string_list[row] != NULL)
    {
        count = 0;
        char *currentToken = string_list[row]->content[count];
        char **param = NULL;
        int param_size = 0;
        GateFunction *currentFunc = NULL;

        int found = 0;
        while(currentToken != NULL)
        {
            if(!found)
            {
                printf("\n");
                for (int i = 0; i < circuit->functionTable->funcs_count; i++)
                {
                    if (strcmp(currentToken, circuit->functionTable->funcs[i]->name) == 0)
                    {
                        printf("TROVATA! (%s): ", circuit->functionTable->funcs[i]->name);
                        currentFunc = circuit->functionTable->funcs[i];
                        param = malloc(sizeof(char *) * (currentFunc->param_count + 1));
                        found = 1;
                    } 
                }
            }
            else if(found)
            {
                if(currentToken != NULL)
                {
                    param_size++;
                    printf("%s - ", currentToken);
                    param[param_size - 1] = currentToken;
                    param[param_size] = NULL;
                }
            }

            currentToken = string_list[row]->content[count+1];
            count++;
        }
         
        if(currentFunc != NULL) currentFunc->func(circuit, param, param_size);
        row++;
    }
}

void *Or_Wrap(Circuit *circuit, char **param, int count)
{
    Wire *a_wire = create_wire(circuit, param[0], 0);
    Wire *b_wire = create_wire(circuit, param[1], 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;

    Transistor *and_gate = create_OR_Gate(circuit, input_wires);
}

void *And_Wrap(Circuit *circuit, char **param, int count)
{
    Wire *input_wire = create_wire(circuit, param[0], 0);
    Wire *a_wire = create_wire(circuit, param[1], 0);
    Wire *b_wire = create_wire(circuit, param[2], 0);

    Transistor *and_gate = create_AND_Gate(circuit, input_wire, a_wire, b_wire);
}

void *Not_Wrap(Circuit *circuit, char **param, int count)
{
    Wire *input_wire = create_wire(circuit, param[0], 0);

    Transistor *and_gate = create_NOT_Gate(circuit, input_wire);
}

void *Nor_Wrap(Circuit *circuit, char **param, int count)
{
    Wire *a_wire = create_wire(circuit, param[0], 0);
    Wire *b_wire = create_wire(circuit, param[1], 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;

    Transistor *and_gate = create_NOR_Gate(circuit, input_wires);
}

void *Nand_Wrap(Circuit *circuit, char **param, int count)
{
    Wire *input_wire = create_wire(circuit, param[0], 0);
    Wire *a_wire = create_wire(circuit, param[1], 0);
    Wire *b_wire = create_wire(circuit, param[2], 0);

    Transistor *and_gate = create_NAND_Gate(circuit, input_wire, a_wire, b_wire);
}

void *Xor_Wrap(Circuit *circuit, char **param, int count)
{
    Wire *a_wire = create_wire(circuit, param[0], 0);
    Wire *b_wire = create_wire(circuit, param[1], 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;

    Transistor *and_gate = create_XOR_Gate(circuit, input_wires);
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


void *Update_Transistor_Wrap(Circuit *circuit, char **param, int count)
{
    Transistor *transistor = NULL;
    Wire **input_wires;
    Wire *output_wire;
    Wire *control_wire;
    
    for(int i = 0; i < circuit->transistor_count; i++)
    {
        if(strcmp(circuit->transistors[i]->name, param[0]) == 0)
        {
            transistor = circuit->transistors[i];
            if(param[1] != NULL) {input_wires = find_wire_by_name(circuit, param[1]);} else {input_wires = transistor->input_wire;}
            if(param[2] != NULL) {output_wire = find_wire_by_name(circuit, param[2]);} else {output_wire = transistor->output_wire;}
            if(param[3] != NULL) {control_wire = find_wire_by_name(circuit, param[3]);} else {control_wire = transistor->control_wire;}
        }
    }

    if(transistor != NULL)
    {
        transistor->input_wire = input_wires;
        transistor->output_wire = output_wire;
        transistor->control_wire = control_wire;
    }
}

void *Update_Wire_Wrap(Circuit *circuit, char **param, int count)
{
    Wire *wire = NULL;
    int power = 0;
    int driven = 0;
    
    for(int i = 0; i < circuit->wire_count; i++)
    {
        if(strcmp(circuit->wires[i]->name, param[0]) == 0)
        {
            wire = circuit->wires[i];
            if(param[1] != NULL) {power = atoi(param[1]);} else {power = wire->power;}
            if(param[2] != NULL) {driven = atoi(param[2]);} else {driven = wire->driven;}
        }
    }

    if(wire != NULL)
    {
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
    circuit->functionTable = malloc(sizeof(FunctionTable));
    circuit->functionTable->funcs = NULL;
    circuit->functionTable->funcs_count = 0;

    return circuit;
}

int main(void)
{
    Circuit *circuit = create_circuit();

    append_func_to_table(circuit, "OR", 3, Or_Wrap);
    append_func_to_table(circuit, "AND", 3, And_Wrap);
    append_func_to_table(circuit, "NOT", 3, Not_Wrap);
    append_func_to_table(circuit, "NOR", 3, Nor_Wrap);
    append_func_to_table(circuit, "NAND", 3, Nand_Wrap);
    append_func_to_table(circuit, "XOR", 3, Xor_Wrap);
    append_func_to_table(circuit, "UPDATE_W", 3, Update_Wire_Wrap);
    append_func_to_table(circuit, "UPDATE_T", 3, Update_Transistor_Wrap);

    printf("\n\n\n");
    RowParser **string_list = parse("XOR xor_a xor_b; UPDATE xor_a 1 0; AND vcc a b; UPDATE vcc 1 0; UPDATE a 1 0; UPDATE b 1 0;");
    printf("\n%d\n", string_list[0]->row);
    printf("\n%s\n", string_list[0]->content[2]);

    execute_parsing(circuit, string_list);

    run_circuit(circuit);
    return 0;
}