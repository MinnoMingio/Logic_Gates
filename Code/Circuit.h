#ifndef CIRCUIT_H
#define CIRCUIT_H

#include "Circuit_Types.h"

// --- Wire / Transistor / Circuit di base ---
void append_wire_to_circuit(Circuit *circuit, Wire *wire);
Wire *initialize_wire(char *name, int power);
Wire *create_wire(Circuit *circuit, char *name, int power);
Wire **create_wire_list(Circuit *circuit, char *name, int power, int count);
Transistor *create_transistor(char *name, int wires);
void update_transistor(Transistor *transistor, int state, int invert);
void append_t_to_c(Circuit *circuit, Transistor *transistor);
Circuit *create_circuit();

// --- Simulazione ---
void update_next_t(Circuit *circuit);
int commit(Circuit *circuit);
void print_transistors(Circuit *circuit);
int run_parallel_circuit(Circuit *circuit, int max_count);
void run_circuit(Circuit *circuit);

/*
// --- Porte logiche costruite con i transistor ---
Transistor *create_OR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire);
Transistor *create_AND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b);
Transistor *create_NOT_Gate(Circuit *circuit, Wire *input_wire, Wire *output_wire);
Transistor *create_NOR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire);
Transistor *create_NAND_Gate(Circuit *circuit, Wire *input_wire, Wire *a, Wire *b);
Transistor *create_XOR_Gate(Circuit *circuit, Wire *a_wire, Wire *b_wire);
void create_Half_Adder(Circuit *circuit, int input_a, int input_b);
void create_Clock_Cicle(Circuit *circuit);
*/

// --- Ricerche ---
Symbol *find_function_by_name(Circuit *circuit, char *name);
Transistor *find_transistor_by_name(Circuit *circuit, char *name);
Wire *find_wire_by_name(Circuit *circuit, char *name);
Wire **wires_by_params(Circuit *circuit, CircuitObj *params, int count);

// --- Props specifiche di wire/transistor ---
void add_wire_props(CircuitObj *circuitObj);
void *add_tran_props(CircuitObj *circuitObj);

// --- Funzioni "Wrap": ponte tra la symbol table del parser e il circuito ---
CircuitObj *Or_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *And_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *Not_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *Nor_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *Nand_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *Xor_Wrap(Context *context, CircuitObj *params, int count);
void *Update_Wire_Wrap(Circuit *circuit, CircuitObj *params, int count);
CircuitObj *Wire_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *Print_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *Create_Circuit_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *Run_Wrap(Context *context, CircuitObj *params, int count);
CircuitObj *Number_Wrap(Circuit *circuit, CircuitObj *params, int count);

#endif // CIRCUIT_H