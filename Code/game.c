#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include "Circuit_Types.h"
#include "Circuit.h"

#define GATE_W 100
#define GATE_H 50

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

    // Solo per NOR / NAND / XOR (gate composti): i loro ingressi sono "fissati"
    // alla creazione su due wire interni (pin_net). Per collegarci un'altra uscita
    // usiamo un transistor "buffer" (pin_buf) che copia il segnale sorgente sul wire
    // interno. Indice 0 = pin A, indice 1 = pin B.
    Wire *pin_net[2];
    Transistor *pin_buf[2];

    // Wire di uscita del gate (per TUTTI i tipi, composti inclusi).
    Wire *out_wire;
} CGate;

typedef struct GateList
{
    CGate **elms;
    int count;
} GateList;

// Wire sempre a 0, usato al posto di NULL quando si scollega un pin
// (cosi' il simulatore non trova mai puntatori NULL negli ingressi).
static Wire *g_ground = NULL;

/* ------------------------------------------------------------------ */
/*  Helper                                                             */
/* ------------------------------------------------------------------ */

CVector2 GetTextCentered(int x, int y, int width, int height, char *text, int fontSize)
{
    CVector2 center = {0, 0};
    center.x = x + (width - MeasureText(text, fontSize)) / 2;
    center.y = y + ((height - fontSize) / 2);
    return center;
}

Wire *GetOutputWire(CGate *g)
{
    if (g == NULL) return NULL;
    return g->out_wire;
}

Transistor *GetPinTransistor(CGate *g, int pin_a)
{
    if (g == NULL || g->content == NULL) return NULL;
    return pin_a ? g->content->tran_a : g->content->tran_b;
}

int IsComposite(CGate *g)
{
    return g->gateType == NOR || g->gateType == NAND || g->gateType == XOR;
}

// Wire attualmente collegato al pin (NULL = scollegato).
//  - OR / NOT        : input_wire[0] del transistor del pin
//  - AND             : control_wire del transistor del pin
//  - NOR/NAND/XOR    : ingresso del buffer di quel pin
Wire *GetPinWire(CGate *g, int pin_a)
{
    Wire *w = NULL;
    int idx = pin_a ? 0 : 1;

    if (IsComposite(g))
    {
        if (g->pin_buf[idx] != NULL && g->pin_buf[idx]->input_wire != NULL)
            w = g->pin_buf[idx]->input_wire[0];
    }
    else
    {
        Transistor *t = GetPinTransistor(g, pin_a);
        if (t != NULL)
        {
            if (g->gateType == OR)
            {
                if (t->input_wire) w = t->input_wire[0];
            }
            else if (g->gateType == NOT)
            {
                if (pin_a && t->input_wire) w = t->input_wire[0];
            }
            else if (g->gateType == AND)
            {
                w = t->control_wire;
            }
        }
    }

    if (w == g_ground) return NULL;
    return w;
}

int GetPinCount(CGate *g)
{
    switch (g->gateType)
    {
    case OR:
    case AND:
    case NOR:
    case NAND:
    case XOR: return 2;
    case NOT: return 1;
    default:  return 0;
    }
}

// Scrive il wire nel punto GIUSTO per il tipo di gate (w = NULL scollega).
void SetPinWire(Circuit *circuit, CGate *g, int pin_a, Wire *w)
{
    Wire *value = w ? w : g_ground;

    if (IsComposite(g))
    {
        int idx = pin_a ? 0 : 1;
        if (g->pin_net[idx] == NULL)
        {
            printf("pin non disponibile su questo gate\n");
            return;
        }

        // il buffer si crea la prima volta che serve e poi si riusa
        if (g->pin_buf[idx] == NULL)
        {
            Transistor *buf = create_transistor("BUF", 1);
            update_transistor(buf, 1, 0);          // sempre "aperto", non invertente
            buf->output_wire = g->pin_net[idx];    // scrive sul wire interno del gate
            append_t_to_c(circuit, buf);
            g->pin_buf[idx] = buf;
        }
        g->pin_buf[idx]->input_wire[0] = value;
        return;
    }

    Transistor *t = GetPinTransistor(g, pin_a);
    if (t == NULL)
    {
        printf("pin non disponibile su questo gate\n");
        return;
    }

    switch (g->gateType)
    {
    case OR:
        if (t->input_wire) t->input_wire[0] = value;
        break;
    case NOT:
        if (pin_a) { if (t->input_wire) t->input_wire[0] = value; }
        else printf("il NOT ha un solo ingresso (pin A)\n");
        break;
    case AND:
        t->control_wire = w;
        if (w == NULL) t->state = 0;
        break;
    default:
        printf("questo gate non ha ingressi collegabili\n");
        break;
    }
}

void ConnectGate(Circuit *circuit, CGate *src, CGate *dst, int pin_a)
{
    Wire *w = GetOutputWire(src);
    if (w == NULL)
    {
        printf("il gate sorgente non ha un'uscita\n");
        return;
    }
    SetPinWire(circuit, dst, pin_a, w);
}

Vector2 GetOutputPos(CGate *g)
{
    Vector2 p = {(float)(g->position.x + GATE_W), (float)(g->position.y + GATE_H / 2)};
    return p;
}

Vector2 GetPinPos(CGate *g, int pin_a)
{
    float y = (GetPinCount(g) == 1) ? GATE_H / 2.0f : (pin_a ? GATE_H * 0.25f : GATE_H * 0.75f);
    Vector2 p = {(float)g->position.x, g->position.y + y};
    return p;
}

/* ------------------------------------------------------------------ */
/*  Disegno                                                            */
/* ------------------------------------------------------------------ */

// ---- Palette (tema scuro "neon") ----
#define RGB(r, g, b) ((Color){(r), (g), (b), 255})

#define COL_BG        RGB(15, 18, 32)     // sfondo
#define COL_GRID      RGB(26, 31, 52)     // puntini della griglia
#define COL_PANEL     RGB(22, 26, 46)     // barre in alto/in basso
#define COL_PANEL_LN  RGB(46, 54, 88)     // bordo delle barre
#define COL_TEXT      RGB(232, 235, 247)
#define COL_TEXT_DIM  RGB(128, 137, 172)
#define COL_WIRE_OFF  RGB(64, 73, 108)    // filo spento
#define COL_WIRE_ON   RGB(232, 190, 90)   // filo acceso (giallo caldo)
#define COL_PIN_IDLE  RGB(88, 98, 134)   // pin non collegato
#define COL_SELECT    RGB(110, 160, 220)   // selezione / anteprima collegamento

// Colore di ogni tipo di gate: ogni famiglia ha il suo tono.
Color GateColor(GateType t)
{
    switch (t)
    {
    case VCC:  return RGB(214, 167, 69);   // ocra
    case OR:   return RGB(84, 170, 160);   // verde acqua spento
    case AND:  return RGB(140, 124, 200);  // lavanda
    case NOT:  return RGB(200, 110, 150);  // rosa antico
    case NOR:  return RGB(90, 150, 200);   // blu acciaio
    case NAND: return RGB(170, 110, 190);  // malva
    case XOR:  return RGB(200, 130, 80);   // rame
    default:   return RGB(140, 150, 170);
    }
}

// Mescola due colori: t = 0 -> a, t = 1 -> b
Color Mix(Color a, Color b, float t)
{
    Color c;
    c.r = (unsigned char)(a.r + (b.r - a.r) * t);
    c.g = (unsigned char)(a.g + (b.g - a.g) * t);
    c.b = (unsigned char)(a.b + (b.b - a.b) * t);
    c.a = 255;
    return c;
}

void DrawBackgroundGrid(int w, int h)
{
    for (int y = 56 + 16; y < h - 34; y += 32)
        for (int x = 16; x < w; x += 32)
            DrawRectangle(x, y, 2, 2, COL_GRID);
}

void DrawGate(CGate *gate, int width, int height, int hovered, int selected)
{
    int bx = gate->position.x;
    int by = gate->position.y;
    Color accent = GateColor(gate->gateType);

    // corpo scuro appena tinto del colore del gate; bordo sottile
    Color body_col = Mix(COL_BG, accent, hovered ? 0.30f : 0.22f);
    Color border_col = selected ? COL_SELECT : Mix(COL_BG, accent, 0.55f);

    // ombra discreta
    DrawRectangle(bx + 3, by + 4, width, height, Fade(BLACK, 0.30f));

    if (selected)
        DrawRectangle(bx - 5, by - 5, width + 10, height + 10, Fade(COL_SELECT, 0.18f));

    DrawRectangle(bx - 1, by - 1, width + 2, height + 2, border_col);
    DrawRectangle(bx, by, width, height, body_col);

    // barra d'accento a sinistra
    DrawRectangle(bx, by, 3, height, accent);

    char *gate_text;
    switch (gate->gateType)
    {
    case VCC:  gate_text = "VCC";  break;
    case OR:   gate_text = "OR";   break;
    case AND:  gate_text = "AND";  break;
    case NOT:  gate_text = "NOT";  break;
    case NOR:  gate_text = "NOR";  break;
    case NAND: gate_text = "NAND"; break;
    case XOR:  gate_text = "XOR";  break;
    default:   gate_text = "NONE"; break;
    }
    CVector2 center_position = GetTextCentered(bx, by, width, height, gate_text, 18);
    DrawText(gate_text, center_position.x, center_position.y, 18, COL_TEXT);
}

// Un filo: curva morbida, sottile; alone leggero se e' acceso.
void DrawWire(Vector2 from, Vector2 to, int on)
{
    if (on)
    {
        DrawLineBezier(from, to, 8.0f, Fade(COL_WIRE_ON, 0.10f));
        DrawLineBezier(from, to, 2.5f, COL_WIRE_ON);
    }
    else
    {
        DrawLineBezier(from, to, 2.5f, COL_WIRE_OFF);
    }
}

// Solo i fili (da disegnare PRIMA dei gate, cosi' restano sotto).
void DrawConnections(GateList *list)
{
    for (int d = 0; d < list->count; d++)
    {
        CGate *dst = list->elms[d];
        int pins = GetPinCount(dst);

        for (int p = 0; p < pins; p++)
        {
            int pin_a = (p == 0);
            Wire *w = GetPinWire(dst, pin_a);
            if (w == NULL) continue;

            for (int s = 0; s < list->count; s++)
            {
                CGate *src = list->elms[s];
                if (src == dst) continue;
                if (GetOutputWire(src) != w) continue;

                DrawWire(GetOutputPos(src), GetPinPos(dst, pin_a), w->power);
            }
        }
    }
}

// Pin di ingresso (con etichetta A/B) e porta di uscita di ogni gate.
void DrawPorts(GateList *list)
{
    for (int i = 0; i < list->count; i++)
    {
        CGate *g = list->elms[i];
        int pins = GetPinCount(g);

        for (int p = 0; p < pins; p++)
        {
            int pin_a = (p == 0);
            Vector2 pos = GetPinPos(g, pin_a);
            Wire *w = GetPinWire(g, pin_a);

            Color c = w ? (w->power ? COL_WIRE_ON : COL_WIRE_OFF) : COL_PIN_IDLE;
            DrawRectangle((int)pos.x - 7, (int)pos.y - 7, 14, 14, COL_BG);
            DrawRectangle((int)pos.x - 5, (int)pos.y - 5, 10, 10, c);

            if (pins == 2)
                DrawText(pin_a ? "A" : "B", (int)pos.x + 11, (int)pos.y - 5, 10, COL_TEXT_DIM);
        }

        Wire *out = GetOutputWire(g);
        if (out)
        {
            Vector2 pos = GetOutputPos(g);
            if (out->power)
            {
                DrawRectangle((int)pos.x - 13, (int)pos.y - 13, 26, 26, Fade(COL_WIRE_ON, 0.08f));
                DrawRectangle((int)pos.x - 10, (int)pos.y - 10, 20, 20, Fade(COL_WIRE_ON, 0.14f));
            }
            DrawRectangle((int)pos.x - 7, (int)pos.y - 7, 14, 14, COL_BG);
            DrawRectangle((int)pos.x - 5, (int)pos.y - 5, 10, 10, out->power ? COL_WIRE_ON : COL_WIRE_OFF);
        }
    }
}

// Barra in alto (titolo + modalita' pin) e barra in basso (comandi).
void DrawHud(int pin_a, int w, int h)
{
    DrawRectangle(0, 0, w, 56, COL_PANEL);
    DrawRectangle(0, 55, w, 1, COL_PANEL_LN);
    DrawText("CIRCUIT LAB", 20, 15, 26, COL_TEXT);

    Rectangle pill = {(float)(w - 200), 11, 180, 34};
    Color pill_col = pin_a ? RGB(70, 110, 180) : RGB(140, 84, 160);
    DrawRectangle((int)pill.x, (int)pill.y, (int)pill.width, (int)pill.height, pill_col);
    DrawText(pin_a ? "COLLEGO SU: PIN A" : "COLLEGO SU: PIN B", (int)pill.x + 16, (int)pill.y + 10, 14, WHITE);

    DrawRectangle(0, h - 34, w, 34, COL_PANEL);
    DrawRectangle(0, h - 34, w, 1, COL_PANEL_LN);
    DrawText("Click: collega  |  N / M: scegli pin  |  Shift+drag: sposta  |  C: simula  |  O: toggle uscita  |  dx: debug  |  centrale: scollega",
             16, h - 23, 13, COL_TEXT_DIM);
}

/* ------------------------------------------------------------------ */
/*  Lista gate / creazione                                             */
/* ------------------------------------------------------------------ */

void appendGate(GateList *list, CGate *gate)
{
    int length = list->count;
    list->elms = realloc(list->elms, sizeof(CGate *) * (length + 1));
    list->elms[length] = gate;
    list->count++;
}

// Uscita di un gate "semplice" (OR / AND / NOT) creato da Circuit.c.
static Wire *SubOut(Gate *g)
{
    if (g == NULL || g->output_trans == NULL) return NULL;
    return g->output_trans->output_wire;
}

// NOR = OR seguito da NOT. Ritorna il wire di uscita.
static Wire *BuildNor(Circuit *circuit, Wire *a, Wire *b)
{
    Gate *or_g = create_OR_Gate(circuit, a, b);
    Gate *not_g = create_NOT_Gate(circuit, SubOut(or_g), NULL);
    return SubOut(not_g);
}

CGate *createGate(Circuit *circuit, GateList *gateList, char *name, GateType type)
{
    CGate *gate = malloc(sizeof(CGate));
    gate->pin_net[0] = gate->pin_net[1] = NULL;
    gate->pin_buf[0] = gate->pin_buf[1] = NULL;
    gate->out_wire = NULL;
    gate->content = NULL;

    switch (type)
    {
    case VCC:
    {
        gate->content = create_gate("VCC_Gate", 0);
        gate->out_wire = create_wire(circuit, "VCC_Wire", 1);
        gate->out_wire->driven = 0;
        break;
    }
    case OR:
    {
        Wire *a = create_wire(circuit, "or_a", 0);
        Wire *b = create_wire(circuit, "or_b", 0);
        gate->content = create_OR_Gate(circuit, a, b);
        gate->out_wire = SubOut(gate->content);
        break;
    }
    case AND:
    {
        Wire *power = create_wire(circuit, "and_power", 1);
        power->driven = 0;
        Wire *a = create_wire(circuit, "and_a", 0);
        Wire *b = create_wire(circuit, "and_b", 0);
        gate->content = create_AND_Gate(circuit, power, a, b);
        gate->out_wire = SubOut(gate->content);
        break;
    }
    case NOT:
    {
        Wire *in = create_wire(circuit, "not_in", 0);
        gate->content = create_NOT_Gate(circuit, in, NULL);
        gate->out_wire = SubOut(gate->content);
        break;
    }
    case NOR:
    {
        // costruito qui a mano (OR + NOT) cosi' conosciamo con certezza il wire di uscita
        Wire *a = create_wire(circuit, "nor_a", 0);
        Wire *b = create_wire(circuit, "nor_b", 0);
        gate->pin_net[0] = a;
        gate->pin_net[1] = b;
        gate->content = create_gate("NOR_Gate", 1);
        gate->out_wire = BuildNor(circuit, a, b);
        break;
    }
    case NAND:
    {
        // AND + NOT
        Wire *power = create_wire(circuit, "nand_power", 1);
        power->driven = 0;
        Wire *a = create_wire(circuit, "nand_a", 0);
        Wire *b = create_wire(circuit, "nand_b", 0);
        gate->pin_net[0] = a;
        gate->pin_net[1] = b;
        gate->content = create_gate("NAND_Gate", 1);
        Gate *and_g = create_AND_Gate(circuit, power, a, b);
        Gate *not_g = create_NOT_Gate(circuit, SubOut(and_g), NULL);
        gate->out_wire = SubOut(not_g);
        break;
    }
    case XOR:
    {
        // stessa struttura di prima: NOR( NOR(a,b), NOR(not a, not b) )
        Wire *a = create_wire(circuit, "xor_a", 0);
        Wire *b = create_wire(circuit, "xor_b", 0);
        gate->pin_net[0] = a;
        gate->pin_net[1] = b;
        gate->content = create_gate("XOR_Gate", 1);

        Wire *not_a = SubOut(create_NOT_Gate(circuit, a, NULL));
        Wire *not_b = SubOut(create_NOT_Gate(circuit, b, NULL));
        Wire *nor_ab = BuildNor(circuit, a, b);
        Wire *nor_not_ab = BuildNor(circuit, not_a, not_b);
        gate->out_wire = BuildNor(circuit, nor_ab, nor_not_ab);
        break;
    }
    default:
        break;
    }

    if (gate->out_wire == NULL)
        printf("ATTENZIONE: gate '%s' senza wire di uscita (output_trans non impostato?)\n", name);

    gate->gateType = type;
    gate->position.x = 0;
    gate->position.y = 0;
    appendGate(gateList, gate);
    return gate;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1000, 800, "Circuit Lab");
    SetTargetFPS(60);

    Circuit *circuit = create_circuit();
    GateList *gateList = malloc(sizeof(GateList));
    gateList->elms = NULL;
    gateList->count = 0;

    g_ground = create_wire(circuit, "ground", 0);
    g_ground->driven = 0;

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
    CGate *gate4 = createGate(circuit, gateList, "gate4", NOR);
    gate4->position.x = 800;
    gate4->position.y = 100;
    CGate *gate5 = createGate(circuit, gateList, "gate5", NAND);
    gate5->position.x = 800;
    gate5->position.y = 250;
    CGate *gate6 = createGate(circuit, gateList, "gate6", XOR);
    gate6->position.x = 800;
    gate6->position.y = 400;
    (void)gate1; (void)gate2; (void)gate3; (void)gate4; (void)gate5; (void)gate6;

    GetOutputWire(vcc_a_gate)->power = 1;
    GetOutputWire(vcc_b_gate)->power = 0;

    int pin_a = 1;
    CGate *gate_clicked_first = NULL;

    while (!WindowShouldClose())
    {
        // ------------------- tastiera -------------------
        if (IsKeyPressed(KEY_C))
        {
            run_circuit(circuit);
            printf("compiled!\n");
        }
        else if (IsKeyPressed(KEY_N))
        {
            printf("NOW ON PIN A\n");
            pin_a = 1;
        }
        else if (IsKeyPressed(KEY_M))
        {
            printf("NOW ON PIN B\n");
            pin_a = 0;
        }

        // ------------------- mouse -------------------
        Vector2 mouse = GetMousePosition();
        int shift = IsKeyDown(KEY_LEFT_SHIFT);

        for (int i = 0; i < gateList->count; i++)
        {
            CGate *g = gateList->elms[i];
            Rectangle rect = {(float)g->position.x, (float)g->position.y, GATE_W, GATE_H};
            if (!CheckCollisionPointRec(mouse, rect)) continue;

            if (shift && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                g->position.x = (int)mouse.x - GATE_W / 2;
                g->position.y = (int)mouse.y - GATE_H / 2;
            }
            else if (!shift && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (gate_clicked_first == NULL)
                {
                    gate_clicked_first = g;
                }
                else if (gate_clicked_first != g)
                {
                    ConnectGate(circuit, gate_clicked_first, g, pin_a);
                    gate_clicked_first = NULL;
                }
            }
            else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                Wire *wa = GetPinWire(g, 1);
                Wire *wb = GetPinWire(g, 0);
                Wire *wo = GetOutputWire(g);
                printf("IN_A: %d, IN_B: %d, OUT: %d\n",
                       wa ? wa->power : -1,
                       wb ? wb->power : -1,
                       wo ? wo->power : -1);
            }
            else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
            {
                SetPinWire(circuit, g, pin_a, NULL);
            }
            else if (IsKeyPressed(KEY_O))
            {
                Wire *wo = GetOutputWire(g);
                if (wo) wo->power = !wo->power;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !shift)
        {
            int on_gate = 0;
            for (int i = 0; i < gateList->count; i++)
            {
                CGate *g = gateList->elms[i];
                Rectangle rect = {(float)g->position.x, (float)g->position.y, GATE_W, GATE_H};
                if (CheckCollisionPointRec(mouse, rect)) { on_gate = 1; break; }
            }
            if (!on_gate) gate_clicked_first = NULL;
        }

        // ------------------- disegno -------------------
        BeginDrawing();
        ClearBackground(COL_BG);
        DrawBackgroundGrid(GetScreenWidth(), GetScreenHeight());

        // fili sotto a tutto
        DrawConnections(gateList);

        // anteprima del collegamento in corso: dal gate sorgente al mouse
        if (gate_clicked_first != NULL)
            DrawLineBezier(GetOutputPos(gate_clicked_first), mouse, 2.5f, Fade(COL_SELECT, 0.85f));

        // gate
        for (int i = 0; i < gateList->count; i++)
        {
            CGate *g = gateList->elms[i];
            Rectangle rect = {(float)g->position.x, (float)g->position.y, GATE_W, GATE_H};
            int hovered = CheckCollisionPointRec(mouse, rect);
            DrawGate(g, GATE_W, GATE_H, hovered, g == gate_clicked_first);
        }

        // pin e uscite sopra ai gate
        DrawPorts(gateList);

        DrawHud(pin_a, GetScreenWidth(), GetScreenHeight());

        EndDrawing();
    }

    CloseWindow();
    return 0;
}