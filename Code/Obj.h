#ifndef OBJ_H
#define OBJ_H

#include "Circuit_Types.h"

// --- Costruzione dei CircuitObj ---
CircuitObj *create_obj();
CircuitObj *create_int_obj(int i);
CircuitObj *create_str_obj(int length);
CircuitObj *create_list_obj();
CircuitObj *create_wire_obj(Circuit *circuit, char *name, int power);
CircuitObj *create_tran_obj(char *name, int wires);
CircuitObj *create_sym_obj(Symbol *sym);

// --- Props (proprieta' tipo ".power", ".name" ecc.) ---
void add_prop(CircuitObj *circuitObj, CircuitObj *prop, char *name);
CircuitObj *search_prop(CircuitObj *obj, char *prop_name);
CircuitObj *get_prop_value(Context *context, CircuitObj *dot_obj, char *prop_name);

// --- Operatori e simboli ---
Operator *create_op(char *name, OpType type);
Symbol *create_symbol(char *name, void *data, SymType type, int param_count);
void append_sym_to_table(Context *context, Symbol *sym);
Symbol *find_var_by_name(Context *context, char *name);

// --- Liste di CircuitObj ---
void append_obj_to_list(CircuitObj *list_obj, CircuitObj *obj, int update_ref_count);
CircuitObj *pop_obj_from_list(CircuitObj *list_obj, int update_ref_count);
CircuitObj *remove_obj_from_list(CircuitObj *list_obj, int index, int update_ref_count);

// --- Valutazione / stampa generica ---
CircuitObj *get_actual_value(Context *context, CircuitObj *obj);
void *print_objects(Context *context, CircuitObj *obj, int evaluate);
char *return_print_objects(Context *context, CircuitObj *obj);

// --- Context ---
Context *create_context();

#endif // OBJ_H