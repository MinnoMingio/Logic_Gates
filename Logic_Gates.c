// FAI UN PROGRAMMA FIGO CON MMAP (MAGARI FILE .txt e .pdf e .exe)
#include<stdio.h>
#include<stdlib.h>

typedef struct Transistor Transistor;

typedef struct Wire{
    char *name;
    int power;
} Wire;

typedef struct Transistor{
    char *name;
    int state; // if it's closed or open
    int invert;
    int power; // if it's running electricity
    struct Wire **input_wire;
    struct Wire *output_wire;
    int count;
} Transistor;

typedef struct Circuit{
    int state;
    Transistor **transistors;
    int count;
} Circuit;

Wire *create_wire(char *name, int power)
{
    Wire *wire = malloc(sizeof(*wire));

    wire->name = name;
    wire->power = power;

    return wire;
}

Wire **create_wire_list(char *name, int power, int count)
{
    Wire **wire = malloc(sizeof(*wire) * count);

    for(int i = 0; i < count; i++){
        wire[i] = malloc(sizeof(Wire));
        wire[i]->name = name;
        wire[i]->power = power;
    }

    return wire;
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

    return transistor;
}

void update_transistor(Transistor *transistor, int state, int invert)
{
    transistor->state = state;
    transistor->invert = invert;
}

void append_t_to_c(Circuit *circuit, Transistor *transistor)
{
    int length = circuit->count;

    circuit->transistors = realloc(circuit->transistors, sizeof(Transistor*) * (length + 1)); 
    circuit->transistors[length] = transistor;
    
    circuit->count++;
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
    for(int i = 0; i < circuit->count; i++)
    {
        Transistor *transistor = circuit->transistors[i];
        Wire **input_wires = transistor->input_wire;
        int current_state = transistor->state + (transistor->invert ? (transistor->state ? -1 : 1) : 0);

        for(int j = 0; j < transistor->count; j++)
        {
            int current_power = transistor->input_wire[j]->power + (transistor->invert ? (transistor->input_wire[j]->power ? -1 : 1) : 0);

            if (current_state == 1 && current_power == 1 && transistor->output_wire != NULL)
            {
                transistor->output_wire->power = 1;
            }
        }
    }
}


void print_transistors(Circuit *circuit)
{
    for(int i = 0; i < circuit->count; i++)
    {
        Transistor *transistor = circuit->transistors[i];
        printf("%s wire out p: %d and gate state is %s \n", transistor->name, transistor->output_wire->power, transistor->state ? "open" : "closed");
    }
}

void run_circuit(Circuit *circuit)
{
    circuit->state = 1;

    update_next_t(circuit);
    print_transistors(circuit);
}

Transistor *create_OR_Gate(Circuit *circuit, Wire **input_wire)
{
    Transistor *transistor2 = create_transistor("OR_1", 1);
    Transistor *transistor3 = create_transistor("OR_2", 1);
    
    update_transistor(transistor2, 1, 0);
    update_transistor(transistor3, 1, 0);

    Wire *wire2_3 = create_wire("2_3", 0);

    transistor2->input_wire[0] = input_wire[0];
    transistor2->output_wire = wire2_3;

    transistor3->input_wire[0] = input_wire[1];
    transistor3->output_wire = wire2_3;

    append_t_to_c(circuit, transistor2);
    append_t_to_c(circuit, transistor3);

    return transistor3;
}

Transistor *create_AND_Gate(Circuit *circuit, Wire *input_wire)
{
    Transistor *transistor1 = create_transistor("AND_1", 1);
    Transistor *transistor2 = create_transistor("AND_2", 1);
    
    update_transistor(transistor1, 1, 0);
    update_transistor(transistor2, 1, 0);

    Wire *mid = create_wire("mid", 0); 
    Wire *out = create_wire("out", 0);

    transistor1->input_wire[0] = input_wire;
    transistor1->output_wire = mid;

    transistor2->input_wire[0] = transistor1->output_wire;
    transistor2->output_wire = out;

    append_t_to_c(circuit, transistor1);
    append_t_to_c(circuit, transistor2);

    return transistor2;
}

Transistor *create_NOT_Gate(Circuit *circuit, Wire *input_wire)
{
    Transistor *transistor1 = create_transistor("NOT", 1);
    
    update_transistor(transistor1, 0, 1);

    Wire *wire1 = create_wire("1", 0);

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

int main()
{
    Circuit *circuit = malloc(sizeof(*circuit));

    circuit->state = 0;
    circuit->transistors = NULL; // Setto a NUll, ovvero a un puntatore void che punta a 0
    circuit->count = 0;

    //Wire *wire1 = create_wire("and_wires_1", 1);
    //Transistor *and_gate = create_AND_Gate(circuit, wire1);
    //circuit->transistors[0]->state = 0;
    //circuit->transistors[1]->state = 1;
    //printf("%d\n", and_gate->state);
    Wire **input_wire = create_wire_list("input", 1, 2);
    input_wire[0]->power = 0;
    ////Wire *or1_output_wire = create_OR_Gate(circuit, input_wire);
    ////Wire *and_output_wire = create_AND_Gate(circuit, or1_output_wire);
    ////Wire *or2_output_wire = create_OR_Gate(circuit, and_output_wire);
    ////Wire *not1_output_wire = create_NOT_Gate(circuit, or2_output_wire);
    ////Wire *or3_output_wire = create_OR_Gate(circuit, not1_output_wire);
    ////Wire *nor_output_wire = create_NOR_Gate(circuit, or3_output_wire);
    Transistor *xor = create_XOR_Gate(circuit, input_wire);

    //circuit->transistors[2]->state = 0; // Stato di chiusura dell'AND Gate
    
    run_circuit(circuit);
}