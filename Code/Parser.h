#ifndef PARSER_H
#define PARSER_H

#include "Circuit_Types.h"

// --- Tokenizer / parser ---
char *create_token(char *start, size_t length);
int can_parse(char c);
CircuitObj *parse_obj(Context *context, char **token, char *start, int is_list);
RowParser **parse(Context *context, CircuitObj *dest, char *txt, int add_separator);

// --- Valutazione delle espressioni parsate ---
CircuitObj *evaluate_index_expr(Context *context, CircuitObj *index_expr);
CircuitObj *execute_func(Context *context, CircuitObj *funcObj, CircuitObj *paramObj, int print);
CircuitObj *evaluate_node(Context *context, CircuitObj *node);
CircuitObj *get_node_list(Context *context, CircuitObj *expression, int *i);
CircuitObj *evaluate_expression(Context *context, CircuitObj *expression);
void execute_parsing(Context *context, RowParser **rows);

#endif // PARSER_H