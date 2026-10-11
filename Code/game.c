//gcc -g game.c Circuit.c Obj.c Parser.c -o game.exe -lraylib -lopengl32 -lgdi32 -lwinmm
#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include "Circuit_Types.h"
#include "Circuit.h"
#include "Obj.h"
#include "Parser.h"
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

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
typedef enum { TAB_EDITOR = 0, TAB_CODE = 1 } AppTab;

Rectangle TabRect(int i)
{
    Rectangle r = {210.0f + i * 134.0f, 16.0f, 124.0f, 40.0f};
    return r;
}

// Barra in alto (titolo + tab) e barra in basso (comandi della tab attiva).
void DrawHud(int tab, int pin_a, int w, int h)
{
    DrawRectangle(0, 0, w, 56, COL_PANEL);
    DrawRectangle(0, 55, w, 1, COL_PANEL_LN);
    DrawText("CIRCUIT LAB", 20, 15, 26, COL_TEXT);

    const char *labels[2] = {"EDITOR  F1", "CODICE  F2"};
    for (int i = 0; i < 2; i++)
    {
        Rectangle r = TabRect(i);
        int active = (tab == i);
        if (active)
        {
            DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, Mix(COL_PANEL, COL_SELECT, 0.22f));
            DrawRectangle((int)r.x, (int)(r.y + r.height) - 3, (int)r.width, 3, COL_SELECT);
        }
        DrawText(labels[i], (int)r.x + 16, (int)r.y + 13, 14, active ? COL_TEXT : COL_TEXT_DIM);
    }

    if (tab == TAB_EDITOR)
    {
        Rectangle pill = {(float)(w - 200), 11, 180, 34};
        Color pill_col = pin_a ? RGB(70, 110, 180) : RGB(140, 84, 160);
        DrawRectangle((int)pill.x, (int)pill.y, (int)pill.width, (int)pill.height, pill_col);
        DrawText(pin_a ? "COLLEGO SU: PIN A" : "COLLEGO SU: PIN B", (int)pill.x + 16, (int)pill.y + 10, 14, WHITE);
    }

    DrawRectangle(0, h - 34, w, 34, COL_PANEL);
    DrawRectangle(0, h - 34, w, 1, COL_PANEL_LN);
    if (tab == TAB_EDITOR)
        DrawText("Click: collega  |  N / M: scegli pin  |  Shift+drag: sposta  |  C: simula  |  O: toggle uscita  |  dx: debug  |  centrale: scollega",
                 16, h - 23, 13, COL_TEXT_DIM);
    else
        DrawText("F5 / Ctrl+Invio: esegui  |  Ctrl+S: salva circuit.txt  |  Tab: indenta  |  click: sposta il cursore  |  rotella: scorri",
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
/*  Tab CODICE: editor di testo + interprete del tuo linguaggio         */
/* ------------------------------------------------------------------ */

#define CODE_MAX 16384
#define OUT_MAX  16384

static char g_code[CODE_MAX];
static int g_code_len = 0;
static int g_cursor = 0;
static int g_code_top = 0;       // prima riga visibile del codice

static char g_out[OUT_MAX];
static int g_out_top = 0;        // prima riga visibile dell'output
static int g_out_status = 0;     // 0 = mai eseguito, 1 = ok, 2 = crash dell'interprete

static Font g_font;
static int g_font_size = 20;

static char g_msg[64];
static double g_msg_until = 0.0;

static void SetMsg(const char *m)
{
    snprintf(g_msg, sizeof(g_msg), "%s", m);
    g_msg_until = GetTime() + 2.5;
}

// Larghezza in pixel dei primi 'len' caratteri di 's'.
static float TextW(const char *s, int len)
{
    char buf[512];
    if (len <= 0) return 0.0f;
    if (len > 511) len = 511;
    memcpy(buf, s, (size_t)len);
    buf[len] = '\0';
    return MeasureTextEx(g_font, buf, (float)g_font_size, 0).x;
}

// ---- gestione del testo ----

static int LineStart(int idx) { while (idx > 0 && g_code[idx - 1] != '\n') idx--; return idx; }
static int LineEnd(int idx)   { while (idx < g_code_len && g_code[idx] != '\n') idx++; return idx; }

static int CountLines(void)
{
    int n = 1;
    for (int i = 0; i < g_code_len; i++) if (g_code[i] == '\n') n++;
    return n;
}

static int CursorLine(void)
{
    int n = 0;
    for (int i = 0; i < g_cursor; i++) if (g_code[i] == '\n') n++;
    return n;
}

// Indice del primo carattere della riga numero 'line' (0 = prima riga).
static int LineIndexStart(int line)
{
    int idx = 0;
    while (line > 0 && idx < g_code_len)
    {
        if (g_code[idx] == '\n') line--;
        idx++;
    }
    return idx;
}

static void CodeInsert(const char *s, int n)
{
    if (g_code_len + n >= CODE_MAX) return;
    memmove(g_code + g_cursor + n, g_code + g_cursor, (size_t)(g_code_len - g_cursor));
    memcpy(g_code + g_cursor, s, (size_t)n);
    g_code_len += n;
    g_cursor += n;
    g_code[g_code_len] = '\0';
}

static void CodeBackspace(void)
{
    if (g_cursor <= 0) return;
    memmove(g_code + g_cursor - 1, g_code + g_cursor, (size_t)(g_code_len - g_cursor));
    g_cursor--;
    g_code_len--;
    g_code[g_code_len] = '\0';
}

static void CodeDelete(void)
{
    if (g_cursor >= g_code_len) return;
    memmove(g_code + g_cursor, g_code + g_cursor + 1, (size_t)(g_code_len - g_cursor - 1));
    g_code_len--;
    g_code[g_code_len] = '\0';
}

static int CodeLoad(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    int n = (int)fread(g_code, 1, CODE_MAX - 1, f);
    fclose(f);

    int w = 0; // tolgo i '\r' dei file Windows
    for (int i = 0; i < n; i++) if (g_code[i] != '\r') g_code[w++] = g_code[i];
    g_code[w] = '\0';
    g_code_len = w;
    g_cursor = 0;
    g_code_top = 0;
    return 1;
}

static int CodeSave(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    fwrite(g_code, 1, (size_t)g_code_len, f);
    fclose(f);
    return 1;
}

// Tasto con ripetizione automatica (tenendolo premuto).
static int KeyRep(int key)
{
    static float held[512];
    static float next[512];
    if (key < 0 || key >= 512) return 0;

    if (IsKeyPressed(key))
    {
        held[key] = 0.0f;
        next[key] = 0.4f;
        return 1;
    }
    if (IsKeyDown(key))
    {
        held[key] += GetFrameTime();
        if (held[key] >= next[key])
        {
            next[key] += 0.035f;
            return 1;
        }
    }
    return 0;
}

// ---- interprete ----

static Context *NewInterpreterContext(void)
{
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

    return context;
}

static jmp_buf g_crash_jmp;
static void CrashHandler(int sig) { (void)sig; longjmp(g_crash_jmp, 1); }

// Esegue il codice scritto nella tab e mette in g_out tutto cio' che l'interprete
// stampa con printf. Se l'interprete va in crash, l'editor resta in piedi.
static void RunCode(void)
{
    g_out[0] = '\0';
    g_out_top = 0;

    FILE *cap = fopen("circuit_out.tmp", "w+b");
    if (cap == NULL)
    {
        snprintf(g_out, OUT_MAX, "[ERRORE] Non riesco a creare circuit_out.tmp per catturare l'output.\n");
        g_out_status = 2;
        return;
    }

    // stdout -> file temporaneo (poi lo rimettiamo a posto)
    fflush(stdout);
    int saved = dup(fileno(stdout));
    dup2(fileno(cap), fileno(stdout));

    // l'interprete puo' tenere puntatori dentro al testo: lo copio e non lo libero
    char *txt = strdup(g_code);
    int crashed = 0;

    void (*old_segv)(int) = signal(SIGSEGV, CrashHandler);
    void (*old_fpe)(int) = signal(SIGFPE, CrashHandler);

    if (setjmp(g_crash_jmp) == 0)
    {
        Context *context = NewInterpreterContext();
        RowParser **rows = parse(context, context->stack, txt, 1);
        execute_parsing(context, rows);
    }
    else
    {
        crashed = 1;
    }

    signal(SIGSEGV, old_segv);
    signal(SIGFPE, old_fpe);

    fflush(stdout);
    if (saved >= 0)
    {
        dup2(saved, fileno(stdout));
        close(saved);
    }

    fseek(cap, 0, SEEK_SET);
    size_t n = fread(g_out, 1, OUT_MAX - 128, cap);
    g_out[n] = '\0';
    fclose(cap);
    remove("circuit_out.tmp");

    // tolgo i '\r'
    size_t w = 0;
    for (size_t i = 0; i < n; i++) if (g_out[i] != '\r') g_out[w++] = g_out[i];
    g_out[w] = '\0';

    if (crashed)
    {
        strcat(g_out, "\n[ERRORE] L'interprete e' andato in crash (accesso a memoria non valido).\n"
                      "Controlla l'ultima riga eseguita: variabile non definita o numero di parametri sbagliato?\n");
        g_out_status = 2;
    }
    else
    {
        g_out_status = 1;
    }
}

// ---- layout ----

static void CodeLayout(int W, int H, Rectangle *code, Rectangle *out)
{
    float top = 70.0f;
    float bottom = (float)(H - 34 - 14);
    float total = (float)(W - 48);
    float cw = total * 0.58f;

    code->x = 16.0f;               code->y = top; code->width = cw;         code->height = bottom - top;
    out->x = 16.0f + cw + 16.0f;   out->y = top;  out->width = total - cw;  out->height = bottom - top;
}

// idx: 0 = CARICA, 1 = SALVA, 2 = ESEGUI
static Rectangle CodeBtn(Rectangle code, int idx)
{
    Rectangle r = {code.x + code.width - 84.0f * (idx + 1), code.y + 3.0f, 78.0f, 24.0f};
    return r;
}

// ---- input della tab ----

static void CodeUpdate(int W, int H, Vector2 mouse)
{
    Rectangle code, out;
    CodeLayout(W, H, &code, &out);

    Rectangle ta = {code.x + 1, code.y + 30, code.width - 2, code.height - 31};
    float lh = (float)(g_font_size + 4);
    float text_x = ta.x + 52;
    float text_y = ta.y + 8;
    int visible = (int)((ta.height - 12) / lh);
    if (visible < 1) visible = 1;

    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int moved = 0;

    // ---- mouse ----
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (CheckCollisionPointRec(mouse, CodeBtn(code, 2)))
        {
            RunCode();
        }
        else if (CheckCollisionPointRec(mouse, CodeBtn(code, 1)))
        {
            SetMsg(CodeSave("circuit.txt") ? "Salvato in circuit.txt" : "Errore nel salvataggio");
        }
        else if (CheckCollisionPointRec(mouse, CodeBtn(code, 0)))
        {
            SetMsg(CodeLoad("circuit.txt") ? "Caricato circuit.txt" : "circuit.txt non trovato");
        }
        else if (CheckCollisionPointRec(mouse, ta))
        {
            int total = CountLines();
            int line = g_code_top + (int)((mouse.y - text_y) / lh);
            if (mouse.y < text_y) line = g_code_top;
            if (line < 0) line = 0;
            if (line >= total) line = total - 1;

            int ls = LineIndexStart(line);
            int le = LineEnd(ls);
            float rel = mouse.x - text_x;
            int col = le - ls;
            for (int j = 0; j <= le - ls; j++)
            {
                float w = TextW(g_code + ls, j);
                if (w >= rel)
                {
                    float wp = (j > 0) ? TextW(g_code + ls, j - 1) : 0.0f;
                    col = (rel - wp < w - rel) ? j - 1 : j;
                    if (col < 0) col = 0;
                    break;
                }
            }
            g_cursor = ls + col;
            moved = 1;
        }
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
        int step = (wheel > 0.0f) ? -3 : 3;
        if (CheckCollisionPointRec(mouse, ta))
        {
            g_code_top += step;
            int max_top = CountLines() - 1;
            if (g_code_top > max_top) g_code_top = max_top;
            if (g_code_top < 0) g_code_top = 0;
        }
        else if (CheckCollisionPointRec(mouse, out))
        {
            g_out_top += step;
            if (g_out_top < 0) g_out_top = 0;
        }
    }

    // ---- tastiera ----
    if (IsKeyPressed(KEY_F5) || (ctrl && IsKeyPressed(KEY_ENTER)))
    {
        RunCode();
    }
    else if (ctrl && IsKeyPressed(KEY_S))
    {
        SetMsg(CodeSave("circuit.txt") ? "Salvato in circuit.txt" : "Errore nel salvataggio");
    }

    int ch;
    while ((ch = GetCharPressed()) > 0)
    {
        if (!ctrl && ch >= 32 && ch <= 126)
        {
            char c = (char)ch;
            CodeInsert(&c, 1);
            moved = 1;
        }
    }

    if (!ctrl && KeyRep(KEY_ENTER))
    {
        int ls = LineStart(g_cursor);
        int sp = 0;
        while (ls + sp < g_cursor && g_code[ls + sp] == ' ') sp++; // mantiene l'indentazione
        char buf[64];
        int n = 0;
        buf[n++] = '\n';
        for (int k = 0; k < sp && n < 63; k++) buf[n++] = ' ';
        CodeInsert(buf, n);
        moved = 1;
    }
    if (KeyRep(KEY_TAB))       { CodeInsert("    ", 4); moved = 1; }
    if (KeyRep(KEY_BACKSPACE)) { CodeBackspace(); moved = 1; }
    if (KeyRep(KEY_DELETE))    { CodeDelete(); moved = 1; }

    if (KeyRep(KEY_LEFT))  { if (g_cursor > 0) g_cursor--; moved = 1; }
    if (KeyRep(KEY_RIGHT)) { if (g_cursor < g_code_len) g_cursor++; moved = 1; }
    if (KeyRep(KEY_HOME))  { g_cursor = LineStart(g_cursor); moved = 1; }
    if (KeyRep(KEY_END))   { g_cursor = LineEnd(g_cursor); moved = 1; }

    if (KeyRep(KEY_UP))
    {
        int ls = LineStart(g_cursor);
        int col = g_cursor - ls;
        if (ls > 0)
        {
            int pls = LineStart(ls - 1);
            int plen = (ls - 1) - pls;
            g_cursor = pls + (col < plen ? col : plen);
        }
        moved = 1;
    }
    if (KeyRep(KEY_DOWN))
    {
        int ls = LineStart(g_cursor);
        int col = g_cursor - ls;
        int le = LineEnd(g_cursor);
        if (le < g_code_len)
        {
            int nls = le + 1;
            int nlen = LineEnd(nls) - nls;
            g_cursor = nls + (col < nlen ? col : nlen);
        }
        moved = 1;
    }

    // tieni il cursore visibile (solo quando e' lui a muoversi, cosi' la rotella resta libera)
    if (moved)
    {
        int cl = CursorLine();
        if (cl < g_code_top) g_code_top = cl;
        if (cl >= g_code_top + visible) g_code_top = cl - visible + 1;
        if (g_code_top < 0) g_code_top = 0;
    }
}

// ---- disegno della tab ----

static int IsLangWord(const char *w)
{
    static const char *words[] = {"CIRCUIT", "WIRE", "AND", "OR", "NOT", "NOR", "NAND", "XOR",
                                  "UPDATE_W", "PRINT", "RUN", "NUMBER"};
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
        if (strcmp(w, words[i]) == 0) return 1;
    return 0;
}

static void DrawSeg(const char *line, int a, int b, float x0, float y, Color c)
{
    if (b <= a) return;
    char buf[512];
    int n = b - a;
    if (n > 511) n = 511;
    memcpy(buf, line + a, (size_t)n);
    buf[n] = '\0';
    Vector2 pos = {x0 + TextW(line, a), y};
    DrawTextEx(g_font, buf, pos, (float)g_font_size, 0, c);
}

// Una riga di codice con una colorazione minima: parole del linguaggio, var, numeri, stringhe.
static void DrawCodeLine(const char *line, int len, float x0, float y)
{
    int i = 0;
    while (i < len)
    {
        char c = line[i];
        if (c == '"')
        {
            int j = i + 1;
            while (j < len && line[j] != '"') j++;
            if (j < len) j++;
            DrawSeg(line, i, j, x0, y, RGB(150, 190, 130));
            i = j;
        }
        else if (isalpha((unsigned char)c) || c == '_')
        {
            int j = i;
            while (j < len && (isalnum((unsigned char)line[j]) || line[j] == '_')) j++;
            char word[64];
            int n = j - i;
            if (n > 63) n = 63;
            memcpy(word, line + i, (size_t)n);
            word[n] = '\0';

            Color col = COL_TEXT;
            if (strcmp(word, "var") == 0) col = RGB(170, 150, 220);
            else if (IsLangWord(word)) col = RGB(84, 170, 160);
            DrawSeg(line, i, j, x0, y, col);
            i = j;
        }
        else if (isdigit((unsigned char)c))
        {
            int j = i;
            while (j < len && isdigit((unsigned char)line[j])) j++;
            DrawSeg(line, i, j, x0, y, RGB(214, 167, 69));
            i = j;
        }
        else
        {
            if (c != ' ') DrawSeg(line, i, i + 1, x0, y, COL_TEXT_DIM);
            i++;
        }
    }
}

static void DrawPanelFrame(Rectangle r, const char *title)
{
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, COL_PANEL);
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 30, Mix(COL_PANEL, COL_BG, 0.35f));
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, COL_PANEL_LN);
    DrawText(title, (int)r.x + 12, (int)r.y + 8, 14, COL_TEXT_DIM);
}

static void DrawButton(Rectangle r, const char *label, Color col, int hovered)
{
    Color c = hovered ? Mix(col, WHITE, 0.15f) : col;
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, c);
    DrawText(label, (int)(r.x + (r.width - MeasureText(label, 12)) / 2), (int)r.y + 6, 12, COL_TEXT);
}

static void CodeDraw(int W, int H, Vector2 mouse)
{
    Rectangle code, out;
    CodeLayout(W, H, &code, &out);
    float lh = (float)(g_font_size + 4);

    // ================= pannello codice =================
    DrawPanelFrame(code, "CODICE  -  circuit.txt");

    DrawButton(CodeBtn(code, 2), "ESEGUI  F5", RGB(60, 130, 120), CheckCollisionPointRec(mouse, CodeBtn(code, 2)));
    DrawButton(CodeBtn(code, 1), "SALVA",      RGB(64, 80, 128),  CheckCollisionPointRec(mouse, CodeBtn(code, 1)));
    DrawButton(CodeBtn(code, 0), "CARICA",     RGB(64, 80, 128),  CheckCollisionPointRec(mouse, CodeBtn(code, 0)));

    Rectangle ta = {code.x + 1, code.y + 30, code.width - 2, code.height - 31};
    DrawRectangle((int)ta.x, (int)ta.y, (int)ta.width, (int)ta.height, Mix(COL_BG, BLACK, 0.25f));
    DrawRectangle((int)ta.x, (int)ta.y, 44, (int)ta.height, Mix(COL_PANEL, COL_BG, 0.5f));

    float text_x = ta.x + 52;
    float text_y = ta.y + 8;
    int visible = (int)((ta.height - 12) / lh) + 1;
    int total = CountLines();
    int cl = CursorLine();

    BeginScissorMode((int)ta.x, (int)ta.y, (int)ta.width, (int)ta.height);

    int idx = LineIndexStart(g_code_top);
    for (int l = g_code_top; l < total && l < g_code_top + visible; l++)
    {
        int le = LineEnd(idx);
        float y = text_y + (l - g_code_top) * lh;

        if (l == cl)
            DrawRectangle((int)(ta.x + 44), (int)y - 2, (int)(ta.width - 44), (int)lh, Fade(WHITE, 0.04f));

        char num[16];
        snprintf(num, sizeof(num), "%d", l + 1);
        Vector2 npos = {ta.x + 38 - TextW(num, (int)strlen(num)), y};
        DrawTextEx(g_font, num, npos, (float)g_font_size, 0, (l == cl) ? COL_TEXT : COL_TEXT_DIM);

        DrawCodeLine(g_code + idx, le - idx, text_x, y);
        idx = le + 1;
    }

    // cursore (lampeggia)
    if (((int)(GetTime() * 2.0)) % 2 == 0 && cl >= g_code_top && cl < g_code_top + visible)
    {
        int ls = LineStart(g_cursor);
        float cx = text_x + TextW(g_code + ls, g_cursor - ls);
        float cy = text_y + (cl - g_code_top) * lh;
        DrawRectangle((int)cx, (int)cy - 1, 2, g_font_size + 2, COL_TEXT);
    }

    EndScissorMode();

    if (GetTime() < g_msg_until)
    {
        DrawRectangle((int)ta.x + 44, (int)(ta.y + ta.height) - 26, (int)ta.width - 44, 26, Fade(BLACK, 0.55f));
        DrawText(g_msg, (int)ta.x + 56, (int)(ta.y + ta.height) - 20, 14, RGB(150, 190, 130));
    }

    // ================= pannello output =================
    DrawPanelFrame(out, "OUTPUT");

    const char *st = (g_out_status == 0) ? "IN ATTESA" : (g_out_status == 1 ? "OK" : "CRASH");
    Color stc = (g_out_status == 0) ? COL_PIN_IDLE : (g_out_status == 1 ? RGB(100, 160, 120) : RGB(190, 85, 85));
    Rectangle pill = {out.x + out.width - 96, out.y + 4, 84, 22};
    DrawRectangle((int)pill.x, (int)pill.y, (int)pill.width, (int)pill.height, stc);
    DrawText(st, (int)(pill.x + (pill.width - MeasureText(st, 12)) / 2), (int)pill.y + 5, 12, COL_TEXT);

    Rectangle ota = {out.x + 1, out.y + 30, out.width - 2, out.height - 31};
    DrawRectangle((int)ota.x, (int)ota.y, (int)ota.width, (int)ota.height, Mix(COL_BG, BLACK, 0.25f));

    BeginScissorMode((int)ota.x, (int)ota.y, (int)ota.width, (int)ota.height);

    if (g_out_status == 0)
    {
        DrawText("Premi F5 (o ESEGUI) per lanciare il codice.", (int)ota.x + 14, (int)ota.y + 12, 14, COL_TEXT_DIM);
    }
    else
    {
        // righe totali dell'output, per limitare lo scorrimento
        int out_total = 1;
        for (const char *p = g_out; *p; p++) if (*p == '\n') out_total++;
        if (g_out_top > out_total - 1) g_out_top = out_total - 1;
        if (g_out_top < 0) g_out_top = 0;

        const char *p = g_out;
        for (int skip = 0; skip < g_out_top && *p; )
        {
            if (*p == '\n') skip++;
            p++;
        }

        float y = ota.y + 8;
        while (*p && y < ota.y + ota.height)
        {
            const char *e = p;
            while (*e && *e != '\n') e++;

            char buf[512];
            int n = (int)(e - p);
            if (n > 511) n = 511;
            memcpy(buf, p, (size_t)n);
            buf[n] = '\0';

            Color c = COL_TEXT;
            if (strncmp(buf, ">>", 2) == 0) c = COL_WIRE_ON;
            else if (strncmp(buf, "[ERRORE]", 8) == 0) c = RGB(210, 110, 110);
            else if (strncmp(buf, "Cicle", 5) == 0 || strncmp(buf, "Tot tries", 9) == 0) c = COL_TEXT_DIM;

            Vector2 pos = {ota.x + 12, y};
            DrawTextEx(g_font, buf, pos, (float)g_font_size, 0, c);

            y += lh;
            p = (*e == '\n') ? e + 1 : e;
        }
    }

    EndScissorMode();
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1000, 800, "Circuit Lab");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Esc non chiude la finestra mentre scrivi (si chiude con la X)

    // font monospazio per il codice (se non esiste, raylib usa quello di default)
    g_font = LoadFontEx("C:/Windows/Fonts/consola.ttf", g_font_size, NULL, 0);
    SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);

    if (!CodeLoad("circuit.txt"))
    {
        const char *sample =
            "var c = CIRCUIT;\n"
            "var vcc = WIRE c \"vcc\" 1 0;\n"
            "var a = WIRE c \"a\" 1 0;\n"
            "var b = WIRE c \"b\" 1 0;\n"
            "var g = AND c vcc a b;\n"
            "PRINT g;\n"
            "RUN c;\n"
            "PRINT g;\n";
        strcpy(g_code, sample);
        g_code_len = (int)strlen(g_code);
    }

    // ------------------- editor grafico dei gate -------------------
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
    int tab = TAB_EDITOR;
    CGate *gate_clicked_first = NULL;

    while (!WindowShouldClose())
    {
        Vector2 mouse = GetMousePosition();
        int W = GetScreenWidth();
        int H = GetScreenHeight();

        // ------------------- cambio tab -------------------
        if (IsKeyPressed(KEY_F1)) tab = TAB_EDITOR;
        if (IsKeyPressed(KEY_F2)) tab = TAB_CODE;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (CheckCollisionPointRec(mouse, TabRect(0))) tab = TAB_EDITOR;
            else if (CheckCollisionPointRec(mouse, TabRect(1))) tab = TAB_CODE;
        }

        if (tab == TAB_CODE)
        {
            CodeUpdate(W, H, mouse);
        }
        else
        {
            // i caratteri digitati nella tab editor non devono finire nel codice
            while (GetCharPressed() > 0) {}

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
        }

        // ------------------- disegno -------------------
        BeginDrawing();
        ClearBackground(COL_BG);

        if (tab == TAB_CODE)
        {
            CodeDraw(W, H, mouse);
        }
        else
        {
            DrawBackgroundGrid(W, H);

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
        }

        DrawHud(tab, pin_a, W, H);

        EndDrawing();
    }

    UnloadFont(g_font);
    CloseWindow();
    return 0;
}