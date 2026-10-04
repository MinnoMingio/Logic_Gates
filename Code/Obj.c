// obj.c
// Sistema generico degli "oggetti" del linguaggio (CircuitObj), tabella dei
// simboli, gestione delle liste e stampa. Usato sia dal parser che dal
// circuito, ma non contiene alcuna logica specifica ne' dell'uno ne'
// dell'altro.

#include "Circuit.h"
#include "Obj.h"
#include "Circuit.h"
#include "Parser.h"

CircuitObj *create_obj()
{
    CircuitObj *obj = malloc(sizeof(CircuitObj));
    obj->type = 0;
    obj->mode = READ_WRITE;

    obj->i = NULL;
    obj->str = NULL;
    obj->sys = NULL;
    obj->list.objs = NULL;
    obj->list.count = 0; 

    obj->props = NULL;
    obj->prop_count = 0;
  
    obj->prop.left = NULL;
    obj->prop.name = NULL;
    
    obj->bracket.index = NULL;
    obj->bracket.left = NULL;
    
    obj->ref_count = 1;
    return obj;
}

CircuitObj *create_int_obj(int i)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_INT;
    obj->i = malloc(sizeof(int*));
    *(obj->i) = i;
    return obj;
}

CircuitObj *create_str_obj(int length)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_STR;
    obj->str = malloc(sizeof(char*));
    *(obj->str) = malloc(length+1);
    (*obj->str)[length-1] = '\0';
    return obj;
}

CircuitObj *create_list_obj()
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_LIST;
    obj->list.objs = NULL;
    obj->list.count = 0;
    return obj;
}

CircuitObj *create_wire_obj(Circuit *circuit, char *name, int power)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_WIRE;
    obj->wire = malloc(sizeof(Wire*));
    *(obj->wire) = create_wire(circuit, name, power);
    return obj;
}

CircuitObj *create_tran_obj(char *name, int wires)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_TRAN;
    obj->transistor = malloc(sizeof(Transistor*));
    *(obj->transistor) = create_transistor(name, wires);
    return obj;
}

CircuitObj *create_gate_obj(char *name)
{
    CircuitObj *obj = create_obj();
    obj->type = OBJ_GATE;
    obj->gate = malloc(sizeof(Gate*));
    *(obj->gate) = NULL;
    return obj;
}

Symbol *create_symbol(char *name, void *data, SymType type, int param_count);

CircuitObj *create_sym_obj(Symbol *sym)
{
    CircuitObj *obj = create_obj();
    //Symbol *sym = create_symbol(name, data, type, param_count);
    switch (sym->type)
    {
    case SYM_FUNC:
        obj->type = OBJ_FUNC;
        break;
    case SYM_VAR:
        obj->type = OBJ_VAR;
        break;
    default:
        break;
    }
    obj->sys= malloc(sizeof(Symbol*));
    *(obj->sys) = sym; 
    return obj;
}
/*
void add_prop(CircuitObj *circuitObj, CircuitObj *prop, char *name)
{
    int length = circuitObj->prop_count;
    circuitObj->props = realloc(circuitObj->props, sizeof(ObjProp*) * (length+1));
    circuitObj->props[length] = malloc(sizeof(ObjProp));
    circuitObj->props[length]->name = strdup(name);
    circuitObj->props[length]->prop = malloc(sizeof(CircuitObj*));

    (*circuitObj->props[length]->prop) = malloc(sizeof(CircuitObj));

    switch(prop->type)
    {
    case OBJ_INT:
        (*circuitObj->props[length]->prop)->type = OBJ_INT;
        (*circuitObj->props[length]->prop)->i = prop->i;
        break;
    case OBJ_STR:
        (*circuitObj->props[length]->prop)->type = OBJ_STR;
        //(*circuitObj->props[length]->prop)->str = wire->name;
        (*circuitObj->props[length]->prop)->str = prop->str;
        break;
    case OBJ_LIST:
        (*circuitObj->props[length]->prop)->type = OBJ_LIST;
        (*circuitObj->props[length]->prop)->list.objs = NULL;
        (*circuitObj->props[length]->prop)->list.count = 0;
        (*circuitObj->props[length]->prop)->list = prop->list;
        break;
    case OBJ_WIRE:
        (*circuitObj->props[length]->prop)->type = OBJ_WIRE;
        (*circuitObj->props[length]->prop)->wire = circuitObj->wire;
        break;
    case OBJ_TRAN:
        (*circuitObj->props[length]->prop)->type = OBJ_TRAN;
        (*circuitObj->props[length]->prop)->transistor = circuitObj->transistor;
        break;

    default:
        break;
    }

    circuitObj->prop_count = length+1;
}
*/

void add_prop(CircuitObj *circuitObj, CircuitObj *prop, char *name)
{
    int length = circuitObj->prop_count;
    circuitObj->props = realloc(circuitObj->props, sizeof(ObjProp*) * (length+1));
    circuitObj->props[length] = malloc(sizeof(ObjProp));
    circuitObj->props[length]->name = strdup(name);
    circuitObj->props[length]->prop = prop;

    circuitObj->prop_count = length+1;
}

/*
void add_int_prop(CircuitObj *circuitObj, int **i, char *name)
{
    int length = circuitObj->prop_count;
    circuitObj->props = realloc(circuitObj->props, sizeof(ObjProp*) * (length+1));
    circuitObj->props[length] = malloc(sizeof(ObjProp));
    circuitObj->props[length]->name = strdup(name);
    circuitObj->props[length]->prop = malloc(sizeof(CircuitObj*));

    (*circuitObj->props[length]->prop) = malloc(sizeof(CircuitObj));

    (*circuitObj->props[length]->prop)->type = OBJ_INT;
    (*circuitObj->props[length]->prop)->i = prop->i;
    
    circuitObj->prop_count = length+1;
}
*/

Operator *create_op(char *name, OpType type)
{
    Operator *op = malloc(sizeof(Operator));
    op->name = strdup(name);
    op->left = NULL;
    op->right = NULL;
    op->type = type;
    return op;
}

Symbol *create_symbol(char *name, void *data, SymType type, int param_count)
{
    Symbol *sys = malloc(sizeof(Symbol));
    sys->name = strdup(name);
    sys->data = data;
    sys->param_count = param_count;
    sys->param_list = NULL;
    sys->obj_returned = NULL;
    sys->type = type;
    return sys;
}

/*
Symbol *create_dc_symbol(char *name, void *data, SymType type, int param_count)
{
    Symbol *sys = malloc(sizeof(Symbol));
    sys->name = strdup(name);
    sys->data = data;
    sys->param_count = param_count;
    sys->param_list = NULL;
    sys->obj_returned = NULL;
    sys->type = type;
    return sys;
}
*/

void append_sym_to_table(Context *context, Symbol *sym)
{
    int len = context->symTable->sym_count + 1;

    Symbol ***function_list = &(context->symTable->sym_list);

    *function_list = realloc(*function_list, sizeof(Symbol*) * (len));

    //(*function_list)[len-1] = malloc(sizeof(Symbol));
    //(*function_list)[len-1]->name = strdup(name);
    //(*function_list)[len-1]->param_count = param_count;
    //(*function_list)[len-1]->data = data;
    //(*function_list)[len-1]->type = type;

    (*function_list)[len-1] = sym;

    context->symTable->sym_count++;
}
CircuitObj *get_prop_value(Context *context, CircuitObj *dot_obj, char *prop_name);
void *print_objects(Context *context, CircuitObj *obj, int evaluate);

char *return_print_objects(Context *context, CircuitObj *obj)
{
    char *result = NULL;

    switch (obj->type)
    {
        case OBJ_INT:
        {
            result = malloc(32); // abbastanza per "int: " + un numero
            snprintf(result, 32, "int: %d", obj->i);
            break;
        }
        case OBJ_STR:
        {
            int len = strlen(*(obj->str)) + 6; // "str: " + stringa + '\0'
            result = malloc(len);
            snprintf(result, len, "str: %s", obj->str);
            break;
        }
        case OBJ_WIRE:
        {
            // Formato: "wire: <name> (power: <val>)"
            const char *name = (*(obj->wire) && (*obj->wire)->name) ? (*obj->wire)->name : "(unnamed)";
            int power = obj->wire ? (*obj->wire)->power : 0;
            int len = strlen(name) + 32;
            result = malloc(len);
            snprintf(result, len, "wire: %s (power: %d)", name, power);
            break;
        }
        case OBJ_TRAN:
        {
            // Formato: "transistor: <name> (out: <val>)"
            const char *name = (obj->transistor && (*obj->transistor)->name) ? (*obj->transistor)->name : "(unnamed)";
            int out_power = (obj->transistor && (*obj->transistor)->output_wire) ? (*obj->transistor)->output_wire->power : -1;
            int len = strlen(name) + 36;
            result = malloc(len);
            snprintf(result, len, "transistor: %s (out: %d)", name, out_power);
            break;
        }
        case OBJ_LIST:
        {
            int cap = 64;
            int len = 0;
            result = malloc(cap);
            if (!result) return NULL;

            // Prefisso iniziale
            len += snprintf(result, cap, "list: [");

            for (int i = 0; i < obj->list.count; i++)
            {
                // Chiamata ricorsiva per l'elemento corrente
                char *item_str = return_print_objects(context, obj->list.objs[i]);
                if (!item_str) continue;

                size_t item_len = strlen(item_str);
                // Separatore ", " tra gli elementi
                size_t needed = len + item_len + (i > 0 ? 2 : 0) + 2; // +2 per "]" e '\0'

                // Ridimensionamento dinamico del buffer se necessario
                if (needed > cap)
                {
                    cap = needed * 2;
                    char *new_res = realloc(result, cap);
                    if (!new_res)
                    {
                        free(item_str);
                        free(result);
                        return NULL;
                    }
                    result = new_res;
                }

                // Aggiunta della virgola e spazio se non è il primo elemento
                if (i > 0)
                {
                    strcat(result, ", ");
                    len += 2;
                }

                strcat(result, item_str);
                len += item_len;
                free(item_str); // Liberiamo la stringa temporanea del singolo elemento
            }

            // Chiusura della parentesi quadra
            if (len + 2 > cap)
            {
                result = realloc(result, len + 2);
            }
            strcat(result, "]");
            break;
        }
        case OBJ_FUNC:
        {
            int len = strlen((*obj->sys)->name) + 7; // "func: " + nome + '\0'
            result = malloc(len);
            snprintf(result, len, "func: %s", (*obj->sys)->name);
            break;
        }
        case OBJ_VAR:
        {
            int len = strlen((*obj->sys)->name) + 7; // "func: " + nome + '\0'
            result = malloc(len);
            snprintf(result, len, "var: %s", (*obj->sys)->name);
            break;
        }
        case OBJ_DOT:
        {
            result = return_print_objects(context, get_prop_value(context, obj->prop.left, obj->prop.name));
            break;
        }
        case OBJ_OP:
        {
            int len = strlen(obj->op->name) + 5;
            result = malloc(len);
            snprintf(result, len, "op: %s", obj->op->name);
            break;
        }
        case 0:
        {
            result = malloc(10);
            snprintf(result, 10, "SEPARATO");
            break;
        }
       default:
        {
            result = malloc(16);
            snprintf(result, 16, "SCONOSCIUTO");
            break;
        }
    }

    return result;
}
CircuitObj *evaluate_index_expr(Context *context, CircuitObj *index_express);
CircuitObj *get_actual_value(Context *context, CircuitObj *obj);
void *print_objects(Context *context, CircuitObj *obj, int evaluate)
{
    switch (obj->type)
    {
        case OBJ_INT:
            printf("int: %d\n", *obj->i);
            break;
        case OBJ_STR:
            printf("str: %s\n", *obj->str);
            break;
        case OBJ_LIST:
            printf("list: %d\n", obj->list.count);
            for(int i = 0; i < obj->list.count; i++)
            {
                printf("   %d - ", i);
                printf("\t");
                print_objects(context, obj->list.objs[i], evaluate);
            }
            break;
        case OBJ_TRAN:
            printf("transistor: %s\n", (*obj->transistor)->name);
            break;
        case OBJ_WIRE:
            printf("wire: %s\n", (*obj->wire)->name);
            break;
        case OBJ_FUNC:
            printf("func: %s\n", (*obj->sys)->name);
            if((*obj->sys)->param_list != NULL)
            {
                printf("parametri: ");
                print_objects(context, (*obj->sys)->param_list, evaluate);
            }
            break;
        case OBJ_VAR:
            //printf("var: %s, Value: %s\n", obj->sys->name, return_print_objects(obj->sys->data));
            printf("var: %s - ", (*obj->sys)->name);
            printf("VALUE: ");
            CircuitObj *var_value = get_actual_value(context, (CircuitObj*)(*obj->sys)->data);
            if(var_value != NULL) print_objects(context, var_value, evaluate);
            else printf("\n");
            break;
        case OBJ_OP:
            printf("op: %s | OP LEFT: ", obj->op->name);
            if(obj->op->left != NULL) print_objects(context, obj->op->left, evaluate);
            else printf("None\n");
            printf("\tOP RIGHT: ");
            if(obj->op->right != NULL) print_objects(context, obj->op->right, evaluate);
            else printf("None");
            printf("\n");
            break;
        case OBJ_DOT:
            printf("attribute: %s\n", obj->prop.name != NULL ? obj->prop.name : "none");
            printf("left prop: ");
            if(obj->prop.left != NULL)
            print_objects(context, obj->prop.left, evaluate);
            else
            printf("\n");
            break;
        case OBJ_INDEX:
            printf("Index: ");
            if(obj->bracket.index != NULL)
            {
                if(obj->bracket.index->i != NULL)
                {
                    if(evaluate)
                    {
                        CircuitObj *eval = evaluate_index_expr(context, obj->bracket.index);
                        CircuitObj *val = get_actual_value(context, eval);
                        if (val && val->type == OBJ_INT)
                        {
                            printf("%d", *(val->i));
                        }
                    }
                    else
                    {
                        printf("NEEDING TO BE EVALUATED");
                    }
                }
            }
            printf("\n");
            printf("left element: ");
            if(obj->bracket.left != NULL)
            print_objects(context, obj->bracket.left, evaluate);
            else
            printf("\n");
            break;

        case 0:
            printf("SEPARATO\n");
            break;
        default:
            printf("SCONOSCIUTO\n");
            break;
    }
}
Symbol *find_var_by_name(Context *context, char *name)
{
    for(int i = 0; i < context->symTable->sym_count; i++)
    {
        if(context->symTable->sym_list[i]->type == SYM_VAR)
        {
            if(strcmp(context->symTable->sym_list[i]->name, name) == 0)
            {
                return context->symTable->sym_list[i];
            }
        }
    }
    return NULL;
}
void append_obj_to_list(CircuitObj *list_obj, CircuitObj *obj, int update_ref_count)
{
    int length = list_obj->list.count;

    list_obj->list.objs = realloc(list_obj->list.objs, sizeof(CircuitObj*) * (length+1));
    list_obj->list.objs[length] = obj;

    if(list_obj->list.objs[length] != NULL && update_ref_count) list_obj->list.objs[length]->ref_count++;
    list_obj->list.count++;
}

CircuitObj *pop_obj_from_list(CircuitObj *list_obj, int update_ref_count)
{
    if(list_obj == NULL || list_obj->list.count <= 0 || list_obj->type != OBJ_LIST) return NULL;

    list_obj->list.count--;
    int length = list_obj->list.count;

    CircuitObj *pop_obj = list_obj->list.objs[length];

    if(length > 0)
    {
        list_obj->list.objs = realloc(list_obj->list.objs, sizeof(CircuitObj *) * (length));
    }
    else
    {
        free(list_obj->list.objs);
        list_obj->list.objs = NULL;
    }

    if(update_ref_count) pop_obj->ref_count--;
    return pop_obj;
}

CircuitObj *remove_obj_from_list(CircuitObj *list_obj, int index, int update_ref_count)
{
    if(list_obj == NULL || list_obj->list.count <= 0 || list_obj->type != OBJ_LIST) return NULL;
    if (index < 0 || index >= list_obj->list.count) return NULL;

    int length = list_obj->list.count;

    CircuitObj *removed_obj = list_obj->list.objs[index];
    for(int i = index; i < length-1; i++)
    {
        list_obj->list.objs[i] = list_obj->list.objs[i+1];
    }

    list_obj->list.count--;
    length = list_obj->list.count; 

    if(length > 0)
    {
        list_obj->list.objs = realloc(list_obj->list.objs, sizeof(CircuitObj *) * (length));
    }
    else
    {
        free(list_obj->list.objs);
        list_obj->list.objs = NULL;
    }

    if(update_ref_count && removed_obj != NULL) removed_obj->ref_count--;
    return removed_obj;
}
CircuitObj *search_prop(CircuitObj *obj, char *prop_name)
{
    for(int i = 0; i < obj->prop_count; i++)
    {
        if(strcmp(obj->props[i]->name, prop_name) == 0)
        {
            return obj->props[i]->prop;
        }
    }
    return NULL;
}

CircuitObj *get_prop_value(Context *context, CircuitObj *dot_obj, char *prop_name)
{
    CircuitObj *real_obj = get_actual_value(context, dot_obj);
    return search_prop(real_obj, prop_name);
}
CircuitObj *get_actual_value(Context *context, CircuitObj *obj)
{
    if(obj == NULL) return NULL;
    // Se è una variabile e ha un valore associato in 'data', restituiamo il suo valore
    if (obj != NULL && obj->type == OBJ_VAR)
    {
        //print_objects(circuit, obj);
        if(obj->sys!= NULL && (*obj->sys)->data != NULL)
        return get_actual_value(context, (CircuitObj *)(*obj->sys)->data);
    }
    if (obj->type == OBJ_DOT)
    {
        CircuitObj *left = obj->prop.left;
        return get_prop_value(context, left, obj->prop.name);
    }
    if (obj->type == OBJ_INDEX)
    {
        CircuitObj *left = get_actual_value(context, obj->bracket.left);
        CircuitObj *index_obj = evaluate_index_expr(context,obj->bracket.index);
        int index = *((get_actual_value(context, index_obj)->i));
        return get_actual_value(context, left->list.objs[index]);
    }
    // Altrimenti è già un valore letterale (es. un INT o una STR) e lo restituiamo così com'è
    return obj;
}
Context *create_context()
{
    Context *context = malloc(sizeof(Context));
    context->symTable = malloc(sizeof(SymbolTable));
    context->symTable->sym_list = NULL;
    context->symTable->sym_count = 0;

    //circuit->context->opTable = malloc(sizeof(OperatorTable));    
    //circuit->context->opTable->ops = NULL;
    //circuit->context->opTable->count = 0;

    context->stack = malloc(sizeof(CircuitObj));
    context->stack->list.objs = NULL;
    context->stack->list.count = 0;

    return context;
}