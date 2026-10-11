// circuit.c
// Tutto cio' che riguarda la simulazione elettrica vera e propria: Wire,
// Transistor, Circuit, le porte logiche costruite con i transistor, e le
// funzioni "*_Wrap" che il parser invoca (tramite la symbol table) per far
// interagire il linguaggio con il circuito. Nessuna funzione qui dentro
// conosce il parser.

#include "Circuit_Types.h"
#include "Circuit.h"
#include "Obj.h"

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

Gate *create_gate(char *name, int type)
{
    Gate *gate = malloc(sizeof(*gate));

    gate->type = type;
    gate->name = strdup(name);
    gate->tran_a = create_transistor("Tran_A", 1);
    gate->tran_b = create_transistor("Tran_B", 1);
    gate->count = 0;
    gate->transistor_list = NULL;

    gate->output_trans = create_transistor("Tran_OUTPUT", 1);

    return gate;
}

void append_t_to_g(Circuit *circuit, Gate *gate, Transistor *transistor)
{
    int length = gate->count;

    gate->transistor_list = realloc(gate->transistor_list, sizeof(Transistor*) * (length + 1)); 
    gate->transistor_list[length] = transistor;
   
    append_t_to_c(circuit, transistor); 
    
    gate->count++;
}

void append_g_to_g(Gate *gate_src, Gate *gate_to_add)
{
    int length = gate_src->count;

    gate_src->gate_list = realloc(gate_src->gate_list, sizeof(Transistor*) * (length + 1)); 
    gate_src->gate_list[length] = gate_to_add;
    
    gate_src->count++;
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
        //Wire **input_wires = transistor->input_wire;

        if(transistor->control_wire != NULL)
        {
            transistor->state = transistor->control_wire->power;      
        }        

        int current_state = transistor->state + (transistor->invert ? (transistor->state ? -1 : 1) : 0);

        for(int j = 0; j < transistor->count; j++)
        {
            if(transistor->input_wire[j] == NULL) continue;
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
Gate *create_OR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire)
{
    Gate *or_gate = create_gate("OR_Gate", 0);    
    Transistor *transistor1 = create_transistor("OR_1", 1);
    Transistor *transistor2 = create_transistor("OR_1", 1);
    
    update_transistor(transistor1, 1, 0);
    update_transistor(transistor2, 1, 0);

    Wire *wire1_2 = create_wire(circuit, "or_out", 0);

    transistor1->input_wire[0] = a_wire;
    transistor1->output_wire = wire1_2;

    transistor2->input_wire[0] = b_wire;
    transistor2->output_wire = wire1_2;

    append_t_to_g(circuit, or_gate, transistor1);
    append_t_to_g(circuit, or_gate, transistor2);

    or_gate->tran_a = transistor1;
    or_gate->tran_b = transistor2;
    or_gate->output_trans = transistor2;
    return or_gate;
}

Gate *create_AND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b)
{
    Gate *and_gate = create_gate("AND_Gate", 0);
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

    append_t_to_g(circuit, and_gate, transistor1);
    append_t_to_g(circuit, and_gate, transistor2);

    and_gate->tran_a = transistor1;
    and_gate->tran_b = transistor2;
    and_gate->output_trans = transistor2;
    return and_gate;
}

Gate *create_NOT_Gate(Circuit *circuit, Wire *input_wire, Wire *output_wire)
{
    Gate *not_gate = create_gate("NOT_Gate", 0);
    Transistor *transistor1 = create_transistor("NOT", 1);
    
    update_transistor(transistor1, 0, 1);

    if(output_wire == NULL)
    {
        output_wire = create_wire(circuit, "not_out", 0);
    }

    transistor1->input_wire[0] = input_wire;
    transistor1->output_wire = output_wire;

    append_t_to_g(circuit, not_gate, transistor1);

    not_gate->tran_a = transistor1;
    not_gate->output_trans = transistor1;
    return not_gate;
}

Gate *create_NOR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire)
{
    Gate *nor_gate = create_gate("NOR_Gate", 1);
    Gate *or_gate = create_OR_Gate(circuit, a_wire, b_wire);
    Gate *not_or_gate = create_NOT_Gate(circuit, or_gate->output_trans->output_wire, NULL);

    append_g_to_g(nor_gate, or_gate);
    append_g_to_g(nor_gate, not_or_gate);
    return nor_gate;
}

Gate *create_NAND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b)
{
    Gate *nand_gate = create_gate("NAND_Gate", 1);
    Gate *and_gate = create_AND_Gate(circuit, input_wire, a, b);
    Gate *not_and_gate = create_NOT_Gate(circuit, and_gate->output_trans->output_wire, NULL);

    append_g_to_g(nand_gate, and_gate); 
    append_g_to_g(nand_gate, not_and_gate); 
    return nand_gate;
}

Gate *create_XOR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire)
{
    //quindi prende i due segnali (0,0) o (1,1) e in pratica (0, 0)
    //restituisce 1 mentre (1,1) restituisce zero. L'ultimo NOR
    //vedendo un segnale unito di (1,0) restituisce 0, mentre invece
    //se fossero (1,0) sin dall'inizio, allora il nor di (1,0)
    //restituisce 0 e poi Il NOR dei valori invertiti (0,1) restituisce
    //comunque zero e allora all'ultimo NOR arrivano due zeri (0,0) e restituirà 1.
    //Quindi se sono diversi restituisce 1 mentre se sono uguali restituisce zero.

    Gate *xor_gate = create_gate("XOR_Gate", 1);
    Wire *a = a_wire;
    Wire *b = b_wire;
    // Passo 1: Invertiamo i singoli segnali
    Gate *not_a = create_NOT_Gate(circuit, a, NULL);
    Gate *not_b = create_NOT_Gate(circuit, b, NULL);
    append_g_to_g(xor_gate, not_a);
    append_g_to_g(xor_gate, not_b);

    // Passo 2: NOR tra A e B (è vero solo se A=0 e B=0)
    Gate *nor_ab = create_NOR_Gate(circuit, a, b);
    append_g_to_g(xor_gate, nor_ab);
    
    // Passo 3: NOR tra NOT_A e NOT_B (è vero solo se A=1 e B=1)
    //Wire **not_a_b_wires = malloc(sizeof(Wire*) * 2); 
    //not_a_b_wires[0] = not_a_wire->output_wire;
    //not_a_b_wires[1] = not_b_wire->output_wire;
    Gate *nor_not_ab = create_NOR_Gate(circuit, not_a->output_trans->output_wire, not_b->output_trans->output_wire);
    append_g_to_g(xor_gate, nor_not_ab);
   
    //free(not_a_b_wires); 
    
    // Passo 4: Se è VERO che (A=0, B=0) oppure è VERO che (A=1, B=1), 
    // l'XOR deve dare 0. Il NOR finale fa proprio questo controllo!
    //Wire **nor_ab_nor_not_ab = malloc(sizeof(Wire*) * 2);
    //nor_ab_nor_not_ab[0] = nor_ab->output_wire;
    //nor_ab_nor_not_ab[1] = nor_not_ab->output_wire;

    Gate *xor = create_NOR_Gate(circuit, nor_ab->output_trans->output_wire, nor_not_ab->output_trans->output_wire);     
    xor->name = "XOR";
    append_g_to_g(xor_gate, xor);
    //free(nor_ab_nor_not_ab);

    xor_gate->tran_a = not_a->tran_a;
    xor_gate->tran_b = not_b->tran_a;
    xor_gate->output_trans = xor->output_trans;
    return xor_gate;
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
    Gate *sum_gate = create_XOR_Gate(circuit, a, b);

    // 2. Riporto: AND tra A e B (alimentata da VCC, controllata da A e B)
    Gate *carry_gate = create_AND_Gate(circuit, vcc, a, b);

    //free(inputs);
}

void create_Clock_Cicle(Circuit *circuit)
{
    // 1. Creiamo un cavo d'ingresso indipendente con valore 0
    Wire *loop_wire = create_wire(circuit, "loop_wire", 0);
    loop_wire->driven = 1; // Permettiamo al transistor di cambiarlo

    // 2. Creiamo una porta NOT che legge da loop_wire e scrive su loop_wire!
    Gate *not_gate = create_NOT_Gate(circuit, loop_wire, loop_wire);

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

void add_gate_props(CircuitObj *circuitObj)
{
    CircuitObj *name_prop = create_obj();
    name_prop->type = OBJ_STR;
    name_prop->str = &((*circuitObj->gate)->name);
    add_prop(circuitObj, name_prop, "name");

    //CircuitObj *in_wire = create_list_obj();
    CircuitObj *a_wire = create_obj();
    a_wire->type = OBJ_WIRE;
    a_wire->wire = NULL;
    a_wire->wire = &((*circuitObj->gate)->tran_a->input_wire[0]);
    add_wire_props(a_wire);
  
    CircuitObj *b_wire = create_obj();
    b_wire->type = OBJ_WIRE;
    b_wire->wire = NULL;
    b_wire->wire = &((*circuitObj->gate)->tran_a->input_wire[0]);
    add_wire_props(b_wire);
    
    add_prop(circuitObj, a_wire, "a");
    add_prop(circuitObj, b_wire, "b");

    CircuitObj *out_wire = create_obj();
    out_wire->type = OBJ_WIRE;
    out_wire->wire = &((*circuitObj->gate)->output_trans->output_wire);
    add_prop(circuitObj, out_wire, "out");
    add_wire_props(out_wire); 
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
        append_obj_to_list(in_wire, wire_obj, 1);
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

CircuitObj *Or_Wrap(Context *context, CircuitObj *params, int count)
{
    /*
    Wire *a_wire = create_wire(circuit, params->list.objs[0]->str, 0);
    Wire *b_wire = create_wire(circuit, params->list.objs[1]->str, 0);

    Wire **input_wires = create_wire_list(circuit, "or_wires", 0, 2);    
    input_wires[0] = a_wire;
    input_wires[1] = b_wire;
    */
    Circuit *current_circuit = params->list.objs[0]->circuit;
    CircuitObj *or_gate_wrap = create_gate_obj("OR_Gate");
    //CircuitObj *circuitObj = create_tran_obj("OR_TRAN", 1);//malloc(sizeof(CircuitObj));
    //Wire **wire_list = wires_by_params(circuit, params, count);
    Wire *a_wire = *params->list.objs[1]->wire;
    Wire *b_wire = *params->list.objs[2]->wire;

    Gate *or_gate = create_OR_Gate(current_circuit, a_wire, b_wire);
    *or_gate_wrap->gate = or_gate;
    add_gate_props(or_gate_wrap);

    return or_gate_wrap;
}

CircuitObj *And_Wrap(Context *context, CircuitObj *params, int count)
{
    CircuitObj *and_gate_wrap = create_gate_obj("AND_Wrap");
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
    Circuit *current_circuit = params->list.objs[0]->circuit;
    Wire *input_wire = *params->list.objs[1]->wire;
    Wire *a_wire = *params->list.objs[2]->wire;
    Wire *b_wire = *params->list.objs[3]->wire;

    Gate *and_gate = create_AND_Gate(current_circuit, input_wire, a_wire, b_wire);
    *and_gate_wrap->gate = and_gate;
    add_gate_props(and_gate_wrap);

    return and_gate_wrap;
}

CircuitObj *Not_Wrap(Context *context, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_gate_obj("NOT_Gate");
    //Wire **wire_list = wires_by_params(circuit, params, count);
    Circuit *current_circuit = params->list.objs[0]->circuit;
    Wire *input_wire = *params->list.objs[1]->wire;
    Wire *output_wire = *params->list.objs[2]->wire;

    Gate *and_gate = create_NOT_Gate(current_circuit, input_wire, output_wire);
    *circuitObj->gate = and_gate;
    add_gate_props(circuitObj);

    return circuitObj;
}

CircuitObj *Nor_Wrap(Context *context, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_gate_obj("NOR_Wrap");
    
    Circuit *current_circuit = params->list.objs[0]->circuit;
    Wire *a_wire = *params->list.objs[1]->wire;
    Wire *b_wire = *params->list.objs[2]->wire;

    Gate *nor_gate = create_NOR_Gate(current_circuit, a_wire, b_wire);
    *circuitObj->gate = nor_gate;
    add_gate_props(circuitObj);

    return circuitObj;
}

CircuitObj *Nand_Wrap(Context *context, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_gate_obj("NAND_Gate");

    Circuit *current_circuit = params->list.objs[0]->circuit;
    Wire *input_wire = *params->list.objs[1]->wire;
    Wire *a_wire = *params->list.objs[2]->wire;
    Wire *b_wire = *params->list.objs[3]->wire;

    Gate *nand_gate = create_NAND_Gate(current_circuit, input_wire, a_wire, b_wire);
    *circuitObj->gate = nand_gate;
    add_gate_props(circuitObj);

    return circuitObj;
}

CircuitObj *Xor_Wrap(Context *context, CircuitObj *params, int count)
{
    CircuitObj *circuitObj = create_gate_obj("XOR_Gate");
    
    printf("XOR params: %s e %s\n", params->list.objs[0]->str, params->list.objs[1]->str);
    if (params == NULL || params->list.count < 2)
    {
        printf("[XOR_WRAP ERROR] Numero di parametri insufficiente! Ricevuti: %d\n", 
                params ? params->list.count : 0);
        return NULL;
    }
    printf("\n");
    
    Circuit *current_circuit = params->list.objs[1]->circuit;
    Wire *a_wire = *params->list.objs[1]->wire;
    Wire *b_wire = *params->list.objs[2]->wire;

    Gate *xor_gate = create_XOR_Gate(current_circuit, a_wire, b_wire);
    *circuitObj->gate = xor_gate;
    add_gate_props(circuitObj);

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

CircuitObj *Wire_Wrap(Context *context, CircuitObj *params, int count)
{
    Circuit *current_circuit = params->list.objs[0]->circuit;
    char *name = strdup(*params->list.objs[1]->str);
    int power = *params->list.objs[2]->i;
    int driven = *params->list.objs[3]->i;

    CircuitObj *circuitObj = create_wire_obj(current_circuit, name, power);
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

CircuitObj *Print_Wrap(Context *context, CircuitObj *params, int count)
{
    for(int i = 0; i < count; i++)
    {
        CircuitObj *obj = get_actual_value(context, params->list.objs[i]);
        if(obj == NULL) {printf("OBJ NULLO\n"); return NULL;}
        printf("\n");
        if(obj->type == OBJ_WIRE)
        {
            printf(">> WIRE '%s' -> Power: %d\n", (*obj->wire)->name, (*obj->wire)->power);
        }
        else if(obj->type == OBJ_TRAN)
        {
            printf(">> TRAN '%s' -> Out Power: %d\n", (*obj->transistor)->name, (*obj->transistor)->output_wire ? (*obj->transistor)->output_wire->power : -1);
        }
        else if(obj->type == OBJ_GATE)
        {
            printf(">> GATE '%s' -> Out Power: %d\n", (*obj->gate)->name, (*obj->gate)->output_trans->output_wire->power);
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
            print_objects(context, obj, 0);
            //Print_Wrap(circuit, obj, obj->list.count); 
        }
        else if(obj->type == OBJ_FUNC)
        {
            printf(">> ");
            print_objects(context, obj, 0);
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

CircuitObj *Create_Circuit_Wrap(Context *context, CircuitObj *params, int count)
{
    Circuit *circuit = create_circuit();
    CircuitObj *circuit_obj = create_obj();
    circuit_obj->circuit = circuit;
    return circuit_obj;
}

CircuitObj *Run_Wrap(Context *context, CircuitObj *params, int count)
{
    CircuitObj *void_obj = malloc(sizeof(CircuitObj));
    void_obj->type = OBJ_VOID;
    Circuit *current_circuit = params->list.objs[0]->circuit;
    run_circuit(current_circuit);
    return void_obj;
}

CircuitObj* Number_Wrap(Circuit *circuit, CircuitObj *params, int count)
{
    CircuitObj *number = create_int_obj(10);
    return number;
}