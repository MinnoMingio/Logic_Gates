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

void update_transistor(Transistor *transistor, int state)
{
    transistor->state = state;
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
            int current_power = transistor->input_wire[j]->power;

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
        printf("%s is out p: %d and gate is %s \n", transistor->name, transistor->output_wire->power, transistor->state ? "open" : "closed");
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
    Transistor *transistor2 = create_transistor("A", 1);
    Transistor *transistor3 = create_transistor("B", 1);
    
    update_transistor(transistor2, 0);
    update_transistor(transistor3, 1);

    //transistor1->next_transistors = transistor2;
    //transistor2->next_transistors = transistor3;
    
    //append_t_to_t(transistor1, transistor2);
    //append_t_to_t(transistor1, transistor3);

    //append_t_to_t(transistor2, transistor4);
    //append_t_to_t(transistor3, transistor4);

    // ----------- Questo che segue rappresenta un or gate ----------- //

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
    Transistor *transistor2 = create_transistor("C", 1);
    Transistor *transistor3 = create_transistor("D", 1);
    
    update_transistor(transistor2, 0);
    update_transistor(transistor3, 1);

    //transistor1->next_transistors = transistor2;
    //transistor2->next_transistors = transistor3;
    
    //append_t_to_t(transistor1, transistor2);
    //append_t_to_t(transistor1, transistor3);

    //append_t_to_t(transistor2, transistor4);
    //append_t_to_t(transistor3, transistor4);

    // ----------- Questo che segue rappresenta un or gate ----------- //

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

int main()
{
    Circuit *circuit = malloc(sizeof(*circuit));

    circuit->state = 0;
    circuit->transistors = NULL; // Setto a NUll, ovvero a un puntatore void che punta a 0
    circuit->count = 0;

    Wire *input_wire = create_wire("input", 1);
    Wire *or_output_wire = create_OR_Gate(circuit, input_wire);    
    Wire *and_output_wire = create_AND_Gate(circuit, or_output_wire);

    run_circuit(circuit);
}