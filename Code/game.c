#include<raylib.h>
#include "Circuit_Types.h"

typedef enum
{
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

typedef struct Gate
{
    GateType gateType;
    Transistor content;
    CVector2 position;
} Gate;

CVector2 GetTextCentered(int x, int y, int width, int height, char *text, int fontSize)
{
    CVector2 center = {0, 0};
    center.x = x + (width-MeasureText(text, fontSize))/2;
    center.y = y + ((height-fontSize)/2);
    return center;
}

void DrawGate(Gate *gate, int width, int height)
{
    DrawRectangle(gate->position.x, gate->position.y, width, height, RED);
    char *gate_text;
    int length = 0;
    switch (gate->gateType)
    {
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

int main(void)
{
    InitWindow(1000, 800, "Circuit");

    Gate *gate = malloc(sizeof(Gate));
    gate->gateType = NOT;
    gate->position.x = 500;
    gate->position.y = 500;
    while(!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(WHITE);

        gate->position.x = GetMousePosition().x;
        gate->position.y = GetMousePosition().y;
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
        DrawGate(gate, 100, 50);
        
        EndDrawing();
    }

    CloseWindow();
}