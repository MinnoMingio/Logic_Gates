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
        int current_state = transistor->state;

        for(int j = 0; j < transistor->count; j++)
        {
            int current_power = transistor->input_wire[j]->power + (transistor->invert ? (transistor->input_wire[j]->power ? -1 : 1) : 0);

            if (current_state + (transistor->invert ? (transistor->state ? -1 : 1) : 0) == 1 && current_power == 1 && transistor->output_wire != NULL)
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

Wire *create_OR_Gate(Circuit *circuit, Wire *input_wire)
{
    Transistor *transistor2 = create_transistor("OR_1", 1);
    Transistor *transistor3 = create_transistor("OR_2", 1);
    
    update_transistor(transistor2, 0, 0);
    update_transistor(transistor3, 1, 0);

    Wire *wire2_3 = create_wire("2_3", 0);

    transistor2->input_wire[0] = input_wire;
    transistor2->output_wire = wire2_3;

    transistor3->input_wire[0] = input_wire;
    transistor3->output_wire = wire2_3;

    append_t_to_c(circuit, transistor2);
    append_t_to_c(circuit, transistor3);

    return wire2_3;
}

Wire *create_AND_Gate(Circuit *circuit, Wire *input_wire)
{
    Transistor *transistor2 = create_transistor("AND_1", 1);
    Transistor *transistor3 = create_transistor("AND_2", 1);
    
    update_transistor(transistor2, 0, 0);
    update_transistor(transistor3, 1, 0);

    Wire *wire2 = create_wire("2", 0);
    Wire *wire3 = create_wire("3", 0);

    transistor2->input_wire[0] = input_wire;
    transistor2->output_wire = wire2;

    transistor3->input_wire[0] = transistor2->output_wire;
    transistor3->output_wire = wire3;

    append_t_to_c(circuit, transistor2);
    append_t_to_c(circuit, transistor3);

    return wire3;
}

Wire *create_NOT_Gate(Circuit *circuit, Wire *input_wire)
{
    Transistor *transistor1 = create_transistor("NOT", 1);
    
    update_transistor(transistor1, 0, 1);

    Wire *wire1 = create_wire("1", 0);

    transistor1->input_wire[0] = input_wire;
    transistor1->output_wire = wire1;

    append_t_to_c(circuit, transistor1);

    return wire1;
}

Wire *create_NOR_Gate(Circuit *circuit, Wire *input_wire)
{
    Wire *or_gate = create_OR_Gate(circuit, input_wire);
    return create_NOT_Gate(circuit, or_gate);
}

int main()
{
    Circuit *circuit = malloc(sizeof(*circuit));

    circuit->state = 0;
    circuit->transistors = NULL; // Setto a NUll, ovvero a un puntatore void che punta a 0
    circuit->count = 0;

    Wire *input_wire = create_wire("input", 1);
    Wire *or1_output_wire = create_OR_Gate(circuit, input_wire);    
    Wire *and_output_wire = create_AND_Gate(circuit, or1_output_wire);
    Wire *or2_output_wire = create_OR_Gate(circuit, and_output_wire);
    Wire *not1_output_wire = create_NOT_Gate(circuit, or2_output_wire);
    Wire *or3_output_wire = create_OR_Gate(circuit, not1_output_wire);
    Wire *nor_output_wire = create_NOR_Gate(circuit, or3_output_wire);
    
    circuit->transistors[2]->state = 1; // Stato di chiusura dell'AND Gate
    
    run_circuit(circuit);
}