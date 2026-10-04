#include<raylib.h>
#include "Circuit_Types.h"

typedef enum
{
    VCC,
    OR,
    AND,
    NOT,
    NOR,
    NAND,
    XOR,
} GateType;

typedef struct CVector2
{
    int x;
    int y;
} CVector2;

typedef struct CGate
{
    GateType gateType;
    Gate *content;
    CVector2 position;
} CGate;

typedef struct GateList
{
    CGate **elms;
    int count;
} GateList;

CGate *findGatebyOutputWire(CGate **gates, int count, Wire *wire)
{
    for(int i = 0; i < count; i++)
    {
        if(gates[i]->content->output == wire)
        {
            return gates[i];
        }
    }
    return NULL;
}

CVector2 GetTextCentered(int x, int y, int width, int height, char *text, int fontSize)
{
    CVector2 center = {0, 0};
    center.x = x + (width-MeasureText(text, fontSize))/2;
    center.y = y + ((height-fontSize)/2);
    return center;
}

void DrawGate(CGate *gate, int width, int height)
{
    DrawRectangle(gate->position.x, gate->position.y, width, height, RED);
    char *gate_text;
    int length = 0;
    switch (gate->gateType)
    {
    case VCC:
        gate_text = "VCC";
        break;
    case OR:
        gate_text = "OR";
        break;
    case AND:
        gate_text = "AND";
        break;
    case NOT:
        gate_text = "NOT";
        break;
    case NOR:
        gate_text = "NOR";
        break;
    case NAND:
        gate_text = "NAND";
        break;
    case XOR:
        gate_text = "XOR";
        break;
    default:
        gate_text = "NONE";
        break;
    }
    CVector2 center_position = GetTextCentered(gate->position.x, gate->position.y, width, height, gate_text, 20);
    DrawText(gate_text, center_position.x, center_position.y, 20, BLACK);

    
}

void appendGate(GateList *list, CGate *gate)
{
    int length = list->count;
    list->elms = realloc(list->elms, sizeof(CGate*) * (length+1));
    list->elms[length] = gate;
    list->count++;
}

CGate *createGate(Circuit *circuit, GateList *gateList, char *name, GateType type)
{
    CGate *gate = malloc(sizeof(CGate));
    
    switch (type)
    {
    case VCC:
        gate->content = create_gate("VCC_Gate", 0);
        gate->content->output = create_wire(circuit, "VCC_Wire", 1);
        gate->content->output->driven = 0;
        break;
    case OR:
        gate->content = create_OR_Gate(circuit, NULL, NULL);
        break;
    case AND:
        gate->content = create_AND_Gate(circuit, NULL, NULL, NULL);
        break;
    case NOT:
        gate->content = create_NOT_Gate(circuit, NULL, NULL);
        break;
    case NOR:
        gate->content = create_NOR_Gate(circuit, NULL, NULL);
        break;
    case NAND:
        gate->content = create_NAND_Gate(circuit, NULL, NULL, NULL);
        break;
    case XOR:
        gate->content = create_XOR_Gate(circuit, NULL, NULL);
        break;
    default:
        break;
    }
    
    //gate->content = create_transistor(name, 2);
    gate->gateType = type;
    gate->position.x = 0;
    gate->position.y = 0;
    appendGate(gateList, gate);
    return gate;
}

int main(void)
{
    InitWindow(1000, 800, "Circuit");

    Circuit *circuit = create_circuit();
    GateList *gateList = malloc(sizeof(GateList));
    gateList->elms = NULL;
    gateList->count = 0;

    CGate *vcc_gate = createGate(circuit, gateList, "vcc_gate", VCC);
    vcc_gate->position.x = 100;
    vcc_gate->position.y = 500;
    CGate *gate1 = createGate(circuit, gateList, "gate1", NOT);
    gate1->position.x = 500;
    gate1->position.y = 500;
    CGate *gate2 = createGate(circuit, gateList, "gate2", OR);
    gate2->position.x = 200;
    gate2->position.y = 200;
    CGate *gate3 = createGate(circuit, gateList, "gate3", AND);
    gate3->position.x = 600;
    gate3->position.y = 200;

    int a_selected = 1;
    CGate *gate_clicked_first = NULL;
    CGate *gate_clicked_second = NULL;
    while(!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(WHITE);

        if(IsKeyPressed(KEY_A))
        {
            run_circuit(circuit);
            printf("compiled!\n");
        }
        if(IsKeyPressed(KEY_ONE))
        {
            a_selected == 1;
        }else if(IsKeyPressed(KEY_TWO))
        {
            a_selected = 0;
        }

        //findGatebyOutputWire()
        
        /*
        if(IsKeyDown(KEY_ONE))
        {
            gate->gateType = OR;
        }else if(IsKeyDown(KEY_ONE))
        {
            gate->gateType = AND;
        }else if(IsKeyDown(KEY_TWO))
        {
            gate->gateType = AND;
        }else if(IsKeyDown(KEY_THREE))
        {
            gate->gateType = NOT;
        }
        */
        for(int i = 0; i < gateList->count; i++)
        {
            CGate *current_gate = gateList->elms[i];
            if(current_gate != NULL) DrawGate(current_gate, 100, 50);

            Wire *out_wire = current_gate->content->output;

            CGate *output_gate = findGatebyOutputWire(gateList->elms, gateList->count, out_wire);
            if (output_gate != NULL)
            {
                Color lineColor = BLACK;
                if(output_gate->content->a != NULL)
                {
                    lineColor = output_gate->content->a->power == 0 ? RED : YELLOW;
                }
                //DrawLine(output_gate->position.x, output_gate->position.y, current_gate->position.x, 15 + current_gate->position.y + (i * 20), lineColor);
                Vector2 point = {current_gate->position.x, 15 + current_gate->position.y + (i * 20)};
                DrawCircleV(point, 6, lineColor); // 'to' = l'endpoint verso cui va la linea
            }

            Rectangle rect = {current_gate->position.x, current_gate->position.y, 100, 50};
            if (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                // click avvenuto proprio in questo frame, dentro il rettangolo
                if(gate_clicked_first != NULL)
                {
                    gate_clicked_second = current_gate;
                    if(a_selected)
                    {
                        if (gate_clicked_second->content->a == NULL)
                        {
                            gate_clicked_second->content->a = create_wire(circuit, "a_wire", gate_clicked_first->content->output->power);
                            gate_clicked_first->content->output = gate_clicked_second->content->a;
                        }else
                        {
                            gate_clicked_second->content->a = gate_clicked_first->content->output;
                        }
                        printf("Done");
                    }else
                    {
                        if (gate_clicked_second->content->b == NULL)
                        {
                            gate_clicked_second->content->b = create_wire(circuit, "b_wire", 1);
                        }
                        gate_clicked_first->content->output = gate_clicked_second->content->b;

                    }
                    
                    gate_clicked_first = NULL;
                    gate_clicked_second = NULL;
                }
                else
                {
                    gate_clicked_first = current_gate;
                }
            }else if (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_SHIFT))
            {
                current_gate->position.x = GetMousePosition().x;
                current_gate->position.y = GetMousePosition().y;
            }
            
        }
        
        EndDrawing();
    }

    CloseWindow();
}