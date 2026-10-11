//gcc -g main.c Circuit.c Obj.c Parser.c -o main.exe
// main.c
// Punto di ingresso: registra le funzioni del circuito nella symbol table,
// legge circuit.txt, lo parsa e lo esegue.

#include "Circuit_Types.h" 
#include "Obj.h"
#include "Circuit.h"
#include "Parser.h"

int main(void)
{
    system("cls");
    //Circuit *circuit = create_circuit();
    Context *context = create_context();

    append_sym_to_table(context, create_symbol("CIRCUIT", Create_Circuit_Wrap, SYM_FUNC, 0));
    append_sym_to_table(context, create_symbol("OR", Or_Wrap, SYM_FUNC, 3));
    append_sym_to_table(context, create_symbol("AND", And_Wrap, SYM_FUNC, 4));
    append_sym_to_table(context, create_symbol("NOT", Not_Wrap, SYM_FUNC, 3));
    append_sym_to_table(context, create_symbol("NOR", Nor_Wrap, SYM_FUNC, 4));
    append_sym_to_table(context, create_symbol("NAND", Nand_Wrap, SYM_FUNC, 4));
    append_sym_to_table(context, create_symbol("XOR", Xor_Wrap, SYM_FUNC, 4));
    append_sym_to_table(context, create_symbol("UPDATE_W", Update_Wire_Wrap, SYM_FUNC, 3));
    append_sym_to_table(context, create_symbol("WIRE", Wire_Wrap, SYM_FUNC, 4));
    append_sym_to_table(context, create_symbol("PRINT", Print_Wrap, SYM_FUNC, 1));
    append_sym_to_table(context, create_symbol("RUN", Run_Wrap, SYM_FUNC, 1));
    append_sym_to_table(context, create_symbol("NUMBER", Number_Wrap, SYM_FUNC, 0));

    FILE *fp = fopen("circuit.txt", "rb");
    if(fp == NULL)
    {
        perror("FILE NOT FOUND");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);

    char *circuit_txt = malloc(file_size+1);
    fseek(fp, 0, SEEK_SET);
    
    fread(circuit_txt, file_size, 1, fp);
    circuit_txt[file_size] = '\0';
    fclose(fp);

    RowParser **row_list = parse(context, context->stack, circuit_txt, 1);

    int print = 0;
    int print_partial = 1;
    
    if(print)
    {
        printf("----------");
        printf("\nSTART - PRIMA\n");
        for (int i = 0; i < context->stack->list.count; i++)
        {
            printf("\n");
            printf("%d) --- \n", i + 1);
            print_objects(context, context->stack->list.objs[i], 0);
        }
        printf("FINE\n");

        if(!print_partial)
        {
            int i = 0;
            while (row_list[i] != NULL)
            {
                printf("ROW: %d) ", row_list[i]->row);
                print_objects(context, row_list[i]->content, 0);
                printf("\n");
                i++;
            }
        }
    }
    
    execute_parsing(context, row_list);

    if (print)
    {
        printf("----------");
        printf("\nSTART - END\n");
        for (int i = 0; i < context->stack->list.count; i++)
        {
            printf("\n");
            printf("%d) --- \n", i + 1);
            print_objects(context, context->stack->list.objs[i], 1);
        }
        printf("FINE\n");

        if(!print_partial)
        {
            int i = 0;
            while (row_list[i] != NULL)
            {
                printf("ROW: %d) ", row_list[i]->row);
                print_objects(context, row_list[i]->content, 1);
                printf("\n");
                i++;
            }
        }
    }

    return 0;
}