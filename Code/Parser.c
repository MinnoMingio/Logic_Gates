// parser.c
// Tokenizer, parser (testo -> alberi di CircuitObj) e valutatore delle
// espressioni. Non conosce Wire/Transistor/Circuit: le funzioni del
// "circuito" gli arrivano solo come puntatori a funzione dentro ai Symbol
// (CircuitFunc), tramite la symbol table del Context.

#include "Circuit_Types.h"
#include "Parser.h"
#include "Obj.h" // create_obj e affini, symbol table, liste

char *create_token(char *start, size_t length)
{
    char *final_token = NULL;

    final_token = malloc(length + 1);
    // memset(final_token, start, end - start);
    memcpy(final_token, start, length);
    final_token[length] = '\0';

    return final_token;
}
int can_parse(char c)
{
    return (isgraph(c) && c != ';' && c != '\n' && c != '.' && c != '[' && c != '=' && c != '+' && c != '-' && c != '*' && c != '/');
}

RowParser **parse(Context *context, CircuitObj *dest, char *txt, int add_separator);
CircuitObj *get_actual_value(Context *context, CircuitObj *obj);
CircuitObj *parse_obj(Context *context, char **token, char *start, int is_list)
{
    CircuitObj *circuitObj = create_obj(); //= create_obj();// malloc(sizeof(CircuitObj));

    while(isspace(start[0]) && start[0] != '\n' && start[0] != ';') start++;

    char *end = *token;
    int length = end - start;
    char *final_token = malloc(length + 1);
    if(final_token == NULL) return NULL;
    if(length>0)
    {
        memcpy(final_token, start, length);
        final_token[length] = '\0';
    }
    else if(length + 1 <= 0)
    {
        return NULL;
    }
    else
    {
        if(!isgraph(start[0])) return NULL;
    }
     
    Symbol *sym = NULL;
    for (int i = 0; i < context->symTable->sym_count; i++)
    {
        if(context->symTable->sym_list[i]->type == SYM_FUNC)
        {
            if (strcmp(final_token, context->symTable->sym_list[i]->name) == 0)
            {
                sym = context->symTable->sym_list[i];
                Symbol *sym_deep_copy = create_symbol(sym->name, sym->data, sym->type, sym->param_count);
                circuitObj = create_sym_obj(sym_deep_copy);
                return circuitObj;
            }
        }else if(context->symTable->sym_list[i]->type == SYM_VAR)
        {
            if (strcmp(final_token, context->symTable->sym_list[i]->name) == 0)
            {
                sym = context->symTable->sym_list[i];
                circuitObj = create_sym_obj(sym);
                return circuitObj;
            }
        }
    }

    if(start[0] == '=')
    {
        Operator *op = create_op("=", OP_ASSIGN);
        circuitObj = create_obj();
        circuitObj->type = OBJ_OP;
        op->prec = 1;
        circuitObj->op = op;
    }
    else if (start[0] == '+')
    {
        Operator *op = create_op("+", OP_ADD);
        circuitObj = create_obj();
        circuitObj->type = OBJ_OP;
        op->prec = 2;
        circuitObj->op = op;
    }
    else if (start[0] == '-')
    {
        Operator *op = create_op("-", OP_SUB);
        circuitObj = create_obj();
        circuitObj->type = OBJ_OP;
        op->prec = 2;
        circuitObj->op = op;
    }
    else if (start[0] == '*')
    {
        Operator *op = create_op("*", OP_MOL);
        circuitObj = create_obj();
        circuitObj->type = OBJ_OP;
        op->prec = 3;
        circuitObj->op = op;
    }
    else if (start[0] == '/')
    {
        Operator *op = create_op("/", OP_DIV);
        circuitObj = create_obj();
        circuitObj->type = OBJ_OP;
        op->prec = 3;
        circuitObj->op = op;
    }
    else if(strcmp(final_token, "if") == 0)
    {
        printf("IF TROVATO!\n");
        circuitObj->type = OBJ_IF;
        circuitObj->condition.line = 0;
        circuitObj->condition.statement = NULL;
    }
    else if (strcmp(final_token, "var") == 0)
    {
        while (isspace((*token)[0]) && (*token)[0] != '\n' && (*token)[0] != '\0' && (*token)[0] != ';')
            (*token)++;
        char *start = *token;
        while (can_parse((*token)[0]))
            (*token)++;
        char *end = *token;

        int length = end - start;

        char *var_name = malloc(length + 1);
        memcpy(var_name, start, length);
        var_name[length] = '\0';

        sym = find_var_by_name(context, var_name);

        if(sym == NULL)
        {
            sym = create_symbol(var_name, NULL, SYM_VAR, 1);
            append_sym_to_table(context, sym);

            circuitObj = create_sym_obj(sym); 
        }else
        {
            printf("VAR GIA' ESISTENTE\n");
            circuitObj = create_sym_obj(sym); 
        }
    }
    else if (isdigit(start[0]))
    {
        circuitObj = create_int_obj(atoi(final_token)); 
    }else if(start[0] == '"')
    {
        (*token)++;
        start = *token;
        while((*token)[0] != '"')
        {
            (*token)++;
        }
        end = *token;
        (*token)++;

        length = end - start;
        int new_length = length;
        circuitObj = create_str_obj(new_length+1);
        memcpy(*circuitObj->str, start, new_length);
        
    }
    else if(start[0] == '.')
    {
        circuitObj->type = OBJ_DOT;
        (*token)++;
        start = (*token);

        while(can_parse((*token)[0]))
        {
            (*token)++;
        }
        end = (*token); 

        length = end - start;
        circuitObj->prop.name = malloc(length+1);
        memcpy(circuitObj->prop.name, start, length);
        circuitObj->prop.name[length] = '\0';
    }
    else if (start[0] == '[' && is_list)
    {
        (*token)++;
        char *start = *token;
        int bracket_balance = 1;
        while ((*token)[0] != '\0' && bracket_balance > 0)
        {
            if ((*token)[0] == '[')
            {
                bracket_balance++;
            }
            if ((*token)[0] == ']')
            {
                bracket_balance--;
            }
            if (bracket_balance > 0)
                (*token)++;
        }
        char *end = *token;

        circuitObj->type = OBJ_LIST;
        circuitObj->list.objs = NULL;
        circuitObj->list.count = 0;

        int new_length = end - start; // Esclusa l'ultima quadra
        char *list_txt = malloc(new_length + 1);
        memcpy(list_txt, start, new_length);
        list_txt[new_length] = '\0';
        parse(context, circuitObj, list_txt, 0); // metto ciruitObj come secondo parametro in quanto e una cazzo di lista e quindi niente
        free(list_txt);

        while((*token)[0] == ']' || (*token)[0] == ' ')
            (*token)++;
    }
    else if(start[0] == '[' && !is_list)
    {
        (*token)++;
        start = *token;
        int bracket_balance = 1;
        while ((*token)[0] != '\0' && bracket_balance > 0)
        {
            if ((*token)[0] == '[')
            {
                bracket_balance++;
            }
            if ((*token)[0] == ']')
            {
                bracket_balance--;
            }
            if (bracket_balance > 0)
                (*token)++;
        }
        char *end = *token;

        circuitObj->type = OBJ_INDEX;

        int new_length = end - start; // Esclusa l'ultima quadra
        char *list_txt = malloc(new_length + 1);
        memcpy(list_txt, start, new_length);
        list_txt[new_length] = '\0';

        CircuitObj *index_obj = create_list_obj();
        parse(context, index_obj, list_txt, 0);
        free(list_txt);

        circuitObj->bracket.index = index_obj;

        while((*token)[0] == ']' || (*token)[0] == ' ')
            (*token)++;
    }
    else if (circuitObj->type == 0)
    {
        circuitObj->type = OBJ_STR;
        circuitObj = create_str_obj(length+1);
        *circuitObj->str = final_token;
    }

    return circuitObj;
}

RowParser **parse(Context *context, CircuitObj *dest, char *txt, int add_separator)
{
    if(txt == NULL || txt[0] == '\0') return NULL;

    char *token = txt;
    RowParser **rows = NULL;
    context->rows = rows;

    int row_count = 0;
    int element_count = 0;

    element_count = 0;

    rows = realloc(rows, sizeof(RowParser *) * (row_count + 2));
    rows[row_count] = malloc(sizeof(RowParser));
    rows[row_count]->row = row_count + 1;
    rows[row_count]->content = create_list_obj();

    while(token[0] != '\0')
    {
        while(token[0] != ';' && token[0] != '\n' && token[0] != '\0')
        {
            CircuitObj *circuitObj = NULL;

            while(isspace(token[0]) && token[0] != '\n' && token[0] != ';') token++;

            if(token[0] == '\0') break;
            if(token[0] == '\n') break;
            if(token[0] == ';') break;

            char *previous = NULL;
            if (token > txt) previous = token - 1;
            char *start = token;
            char *end = token;

            CircuitObj *previous_obj = NULL;
            if(dest->list.count > 0)
            {
                previous_obj = dest->list.objs[dest->list.count - 1];
            }

            if(previous_obj != NULL && previous_obj->type == OBJ_IF)
            {
                
            }

            if (token[0] == '\"')
            {
                circuitObj = parse_obj(context, &token, token, 0);
                start = token;
            }
            else if (token[0] == '=' || token[0] == '+' || token[0] == '-' || token[0] == '*' || token[0] == '/')
            {
                circuitObj = parse_obj(context, &token, token, 0);
                token++;
                start = token;
            }
            else if (token[0] == '.' && previous_obj != NULL)
            {
                circuitObj = parse_obj(context, &token, token, 0);
                circuitObj->prop.left = previous_obj;
                start = token;
            }
            else if(start[0] == '[' && ((previous != NULL && !isgraph(previous[0])) || previous == NULL))
            {
                circuitObj = parse_obj(context, &token, token, 1);
                start = token;
            }
            else if(start[0] == '[' && previous_obj != NULL && ((previous != NULL && isgraph(previous[0]))))
            {
                circuitObj = parse_obj(context, &token, token, 0);
                circuitObj->bracket.left = previous_obj;
                start = token;
            }
            else
            {
                while (can_parse(token[0]))
                {
                    token++;
                }
                if (token > txt)
                    previous = token - 1;

                end = token;
                int length = end - start;
                if (length > 0)
                {
                    circuitObj = parse_obj(context, &token, start, 0);
                }
            }

            if(circuitObj != NULL)
            {
                append_obj_to_list(dest, circuitObj, 1);
                append_obj_to_list(rows[row_count]->content, circuitObj, 1);
                element_count++;
            }else
            {
                token++;
                continue;
            }
        }

        if(add_separator && element_count > 0)
        {
            //printf("RIGA NUMERO: %d E HA %d\n", row_count, (int)(token[0]));
            CircuitObj *circuitObj = create_obj();
            append_obj_to_list(context->stack, circuitObj, 1);
            append_obj_to_list(rows[row_count]->content, circuitObj, 1);
            element_count = 0;
        }

        while(token[0] == ';' || token[0] == '\r') token++;
        if(token[0] == '\n')
        {
            token++;
            row_count++;

            element_count = 0;

            rows = realloc(rows, sizeof(RowParser *) * (row_count + 2));
            rows[row_count] = malloc(sizeof(RowParser));
            rows[row_count]->row = row_count + 1;
            rows[row_count]->content = create_list_obj();
        }
    }
    row_count++;
    rows[row_count] = NULL;
    return rows;
}
//Setto i prototypes
CircuitObj *evaluate_single_expression(Context *context, CircuitObj **stack, int *i, int *stack_count);
CircuitObj *evaluate_expression(Context *context, CircuitObj *expression);
CircuitObj *evaluate_node(Context *context, CircuitObj *node);
CircuitObj *get_node_list(Context *context, CircuitObj *expression, int *i);

CircuitObj *evaluate_index_expr(Context *context, CircuitObj *index_expr)
{
    int i = 0;
    CircuitObj **expression = get_node_list(context, index_expr, &i)->list.objs;
    CircuitObj *index_obj = NULL;
    //for(int i = 0; i < index_expr->list.count; i++)
    //{
        //index_obj = evaluate_single_expression(circuit, expression, &i, &index_expr->list.count);
    //}
    return get_actual_value(context, evaluate_node(context, expression[0]));
}
CircuitObj *execute_func(Context *context, CircuitObj *funcObj, CircuitObj *paramObj, int print)
{
    //if(print) printf("FUNC TROVATA! (%s): \n", (*funcObj->sys)->name);

    Symbol *currentFunc = NULL;
    currentFunc = *funcObj->sys;
    int param_size = paramObj->list.count;
    
    if (currentFunc != NULL)
    {
        CircuitObj *returned_obj = ((CircuitFunc)currentFunc->data)(context, paramObj, param_size);
        //free(paramObj); // Devi liberare anche i figli della list obj
        //if(print) printf("------------------------------------------\n");
        return returned_obj;
    }else
    {
        //printf("\n");
        return NULL;
    }
}

CircuitObj *evaluate_node(Context *context, CircuitObj *node)
{
    if(node->type == OBJ_OP)
    {
        CircuitObj *left = evaluate_node(context, node->op->left);
        CircuitObj *right = evaluate_node(context, node->op->right);
        int l_value = 0;
        int r_value = 0;
        switch (node->op->type)
        {
        case OP_ASSIGN:
            if(left->type == OBJ_VAR)
            {
                (*left->sys)->data = right;
            }
            else
            {
                CircuitObj *prop = get_actual_value(context, left);

                switch (prop->type)
                {
                case OBJ_INT:
                    *prop->i = *get_actual_value(context, right)->i; 
                    break;
                case OBJ_STR:
                    *prop->str = *get_actual_value(context, right)->str;
                
                default:
                    break;
                }
            }
            return left;
            break;
        case OP_ADD:
            
            CircuitObj *actual_left = get_actual_value(context, left); 
            CircuitObj *actual_right = get_actual_value(context, right); 
            
            if(actual_left->type == OBJ_INT && actual_right->type == OBJ_INT)
            {
                l_value = *(actual_left->i);
                r_value = *(actual_right->i);
                return create_int_obj(l_value + r_value);
            }
            break;    
        case OP_SUB:
            l_value = *(get_actual_value(context, left)->i);
            r_value = *(get_actual_value(context, right)->i);
            return create_int_obj(l_value-r_value);
            break;
        case OP_MOL:
            l_value = *(get_actual_value(context, left)->i);
            r_value = *(get_actual_value(context, right)->i);
            return create_int_obj(l_value*r_value);
            break;
        case OP_DIV:
            l_value = *(get_actual_value(context, left)->i);
            r_value = *(get_actual_value(context, right)->i);
            return create_int_obj(l_value/r_value);
            break;
        default:
            break;
        }
    }
    else if(node->type == OBJ_FUNC)
    {
        Symbol *funcObj = (*node->sys);
        CircuitObj *param_list = funcObj->param_list;
        CircuitObj *evaluated_param_list = create_list_obj();
        if(param_list != NULL)
        {
            for (int i = 0; i < param_list->list.count; i++)
            {
                append_obj_to_list(evaluated_param_list, get_actual_value(context, evaluate_node(context, param_list->list.objs[i])), 1);
            }
        }
        
        return execute_func(context, node, evaluated_param_list, 1);
    }
    else
    {
        return node;
    }
}

CircuitObj *get_node_list(Context *context, CircuitObj *expression, int *i)
{
    CircuitObj **expression_stack = expression->list.objs;
    int expression_count = expression->list.count;

    //Prima AST
    CircuitObj *op_list = create_list_obj();
    CircuitObj *node_list = create_list_obj();
    int last_i = 0;
    int break_next = 0;
    for (*i; *i < expression_count; (*i)++)
    {
        if(expression_stack[*i] == NULL) continue;

        // Ha precedenza su tutto
        if(break_next)
        {
            last_i = *i;
            break;
        }

        // Se non ci dobbiamo fermare allora skippiamo i left obj dei prop o delle liste
        while (*i + 1 < expression_count && (expression_stack[*i + 1]->type == OBJ_DOT || expression_stack[*i + 1]->type == OBJ_INDEX))
        {
            (*i)++;
        }
        if(*i+1 < expression_count && expression_stack[(*i)]->type != OBJ_OP && expression_stack[(*i) + 1]->type != OBJ_OP)
        {
            //printf("LAST I: ");
            //print_objects(circuit, expression_stack[*i], 0);
            //printf("\n");
            break_next = 1;
        }

        if (expression_stack[*i]->type == OBJ_OP)
        {
            if (expression_stack[*i] != NULL && expression_stack[*i]->type != 0)
            {
                int op_count = op_list->list.count;
                while (op_list->list.count > 0 && expression_stack[*i]->op->prec <= op_list->list.objs[op_count - 1]->op->prec)
                {
                    // Rimuovi dalla lista (pop)
                    CircuitObj *last_op_obj = pop_obj_from_list(op_list, 0);
                    Operator *last_op = last_op_obj->op;
                    // Non modificare direttamente l'ultimo op, ma usa un altro op cosi' da lasciare il codice originale intatto
                     
                    CircuitObj *right_node_obj = pop_obj_from_list(node_list, 0);
                    CircuitObj *left_node_obj = pop_obj_from_list(node_list, 0);
                    
                    CircuitObj *ast_obj = create_obj(); 
                    Operator *ast_op = create_op(last_op->name, last_op->type);
                    ast_op->right = right_node_obj;
                    ast_op->left = left_node_obj;

                    ast_obj->type = OBJ_OP;
                    ast_obj->op = ast_op;
                    
                    append_obj_to_list(node_list, ast_obj, 1);
                     
                    op_count = op_list->list.count;
                }
                if(expression_stack[*i]->type == OBJ_OP) append_obj_to_list(op_list, expression_stack[*i], 1);
            }
        }
        else if(expression_stack[*i]->type != 0)
        {
            append_obj_to_list(node_list, expression_stack[*i], 1);
        }
    }

    while(op_list->list.count > 0)
    {
        CircuitObj *last_op_obj = pop_obj_from_list(op_list, 0);
        Operator *last_op = last_op_obj->op;

        CircuitObj *right_node_obj = pop_obj_from_list(node_list, 0);
        CircuitObj *left_node_obj = pop_obj_from_list(node_list, 0);
        
        CircuitObj *ast_obj = create_obj();
        Operator *ast_op = create_op(last_op->name, last_op->type);
        ast_op->right = right_node_obj;
        ast_op->left = left_node_obj;
        
        ast_obj->type = OBJ_OP;
        ast_obj->op = ast_op;
        
        append_obj_to_list(node_list, ast_obj, 1);
    }

    last_i--;
    if(last_i >= 0)
    if (expression_stack[last_i]->type == OBJ_FUNC)
    {
        CircuitObj *funcObj = expression_stack[last_i];
        //printf("\nFUNC: ");
        //print_objects(circuit, funcObj, 0);
        CircuitObj *arg_list = create_list_obj();
        int arg_count = (*funcObj->sys)->param_count;
        if (arg_count > 0)
        {
            //(*i)++;
            for (int j = 0; j < arg_count; j++)
            {
                CircuitObj *arg_result = get_node_list(context, expression, i);
                //CircuitObj *arg_node = arg_result->list.objs[0]; // estrai l'unico nodo, non la lista contenitore
                append_obj_to_list(arg_list, arg_result->list.objs[0], 1);
            }
            // CircuitObj *arg_node_list = get_node_list(circuit, expression, i);
            //printf("PARAMETRI DI %s: ", (*funcObj->sys)->name);
            //print_objects(circuit, arg_list, 0);

            (*funcObj->sys)->param_list = arg_list;
        }

        //append_obj_to_list(node_list, funcObj, 1);
    }

    return node_list;
}

CircuitObj* evaluate_expression(Context *context, CircuitObj *expression)
{
    //printf("EXPRESSION: ");
    //print_objects(circuit, expression, 0);
    //printf("\n");
    int i = 0;
    CircuitObj *node_list = get_node_list(context, expression, &i);
    CircuitObj **expression_stack = node_list->list.objs;
    int expression_count = node_list->list.count;

    /* 
    printf("\nNODE_LIST: ");
    print_objects(circuit, node_list, 0);
    printf("---------------------------------------------------");
    printf("\n");
    */

    for (int i = 0; i < expression_count; i++)
    {
        if(expression_stack[i]->type == 0) continue;
         
        CircuitObj *evaluated_node = evaluate_node(context, expression_stack[i]);
        //printf("%d)\n", i);
        //if(evaluated_node != NULL && evaluated_node->type != 0) print_objects(circuit, evaluated_node, 0);
        //printf("\n");
    }
}

void execute_parsing(Context *context, RowParser **rows)
{
    int i = 0;
    while(rows[i] != NULL)
    {
        CircuitObj *row = rows[i]->content;
        evaluate_expression(context, row);
        i++;
    }
}