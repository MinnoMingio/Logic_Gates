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
        if(gates[i]->content->tran_a->input_wire[0] == wire || gates[i]->content->tran_b->input_wire[0] == wire)
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
    Wire *input = create_wire(circuit, "input", 0);
    Wire *a = create_wire(circuit, "a", 0);
    Wire *b = create_wire(circuit, "b", 0);
    
    switch (type)
    {
    case VCC:
        gate->content = create_gate("VCC_Gate", 0);
        gate->content->output_trans->output_wire = create_wire(circuit, "VCC_Wire", 1);
        gate->content->output_trans->output_wire->driven = 0;
        break;
    case OR:
        gate->content = create_OR_Gate(circuit, a, b);
        break;
    case AND:
        gate->content = create_AND_Gate(circuit, input, a, b);
        break;
    case NOT:
        gate->content = create_NOT_Gate(circuit, input, NULL);
        break;
    case NOR:
        gate->content = create_NOR_Gate(circuit, a, b);
        break;
    case NAND:
        gate->content = create_NAND_Gate(circuit, input, a, b);
        break;
    case XOR:
        gate->content = create_XOR_Gate(circuit, a, b);
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

    CGate *vcc_a_gate = createGate(circuit, gateList, "vcc_a_gate", VCC);
    vcc_a_gate->position.x = 100;
    vcc_a_gate->position.y = 500;
    CGate *vcc_b_gate = createGate(circuit, gateList, "vcc_b_gate", VCC);
    vcc_b_gate->position.x = 100;
    vcc_b_gate->position.y = 600;
    CGate *gate1 = createGate(circuit, gateList, "gate1", NOT);
    gate1->position.x = 500;
    gate1->position.y = 500;
    CGate *gate2 = createGate(circuit, gateList, "gate2", OR);
    gate2->position.x = 200;
    gate2->position.y = 200;
    CGate *gate3 = createGate(circuit, gateList, "gate3", AND);
    gate3->position.x = 600;
    gate3->position.y = 200;
    CGate *gate4 = createGate(circuit, gateList, "gate4", XOR);
    gate4->position.x = 800;
    gate4->position.y = 200;

    vcc_a_gate->content->output_trans->output_wire->power = 1;
    int tran_a = 1;
    CGate *gate_clicked_first = NULL;
    CGate *gate_clicked_second = NULL;
    while(!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(WHITE);

        if(IsKeyPressed(KEY_C))
        {
            run_circuit(circuit);
            printf("compiled!\n");
        }
        else if(IsKeyPressed(KEY_N))
        {
            printf("NOW ON TRAN A\n");
            tran_a = 1;
        }else if(IsKeyPressed(KEY_M))
        {
            printf("NOW ON TRAN B\n");
            tran_a = 0;
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
            
            Wire *out_wire = current_gate->content->output_trans->output_wire;

            CGate *output_gate = findGatebyOutputWire(gateList->elms, gateList->count, out_wire);
            if (output_gate != NULL)
            {
                if(output_gate->content->type == 1)
                {
                }
                Color lineColor = BLACK;
                if(output_gate->content->tran_a != NULL && output_gate->content->tran_a->input_wire[0] != NULL)
                {
                    lineColor = output_gate->content->tran_a->input_wire[0]->power == 0 ? RED : YELLOW;
                    DrawLine(output_gate->position.x, output_gate->position.y, current_gate->position.x+100, 15 + current_gate->position.y + (20), lineColor);
                }else if(output_gate->content->tran_b != NULL && output_gate->content->tran_b->input_wire[0] != NULL)
                {
                    lineColor = output_gate->content->tran_b->input_wire[0]->power == 0 ? RED : YELLOW;
                    DrawLine(output_gate->position.x, output_gate->position.y, current_gate->position.x+100, 20 + current_gate->position.y + (20), lineColor);
                }
                Vector2 point = {current_gate->position.x+100, 15 + current_gate->position.y + (20)};
                DrawCircleV(point, 6, lineColor); // 'to' = l'endpoint verso cui va la linea
            }

            Rectangle rect = {current_gate->position.x, current_gate->position.y, 100, 50};
            if (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if((gate_clicked_first != NULL && gate_clicked_second != NULL && gate_clicked_first != gate_clicked_second) || (gate_clicked_first == NULL || gate_clicked_second == NULL))
                {
                    if (gate_clicked_first == NULL)
                    {
                        gate_clicked_first = current_gate;
                        printf("A\n");
                    }else
                    {
                        gate_clicked_second = current_gate;
                        printf("B\n");
                    }
                    
                    if (gate_clicked_first != NULL && gate_clicked_second != NULL)
                    {
                        printf("COUPLED\n");
                        printf("%s\n", gate_clicked_first->content->name);
                        printf("%s\n", gate_clicked_second->content->name);

                        //gate_clicked_first->content->output_trans->output_wire = gate_clicked_second->content->tran_a->input_wire[0];
                        if(tran_a == 1)
                        {
                            gate_clicked_second->content->tran_a->input_wire[0] = gate_clicked_first->content->output_trans->output_wire;
                            if (gate_clicked_second->gateType == AND)
                            {
                                gate_clicked_second->content->tran_a->control_wire = gate_clicked_first->content->output_trans->output_wire;
                            }
                            
                        }else
                        {
                            gate_clicked_second->content->tran_b->input_wire[0] = gate_clicked_first->content->output_trans->output_wire;
                            if (gate_clicked_second->gateType == AND)
                            {
                                gate_clicked_second->content->tran_b->control_wire = gate_clicked_first->content->output_trans->output_wire;
                            }
                        }

                        gate_clicked_first = NULL;
                        gate_clicked_second = NULL;
                    }
                }
            }
            else if (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                printf("IN_A: %d, IN_B: %d, OUT: %d\n", current_gate->content->tran_a->input_wire[0]->power, current_gate->content->tran_b->input_wire[0]->power, current_gate->content->output_trans->output_wire->power);
            }else if (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
            {
                CGate *input_gate = findGatebyOutputWire(gateList->elms, gateList->count, current_gate->content->output_trans->output_wire);
                if(tran_a == 1)
                input_gate->content->tran_a->input_wire[0] = NULL;
                else
                input_gate->content->tran_b->input_wire[0] = NULL;
            }else if (CheckCollisionPointRec(GetMousePosition(), rect) && IsKeyPressed(KEY_O))
            {
                current_gate->content->output_trans->output_wire->power = !current_gate->content->output_trans->output_wire->power;
            }
        }
        
        EndDrawing();
    }

    CloseWindow();
}