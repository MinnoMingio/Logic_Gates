#include <stdio.h>
#include <stdlib.h>

typedef struct Wire {
    char *name;
    int power;       // valore corrente, stabile
    int next_power;  // valore in calcolo per il prossimo giro
    int driven;       // 1 se e' l'output di un transistor (quindi va ricalcolato ogni giro)
} Wire;

typedef struct Transistor {
    char *name;
    int state;
    int invert;
    struct Wire **input_wire;
    struct Wire *output_wire;
    int count;
} Transistor;

typedef struct Circuit {
    Transistor **transistors;
    int t_count;
    Wire **wires;
    int w_count;
} Circuit;

/* ---------- costruzione wire / registro ---------- */

Wire *create_wire(char *name, int power)
{
    Wire *wire = malloc(sizeof(*wire));
    wire->name = name;
    wire->power = power;
    wire->next_power = power;
    wire->driven = 0;
    return wire;
}

void register_wire(Circuit *circuit, Wire *wire)
{
    circuit->wires = realloc(circuit->wires, sizeof(Wire*) * (circuit->w_count + 1));
    circuit->wires[circuit->w_count] = wire;
    circuit->w_count++;
}

/* ---------- transistor ---------- */

Transistor *create_transistor(char *name, int wires)
{
    Transistor *t = malloc(sizeof(*t));
    t->name = name;
    t->state = 0;
    t->invert = 0;
    t->count = wires;
    t->input_wire = wires > 0 ? malloc(sizeof(Wire*) * wires) : NULL;
    t->output_wire = NULL;
    return t;
}

void update_transistor(Transistor *t, int state, int invert)
{
    t->state = state;
    t->invert = invert;
}

void append_t_to_c(Circuit *circuit, Transistor *t)
{
    circuit->transistors = realloc(circuit->transistors, sizeof(Transistor*) * (circuit->t_count + 1));
    circuit->transistors[circuit->t_count] = t;
    circuit->t_count++;
}

/* ---------- il cuore: settling con doppio buffer ---------- */

/* un solo passaggio: legge SEMPRE i valori "power" (vecchi),
   scrive SEMPRE su "next_power" (nuovi). Non mischia mai le due cose. */
void step(Circuit *circuit)
{
    /* i wire "driven" partono da 0 ogni giro (altrimenti non potrebbero
       mai spegnersi); i wire non driven (ingressi esterni) mantengono
       il loro valore attuale */
    for (int i = 0; i < circuit->w_count; i++) {
        Wire *w = circuit->wires[i];
        w->next_power = w->driven ? 0 : w->power;
    }

    for (int i = 0; i < circuit->t_count; i++) {
        Transistor *t = circuit->transistors[i];
        int current_state = t->invert ? !t->state : t->state;

        for (int j = 0; j < t->count; j++) {
            int in_power = t->input_wire[j]->power; /* legge il vecchio valore */
            int current_power = t->invert ? !in_power : in_power;

            if (current_state == 1 && current_power == 1 && t->output_wire != NULL) {
                t->output_wire->next_power = 1; /* OR naturale se piu' transistor scrivono sullo stesso wire */
            }
        }
    }
}

/* applica next_power -> power per tutti i wire, ritorna 1 se qualcosa e' cambiato */
int commit(Circuit *circuit)
{
    int changed = 0;
    for (int i = 0; i < circuit->w_count; i++) {
        Wire *w = circuit->wires[i];
        if (w->power != w->next_power) changed = 1;
        w->power = w->next_power;
    }
    return changed;
}

/* esegue step+commit ripetutamente finche' il circuito si stabilizza
   (fixed point), o segnala oscillazione se non si stabilizza mai */
int run_circuit_settle(Circuit *circuit, int max_iterations)
{
    for (int iter = 1; iter <= max_iterations; iter++) {
        step(circuit);
        int changed = commit(circuit);
        if (!changed) {
            printf("  -> stabilizzato dopo %d iterazione/i\n", iter);
            return iter;
        }
    }
    printf("  -> ATTENZIONE: non stabilizzato dopo %d iterazioni (probabile oscillazione)\n", max_iterations);
    return -1;
}

/* ---------- gate helpers (come i tuoi, ma con register_wire) ---------- */

Transistor *make_buffer_or_not(Circuit *circuit, Wire *in, int invert, char *name)
{
    Transistor *t = create_transistor(name, 1);
    update_transistor(t, 1, invert);
    Wire *out = create_wire("w", 0);
    register_wire(circuit, out);
    out->driven = 1;
    t->input_wire[0] = in;
    t->output_wire = out;
    append_t_to_c(circuit, t);
    return t;
}

/* NOR a due ingressi: due transistor che scrivono (invertiti) sullo stesso wire di uscita.
   out = 1 solo se ENTRAMBI gli ingressi sono 0 */
Wire *make_nor(Circuit *circuit, Wire *a, Wire *b, char *name)
{
    Wire *out = create_wire(name, 0);
    register_wire(circuit, out);
    out->driven = 1;

    Transistor *t1 = create_transistor("nor_a", 1);
    update_transistor(t1, 1, 1); /* invert=1: passa se l'ingresso e' 0 */
    t1->input_wire[0] = a;
    t1->output_wire = out;
    append_t_to_c(circuit, t1);

    Transistor *t2 = create_transistor("nor_b", 1);
    update_transistor(t2, 1, 1);
    t2->input_wire[0] = b;
    t2->output_wire = out;
    append_t_to_c(circuit, t2);

    return out;
}

void print_wire(Wire *w)
{
    printf("%-6s = %d\n", w->name, w->power);
}

/* ---------- test: SR-latch con due NOR incrociati ---------- */
/* Q  = NOR(R, Qn)
   Qn = NOR(S, Q)
   Qn e' un input di se stesso -> Q, e Q e' un input di Qn -> feedback vero */

int main(void)
{
    Circuit circuit = {0};

    Wire *S = create_wire("S", 0);
    Wire *R = create_wire("R", 0);
    register_wire(&circuit, S);
    register_wire(&circuit, R);

    Wire *Q  = create_wire("Q", 0);
    Wire *Qn = create_wire("Qn", 0);
    register_wire(&circuit, Q);
    register_wire(&circuit, Qn);
    Q->driven = 1;
    Qn->driven = 1;

    /* NOR vero = OR (merge, non invertito) + NOT in serie.
       or_Q  = OR(R, Qn)   ->   Q  = NOT(or_Q)
       or_Qn = OR(S, Q)    ->   Qn = NOT(or_Qn) */
    Wire *or_Q = create_wire("or_Q", 0);
    Wire *or_Qn = create_wire("or_Qn", 0);
    register_wire(&circuit, or_Q);
    register_wire(&circuit, or_Qn);
    or_Q->driven = 1;
    or_Qn->driven = 1;

    Transistor *g1a = create_transistor("or_Q_R", 1); update_transistor(g1a, 1, 0);
    g1a->input_wire[0] = R; g1a->output_wire = or_Q; append_t_to_c(&circuit, g1a);

    Transistor *g1b = create_transistor("or_Q_Qn", 1); update_transistor(g1b, 1, 0);
    g1b->input_wire[0] = Qn; g1b->output_wire = or_Q; append_t_to_c(&circuit, g1b);

    Transistor *g2 = create_transistor("not_Q", 1); update_transistor(g2, 0, 1);
    g2->input_wire[0] = or_Q; g2->output_wire = Q; append_t_to_c(&circuit, g2);

    Transistor *g3a = create_transistor("or_Qn_S", 1); update_transistor(g3a, 1, 0);
    g3a->input_wire[0] = S; g3a->output_wire = or_Qn; append_t_to_c(&circuit, g3a);

    Transistor *g3b = create_transistor("or_Qn_Q", 1); update_transistor(g3b, 1, 0);
    g3b->input_wire[0] = Q; g3b->output_wire = or_Qn; append_t_to_c(&circuit, g3b);

    Transistor *g4 = create_transistor("not_Qn", 1); update_transistor(g4, 0, 1);
    g4->input_wire[0] = or_Qn; g4->output_wire = Qn; append_t_to_c(&circuit, g4);

    printf("stato iniziale (S=0, R=0, indeterminato prima del set/reset):\n");

    printf("\n1) SET (S=1, R=0):\n");
    S->power = 1; R->power = 0;
    run_circuit_settle(&circuit, 20);
    print_wire(Q); print_wire(Qn);

    printf("\n2) torniamo a riposo (S=0, R=0) -> deve MANTENERE lo stato (memoria):\n");
    S->power = 0; R->power = 0;
    run_circuit_settle(&circuit, 20);
    print_wire(Q); print_wire(Qn);

    printf("\n3) RESET (S=0, R=1):\n");
    S->power = 0; R->power = 1;
    run_circuit_settle(&circuit, 20);
    print_wire(Q); print_wire(Qn);

    printf("\n4) torniamo a riposo di nuovo (S=0, R=0) -> deve MANTENERE il nuovo stato:\n");
    S->power = 0; R->power = 0;
    run_circuit_settle(&circuit, 20);
    print_wire(Q); print_wire(Qn);

    return 0;
}