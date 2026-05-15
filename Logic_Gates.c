#include<stdio.h>
#include<stdlib.h>

typedef struct Wire{
    int power;
} Wire;

typedef struct Transistor{
    char *name;
    int state; // if it's closed or open
    int power; // if it's running electricity
    struct Transistor **next_transistors;
    int count;
} Transistor;

typedef struct Circuit{
    int state;
    Transistor **transistors;
    int count;
} Circuit;

Transistor *create_transistor(char *name)
{
    Transistor *transistor = malloc(sizeof(*transistor));

    transistor->name = name;
    transistor->state = 0;
    transistor->power = 0;
    transistor->count = 0;
    transistor->next_transistors = NULL;

    return transistor;
}

void update_transistor(Transistor *transistor, int state, int power)
{
    transistor->state = state;
    transistor->power = power;
}

void append_t_to_c(Circuit *circuit, Transistor *transistor)
{
    int length = circuit->count;

    circuit->transistors = realloc(circuit->transistors, sizeof(Transistor*) * (length + 1)); 
    circuit->transistors[length] = transistor;
    
    circuit->count++;
}

void append_t_to_t(Transistor *from, Transistor *to)
{
    int length = from->count;

    from->next_transistors = realloc(from->next_transistors, sizeof(Transistor*) * (length + 1)); 
    from->next_transistors[length] = to;
    
    from->count++;
}

void update_next_t(Circuit *circuit, Transistor *transistor, int index)
{
    if(index > circuit->count) return;

    int current_state = transistor->state;
    int current_power = transistor->power;

    if(current_state == 1 && current_power == 1 && transistor->next_transistors != NULL)
    {
        for(int i = 0; i < transistor->count; i++)
        {
            transistor->next_transistors[i]->power = 1;
            update_next_t(circuit, transistor->next_transistors[i], index + 1);
        }
    }
}

void print_transistors(Transistor *transistor)
{

    printf("%s is powered %d and gate %d \n", transistor->name, transistor->power, transistor->state);
    if(transistor->count == 0) return;
    for(int i = 0; i < transistor->count; i++)
    {
        print_transistors(transistor->next_transistors[i]);
    }
}

void run_circuit(Circuit *circuit)
{
    circuit->state = 1;
    update_next_t(circuit, circuit->transistors[0], 0);

    print_transistors(circuit->transistors[0]); 

    for(int i = 0; i < circuit->count; i++)
    {
        Transistor *current_transistor = circuit->transistors[i];
    }
}

int main()
{
    Circuit *circuit = malloc(sizeof(*circuit));

    circuit->state = 0;
    circuit->transistors = NULL; // Settando a NUll ovvero a un puntatore void che punta a 0
    circuit->count = 0;

    Transistor *transistor1 = create_transistor("In");
    Transistor *transistor2 = create_transistor("A");
    Transistor *transistor3 = create_transistor("B");
    Transistor *transistor4 = create_transistor("Out");
    //transistor1->next_transistors = transistor2;
    //transistor2->next_transistors = transistor3;
    append_t_to_t(transistor1, transistor2);
    append_t_to_t(transistor1, transistor3);

    append_t_to_t(transistor2, transistor4);
    append_t_to_t(transistor3, transistor4);

    
    update_transistor(transistor1, 1, 1);
    update_transistor(transistor2, 0, 0);
    update_transistor(transistor3, 1, 0);

    append_t_to_c(circuit, transistor1);
    append_t_to_c(circuit, transistor2);
    append_t_to_c(circuit, transistor3);
    append_t_to_c(circuit, transistor4);

    run_circuit(circuit);

    printf("\nHello world! %d\n", transistor1->state);
}