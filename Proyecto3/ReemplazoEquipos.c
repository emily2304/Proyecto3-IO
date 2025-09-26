/* Proyecto 3 IO - Reemplazo de Equipos
   Estudiantes:
   Emily Sánchez -
   Viviana Vargas -
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <math.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>

GtkWidget *windowReemplazo;
GtkWidget *costoEntry;
GtkWidget *plazoSpin;
GtkWidget *vidaSpin;
GtkWidget *scrollReemplazo;
GtkWidget *calcReemplazo;
GtkWidget *editLatexReemplazo;
GtkWidget *fileLoadReemplazo;
GtkWidget *loadToGridReemplazo;
GtkWidget *saveReemplazo;
GtkWidget *fileNameReemplazo;
GtkWidget *exitReemplazo;
GtkWidget *profitCheck;
GtkCssProvider *cssProvider;

// --- Variables globales ---
double **dp_equipo;
int **decision_equipo;
gboolean profitSelected = FALSE;

GPtrArray *entryResale = NULL;
GPtrArray *entryMaint  = NULL;
GPtrArray *entryProfit   = NULL;
static gchar *selected_csv_path = NULL;
static gchar *last_selected_tex_reemplazo = NULL;

// Struct para la info
typedef struct {
    int     n; //Plazo
    double  W;
    double *costo;         
    double *valor_residual;
    double *costo_mant;
    double *beneficio;
} Equipment;

typedef struct {
    double costo_minimo;
    int proximo_reemplazo;
    GPtrArray *planes; // Array de posibles planes óptimos
} SolucionReemplazo;

void set_css(GtkCssProvider *cssProvider, GtkWidget *widget);

//Función para que se utilice el archivo .css como proveedor de estilos.
void set_css(GtkCssProvider *cssProvider, GtkWidget *widget) {
    GtkStyleContext *styleContext = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(styleContext, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

// --- HELPERS ---

void validate_entry(GtkEditable *editable, const gchar *text, gint length, gint *position, gpointer user_data) {
    gchar *filtered = g_new(gchar, length + 1);
    int j = 0;
    for (int i = 0; i < length; i++) {
        gunichar ch = g_utf8_get_char(text + i);
        if (g_unichar_isdigit(ch) || ch == '.' || ch == ',') {
            filtered[j++] = text[i];
        }
    }
    filtered[j] = '\0';

    if (j == 0) {
        g_signal_stop_emission_by_name(editable, "insert-text");
        g_free(filtered);
        return;
    }

    g_signal_handlers_block_by_func(editable, G_CALLBACK(validate_entry), user_data);
    gtk_editable_insert_text(editable, filtered, j, position);
    g_signal_handlers_unblock_by_func(editable, G_CALLBACK(validate_entry), user_data);

    g_signal_stop_emission_by_name(editable, "insert-text");
    g_free(filtered);
}

static gchar* set_extension(const gchar *name) {
    if (!name || !*name) {
        return g_strdup("equipment_replacement.csv");
    }
    if (g_str_has_suffix(name, ".csv")) {
        return g_strdup(name);
    }
    return g_strconcat(name, ".csv", NULL);
}

static gboolean is_profit_enabled(void){
    return profitCheck && GTK_IS_TOGGLE_BUTTON(profitCheck)? gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(profitCheck)): FALSE;
}

static void set_real(char *s) {
    if (!s) {
        return;
    }
    for (char *p = s; *p; ++p) {
        if (*p == ',') *p = '.';
    }
}

static char *trim(char *s) {
    if (!s) {
        return s;
    }
    while (g_ascii_isspace(*s)) {
        s++;
    }
    if (*s == 0) {
        return s;
    }
    char *end = s + strlen(s) - 1;
    while (end > s && g_ascii_isspace(*end)) {
        *end-- = 0;
    }
    return s;
}

static gchar* set_path_csv(void) {

    gchar *dir = g_build_filename(g_get_current_dir(), "Saved_Equipment", NULL);
    g_mkdir_with_parents(dir, 0755);

    const gchar *name = fileNameReemplazo && GTK_IS_ENTRY(fileNameReemplazo)
                        ? gtk_entry_get_text(GTK_ENTRY(fileNameReemplazo))
                        : "";

    gchar *basename = NULL;
    if (name && *name) {
        basename = g_str_has_suffix(name, ".csv") ? g_strdup(name)
                                                  : g_strconcat(name, ".csv", NULL);
    } else {
        GDateTime *now = g_date_time_new_now_local();
        gchar *ts = g_date_time_format(now, "%Y%m%d_%H%M%S");
        basename = g_strconcat("Reemplazo_", ts, ".csv", NULL);
        g_date_time_unref(now);
        g_free(ts);
    }

    gchar *path = g_build_filename(dir, basename, NULL);
    g_free(dir);
    g_free(basename);
    return path;
}

static void free_equipment(Equipment *e) {
    if (!e) {
        return;
    }

    g_free(e->costo);
    g_free(e->valor_residual);
    g_free(e->costo_mant);
    g_free(e->beneficio);
    memset(e, 0, sizeof(*e));

}

static void free_dp_and_decisions(int n, int W) {
    if (dp_equipo) {
        for (int i = 0; i <= n; i++) {
            free(dp_equipo[i]);
        }
        free(dp_equipo);
        dp_equipo = NULL;
    }
    if (decision_equipo) {
        for (int i = 0; i <= n; i++) {
            free(decision_equipo[i]);
        }
        free(decision_equipo);
        decision_equipo = NULL;
    }
}

static double read_user_entry(GtkWidget *entry, double fallback) {
    if (!entry || !GTK_IS_ENTRY(entry)) {
        return fallback;
    }

    const gchar *txt = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!txt || !*txt) {
        return fallback;
    }

    char *endptr = NULL;
    double v = g_strtod(txt, &endptr);

    if (endptr == txt) {
        return fallback;
    }

    return v;
}


static void clean_arrays() {
    if (entryResale) { 
        g_ptr_array_free(entryResale, TRUE); 
        entryResale = NULL; }
    if (entryMaint)  { 
        g_ptr_array_free(entryMaint,  TRUE); 
        entryMaint  = NULL; }
    if (entryProfit)   { 
        g_ptr_array_free(entryProfit,   TRUE); 
        entryProfit   = NULL; }
}

static void clear_grid(GtkWidget *container) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(container));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_container_remove(GTK_CONTAINER(container), GTK_WIDGET(l->data));
    }
    g_list_free(children);
}


void build_table(int plazo, gboolean profit) {
    if (!GTK_IS_SCROLLED_WINDOW(scrollReemplazo)) {
        return;
    }

    if (plazo < 1)  {
        plazo = 1;
    }
    if (plazo > 30) {
        plazo = 30;
    }

    clear_grid(scrollReemplazo);
    clean_arrays();

    entryResale = g_ptr_array_sized_new(plazo);
    entryMaint  = g_ptr_array_sized_new(plazo);
    if (profit) {
        entryProfit = g_ptr_array_sized_new(plazo);
    }

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);


    int col = 0;
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Time (T)"),            col++, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Resale"),      col++, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Maintenance"),col++, 0, 1, 1);
    if (profit) {
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Profit"), col++, 0, 1, 1);
    }

    for (int i = 0; i < plazo; i++) {
        int c = 0;

        // Columna Term
        gchar tbuf[8];
        g_snprintf(tbuf, sizeof(tbuf), "%d", i + 1);
        GtkWidget *lbl_t = gtk_label_new(tbuf);
        gtk_widget_set_halign(lbl_t, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), lbl_t, c++, i + 1, 1, 1);

        // Columna Resale
        GtkWidget *e_res = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(e_res), 10);
        gtk_entry_set_alignment(GTK_ENTRY(e_res), 1.0);
        gtk_entry_set_placeholder_text(GTK_ENTRY(e_res), "0.00");
        g_signal_connect(e_res, "insert-text", G_CALLBACK(validate_entry), NULL);
        gtk_grid_attach(GTK_GRID(grid), e_res, c++, i + 1, 1, 1);
        g_ptr_array_add(entryResale, e_res);

        // Columna Maintenance
        GtkWidget *e_maint = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(e_maint), 10);
        gtk_entry_set_alignment(GTK_ENTRY(e_maint), 1.0);
        gtk_entry_set_placeholder_text(GTK_ENTRY(e_maint), "0.00");
        g_signal_connect(e_maint, "insert-text", G_CALLBACK(validate_entry), NULL);
        gtk_grid_attach(GTK_GRID(grid), e_maint, c++, i + 1, 1, 1);
        g_ptr_array_add(entryMaint, e_maint);

        // Columna Profit
        if (profit) {
            GtkWidget *e_gain = gtk_entry_new();
            gtk_entry_set_width_chars(GTK_ENTRY(e_gain), 10);
            gtk_entry_set_alignment(GTK_ENTRY(e_gain), 1.0);
            gtk_entry_set_placeholder_text(GTK_ENTRY(e_gain), "0.00");
            g_signal_connect(e_gain, "insert-text", G_CALLBACK(validate_entry), NULL);
            gtk_grid_attach(GTK_GRID(grid), e_gain, c++, i + 1, 1, 1);
            g_ptr_array_add(entryProfit, e_gain);
        }
    }

    gtk_container_add(GTK_CONTAINER(scrollReemplazo), grid);
    gtk_widget_show_all(grid);
}

static gboolean read_table(Equipment *out) {
    if (!out) {
        return FALSE;
    }
    memset(out, 0, sizeof(*out));

    if (!plazoSpin || !costoEntry) {
        g_printerr("Missing Data: Term and/or lifespan.\n");
        return FALSE;
    }

    int plazo = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(plazoSpin));

    if (plazo < 1) {
        plazo = 1;
    }
    if (plazo > 30) {
        plazo = 30;
    }
    out->n = plazo;

    double costo_inicial = read_user_entry(costoEntry, 0.0);

    out->W = (double)plazo * costo_inicial;
    if (out->W <= 0.0) out->W = (double)plazo * (costo_inicial > 0 ? costo_inicial : 1.0);

    out->costo          = g_new0(double, plazo);
    out->valor_residual = g_new0(double, plazo);
    out->costo_mant     = g_new0(double, plazo);
    out->beneficio      = g_new0(double, plazo);

    if (!entryResale || !entryMaint) {
        g_printerr("No table created yet.\n");
        return FALSE;
    }

    for (int i = 0; i < plazo; i++) {
        GtkWidget *e_res   = GTK_WIDGET(g_ptr_array_index(entryResale, i));
        GtkWidget *e_maint = GTK_WIDGET(g_ptr_array_index(entryMaint,  i));

        out->costo[i]          = costo_inicial;
        out->valor_residual[i] = read_user_entry(e_res,   0.0); 
        out->costo_mant[i]     = read_user_entry(e_maint, 0.0); 

        if (entryProfit && i < (int)entryProfit->len) {
            GtkWidget *e_gain = GTK_WIDGET(g_ptr_array_index(entryProfit, i));
            out->beneficio[i] = read_user_entry(e_gain, 0.0);
        } else {
            out->beneficio[i] = 0.0; 
        }
    }

    return TRUE;
}

static gboolean save_to_csv(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }

    int plazo = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(plazoSpin));
    gboolean useProfit = FALSE;
    if (profitCheck && GTK_IS_TOGGLE_BUTTON(profitCheck)) {
        useProfit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(profitCheck));
    }

    if (!entryResale || !entryMaint || (useProfit && !entryProfit)) {
        g_printerr("No table to save. Try again.\n");
        return FALSE;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        g_printerr("Could not open file: %s\n", path);
        return FALSE;
    }

    double costo_inicial = read_user_entry(costoEntry, 0.0);
    int vida_util = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vidaSpin));
    fprintf(f, "#META,%.6f,%d,%d,%d\n", costo_inicial, vida_util, plazo, useProfit ? 1 : 0);

    if (useProfit) {
        fprintf(f, "t,Resale,Maintenance,Profit\n");
    } else {
        fprintf(f, "t,Resale,Maintenance\n");
    }

    for (int i = 0; i < plazo; i++) {
        GtkWidget *e_res   = GTK_WIDGET(g_ptr_array_index(entryResale, i));
        GtkWidget *e_maint = GTK_WIDGET(g_ptr_array_index(entryMaint,  i));
        const gchar *s_res   = gtk_entry_get_text(GTK_ENTRY(e_res));
        const gchar *s_maint = gtk_entry_get_text(GTK_ENTRY(e_maint));

        if (useProfit) {
            GtkWidget *e_prof = GTK_WIDGET(g_ptr_array_index(entryProfit, i));
            const gchar *s_prof = gtk_entry_get_text(GTK_ENTRY(e_prof));
            fprintf(f, "%d,%s,%s,%s\n", i + 1,
                    s_res ? s_res : "", s_maint ? s_maint : "", s_prof ? s_prof : "");
        } else {
            fprintf(f, "%d,%s,%s\n", i + 1,
                    s_res ? s_res : "", s_maint ? s_maint : "");
        }
    }

    fclose(f);
    return TRUE;
}

static gboolean load_from_csv(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }

    gchar *content = NULL;
    gsize len = 0;
    if (!g_file_get_contents(path, &content, &len, NULL)) {
        g_printerr("Could not read file %s\n", path);
        return FALSE;
    }

    gchar **lines = g_strsplit(content, "\n", -1);

    int start = 0;
    int cols_detected = 0;
    int rows = 0;
    gboolean meta_seen = FALSE;

    for (int i = 0; lines[i]; ++i) {
        char *line = trim(lines[i]);
        if (!line || !*line) { continue; }

        if (g_ascii_strncasecmp(line, "#META", 5) == 0) {

            gchar **tokm = g_strsplit(line, ",", -1);

            if (tokm[1] && costoEntry && GTK_IS_ENTRY(costoEntry)) {
                gtk_entry_set_text(GTK_ENTRY(costoEntry), trim(tokm[1]));
            }
            if (tokm[2] && vidaSpin && GTK_IS_SPIN_BUTTON(vidaSpin)) {
                int vida = atoi(trim(tokm[2]));
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(vidaSpin), vida);
            }
            if (tokm[3] && plazoSpin && GTK_IS_SPIN_BUTTON(plazoSpin)) {
                int plazo = atoi(trim(tokm[3]));
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(plazoSpin), plazo);
            }
            if (tokm[4] && profitCheck && GTK_IS_TOGGLE_BUTTON(profitCheck)) {
                int p = atoi(trim(tokm[4]));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(profitCheck), p ? TRUE : FALSE);
            }
            g_strfreev(tokm);
            meta_seen = TRUE;
            start = i + 1; 
            break;
        } else {
            start = i;
            break;
        }
    }

    for (int i = start; lines[i]; ++i) {
        char *raw = lines[i];
        char *line = trim(raw);
        if (!line || !*line) continue;

        int cols = 1;
        for (char *p = line; *p; ++p) if (*p == ',') cols++;
        cols_detected = cols;

        if (g_ascii_strncasecmp(line, "t,", 2) == 0 || g_ascii_strncasecmp(line, "time", 4) == 0) {
            start = i + 1;
        }
        break;
    }

    if (cols_detected != 3 && cols_detected != 4) {
        g_strfreev(lines);
        g_free(content);
        GtkWidget *dlg = gtk_message_dialog_new(
            GTK_WINDOW(windowReemplazo), GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "Se esperaban 3 o 4 columnas. Detectadas: %d.", cols_detected
        );
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return FALSE;
    }

    gboolean use_profit = (cols_detected == 4);

    for (int i = start; lines[i]; ++i) {
        char *line = trim(lines[i]);
        if (line && *line) rows++;
    }
    if (rows <= 0) {
        g_strfreev(lines);
        g_free(content);
        return FALSE;
    }

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(plazoSpin), rows);
    build_table(rows, use_profit);

    if (profitCheck && GTK_IS_TOGGLE_BUTTON(profitCheck)) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(profitCheck), use_profit);
    }

    int r = 0;
    for (int i = start; lines[i]; ++i) {
        char *raw = lines[i];
        char *line = trim(raw);
        if (!line || !*line) continue;

        gchar **tok = g_strsplit(line, ",", -1);
        char *s_res   = tok[1] ? trim(tok[1]) : (char*)"";
        char *s_maint = tok[2] ? trim(tok[2]) : (char*)"";
        char *s_prof  = (use_profit && tok[3]) ? trim(tok[3]) : NULL;

        GtkWidget *e_res   = GTK_WIDGET(g_ptr_array_index(entryResale, r));
        GtkWidget *e_maint = GTK_WIDGET(g_ptr_array_index(entryMaint,  r));
        gtk_entry_set_text(GTK_ENTRY(e_res),   s_res);
        gtk_entry_set_text(GTK_ENTRY(e_maint), s_maint);

        if (use_profit && entryProfit) {
            GtkWidget *e_prof = GTK_WIDGET(g_ptr_array_index(entryProfit, r));
            gtk_entry_set_text(GTK_ENTRY(e_prof), s_prof ? s_prof : "");
        }

        g_strfreev(tok);
        r++;
        if (r >= rows) break;
    }

    g_strfreev(lines);
    g_free(content);
    return TRUE;
}

double calcular_costo_periodo_correcto(Equipment *e, int inicio, int fin, int vida_util_equipo) {
    if (fin <= inicio) return 0.0;
    int duracion = fin - inicio; 
    if (duracion > vida_util_equipo) {
        return 1e20; 
    }
    double costo_total = e->costo[0];
    for (int i = 0; i < duracion; i++) {
        if (i < e->n) { 
            costo_total += e->costo_mant[i];
        }
    }
    if (duracion > 0 && duracion <= e->n) {
        costo_total -= e->valor_residual[duracion - 1];
    }
    return costo_total;
}

// Función del algoritmo
SolucionReemplazo* equipo_replacement_algorithm_corregido(Equipment *e, int vida_util_equipo) {
    int plazo_proyecto = e->n; 
    SolucionReemplazo *sol = g_new0(SolucionReemplazo, 1);
    sol->planes = g_ptr_array_new_with_free_func(g_free);
    
    g_print("Plazo del proyecto (vidaSpin): %d años\n", plazo_proyecto);
    g_print("Vida útil del equipo (plazoSpin): %d años\n", vida_util_equipo);
    g_print("Costo inicial: $%.2f\n", e->costo[0]);
    double *g = g_new0(double, plazo_proyecto + 1); 
    GPtrArray **planes_por_tiempo = g_new0(GPtrArray*, plazo_proyecto + 1); 
    
    for (int i = 0; i <= plazo_proyecto; i++) {
        planes_por_tiempo[i] = g_ptr_array_new_with_free_func(g_free);
    }
    g[plazo_proyecto] = 0.0;
    g_ptr_array_add(planes_por_tiempo[plazo_proyecto], g_strdup(""));
    
    for (int t = plazo_proyecto - 1; t >= 0; t--) {
        g[t] = 1e20; 
        GPtrArray *mejores_j = g_ptr_array_new();
        for (int j = t + 1; j <= plazo_proyecto; j++) {
            int duracion = j - t;
            if (duracion > vida_util_equipo) continue;
            double costo_actual = calcular_costo_periodo_correcto(e, t, j, vida_util_equipo);
            if (costo_actual >= 1e20) continue;
            double costo_total = costo_actual + g[j];
            g_print("g(%d): C_%d,%d + g(%d) = %.2f + %.2f = %.2f\n", t, t, j, j, costo_actual, g[j], costo_total);
            if (costo_total < g[t] - 1e-9) {
                g[t] = costo_total;
                g_ptr_array_set_size(mejores_j, 0);
                g_ptr_array_add(mejores_j, GINT_TO_POINTER(j));
            } 
            else if (fabs(costo_total - g[t]) < 1e-9) {
                g_ptr_array_add(mejores_j, GINT_TO_POINTER(j));
            }
        }
        for (guint i = 0; i < mejores_j->len; i++) {
            int j = GPOINTER_TO_INT(g_ptr_array_index(mejores_j, i));
            
            if (planes_por_tiempo[j]->len > 0) {
                for (guint k = 0; k < planes_por_tiempo[j]->len; k++) {
                    char *plan_existente = (char*)g_ptr_array_index(planes_por_tiempo[j], k);
                    char *nuevo_plan;
                    if (strlen(plan_existente) > 0) {
                        nuevo_plan = g_strdup_printf("%d-%s", j, plan_existente);
                    } else {
                        nuevo_plan = g_strdup_printf("%d", j);
                    }
                    g_ptr_array_add(planes_por_tiempo[t], nuevo_plan);
                }
            } else {
                char *plan_base = g_strdup_printf("%d", j);
                g_ptr_array_add(planes_por_tiempo[t], plan_base);
            }
        }
        
        g_ptr_array_free(mejores_j, TRUE);
        g_print("g(%d) = %.2f\n", t, g[t]);
    }
    
    sol->costo_minimo = g[0];
    for (guint i = 0; i < planes_por_tiempo[0]->len; i++) {
        char *plan = (char*)g_ptr_array_index(planes_por_tiempo[0], i);
        char *plan_completo = g_strdup_printf("0-%s", plan);
        g_ptr_array_add(sol->planes, plan_completo);
    }
    // Liberar memoria
    g_free(g);
    for (int i = 0; i <= plazo_proyecto; i++) {
        g_ptr_array_free(planes_por_tiempo[i], TRUE);
    }
    g_free(planes_por_tiempo);
    
    return sol;
}

// Función para crear un grafo de saltos de rana
gboolean generar_grafo_saltos_rana_completo(const char *plan_optimo, const char *filename, int año_maximo) {
    if (!plan_optimo || !filename) return FALSE;    
    
    gchar **saltos = g_strsplit(plan_optimo, "-", -1);
    if (!saltos || !saltos[0]) {
        g_strfreev(saltos);
        return FALSE;
    }
    
    int num_nodos_plan = 0;
    while (saltos[num_nodos_plan] != NULL) num_nodos_plan++;
    
    if (num_nodos_plan < 2) {
        g_strfreev(saltos);
        return FALSE;
    }
    
    gchar *dot_filename = g_strconcat(filename, ".dot", NULL);
    FILE *dot_file = fopen(dot_filename, "w");
    if (!dot_file) {
        g_free(dot_filename);
        g_strfreev(saltos);
        return FALSE;
    }
    fprintf(dot_file, "digraph PlanOptimo {\n");
    fprintf(dot_file, "    rankdir=LR;\n");
    fprintf(dot_file, "    node [shape=circle, style=filled, fillcolor=lightblue, fontname=Arial];\n");
    fprintf(dot_file, "    edge [color=darkgreen, arrowhead=vee, arrowsize=0.8];\n\n");
    for (int i = 0; i <= año_maximo; i++) {
        gchar nodo_str[10];
        g_snprintf(nodo_str, sizeof(nodo_str), "%d", i);
        gboolean en_plan = FALSE;
        int posicion_en_plan = -1;
        
        for (int j = 0; j < num_nodos_plan; j++) {
            if (atoi(saltos[j]) == i) {
                en_plan = TRUE;
                posicion_en_plan = j;
                break;
            }
        }
        
        if (en_plan) {
            if (posicion_en_plan == 0) {
                fprintf(dot_file, "    \"%d\" [fillcolor=green, fontcolor=white];\n", i);
            } else if (posicion_en_plan == num_nodos_plan - 1) {
                fprintf(dot_file, "    \"%d\" [fillcolor=red, fontcolor=white];\n", i);
            } else {
                fprintf(dot_file, "    \"%d\" [fillcolor=orange];\n", i);
            }
        } else {
            fprintf(dot_file, "    \"%d\" [fillcolor=lightgray, color=gray, fontcolor=black];\n", i);
        }
    }
    
    fprintf(dot_file, "\n");
    
    for (int i = 0; i < num_nodos_plan - 1; i++) {
        int inicio = atoi(saltos[i]);
        int fin = atoi(saltos[i+1]);
        int duracion = fin - inicio;
        
        fprintf(dot_file, "    \"%d\" -> \"%d\" [label=\"%d año%s\", fontsize=10, color=darkgreen, penwidth=2.0];\n", 
                inicio, fin, duracion, duracion > 1 ? "s" : "");
    }
    
    for (int i = 0; i < año_maximo; i++) {
        gboolean conexion_existe = FALSE;
        
        for (int j = 0; j < num_nodos_plan - 1; j++) {
            int inicio_plan = atoi(saltos[j]);
            int fin_plan = atoi(saltos[j+1]);
            
            if (inicio_plan == i && fin_plan == i + 1) {
                conexion_existe = TRUE;
                break;
            }
        }
        
        if (!conexion_existe) {
            fprintf(dot_file, "    \"%d\" -> \"%d\" [style=dotted, color=gray, arrowhead=empty, arrowsize=0.5, constraint=false];\n", i, i + 1);
        }
    }
    
    fprintf(dot_file, "}\n");
    fclose(dot_file);
    
    // Generar imágenes PNG y PDF
    gchar *cmd = g_strdup_printf("dot -Tpng -o\"%s.png\" \"%s\"", filename, dot_filename);
    int result = system(cmd);
    gchar *cmd_pdf = g_strdup_printf("dot -Tpdf -o\"%s.pdf\" \"%s\"", filename, dot_filename);
    int result_pdf = system(cmd_pdf);
    
    // Limpiar
    g_free(cmd);
    g_free(cmd_pdf);
    g_free(dot_filename);
    g_strfreev(saltos);
    
    return (result == 0 && result_pdf == 0);
}
// --- LATEX
void compile_latex_file(const gchar *tex_file) {
    gchar *dir = g_path_get_dirname(tex_file);
    gchar *base = g_path_get_basename(tex_file);
    
    gchar *cmd = g_strdup_printf("cd \"%s\" && pdflatex -interaction=nonstopmode \"%s\"", dir, base);
    int result = system(cmd);
    
    if (result == 0) {
        gchar *pdf_file;
        if (g_str_has_suffix(base, ".tex")) {
            gchar *base_name = g_strndup(base, strlen(base) - 4);
            pdf_file = g_strdup_printf("%s/%s.pdf", dir, base_name);
            g_free(base_name);
        } else {
            pdf_file = g_strdup_printf("%s/%s.pdf", dir, base);
        }
        
        if (g_file_test(pdf_file, G_FILE_TEST_EXISTS)) {
            gchar *view_cmd = g_strdup_printf("evince --presentation \"%s\" &", pdf_file);
            system(view_cmd);
            g_free(view_cmd);
        }
        g_free(pdf_file);
    }
    
    g_free(cmd);
    g_free(dir);
    g_free(base);
}

void on_select_latex_file_reemplazo(GtkButton *button, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;
    dialog = gtk_file_chooser_dialog_new("Select LaTeX File", GTK_WINDOW(windowReemplazo), action, "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "LaTeX Files (*.tex)");
    gtk_file_filter_add_pattern(filter, "*.tex");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    gchar *reports_dir = g_build_filename(g_get_current_dir(), "ReportsEquipment", NULL);
    if (g_file_test(reports_dir, G_FILE_TEST_IS_DIR)) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), reports_dir);
    }
    g_free(reports_dir);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        if (last_selected_tex_reemplazo) {
            g_free(last_selected_tex_reemplazo);
        }
        last_selected_tex_reemplazo = gtk_file_chooser_get_filename(chooser);
        
        GtkWidget *choice_dialog = gtk_dialog_new_with_buttons(
            "What do you want to do?",
            GTK_WINDOW(windowReemplazo),
            GTK_DIALOG_MODAL,
            "Edit",
            GTK_RESPONSE_YES,
            "Compile",
            GTK_RESPONSE_NO,
            "Both",
            GTK_RESPONSE_APPLY,
            NULL
        );
        
        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(choice_dialog));
        GtkWidget *label = gtk_label_new("Select an option for the LaTeX file:");
        gtk_container_add(GTK_CONTAINER(content_area), label);
        gtk_widget_show_all(choice_dialog);
        
        gint choice = gtk_dialog_run(GTK_DIALOG(choice_dialog));
        gtk_widget_destroy(choice_dialog);
        
        if (choice == GTK_RESPONSE_YES) {
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex_reemplazo);
            system(edit_cmd);
            g_free(edit_cmd);
        } else if (choice == GTK_RESPONSE_NO) {
            compile_latex_file(last_selected_tex_reemplazo);
        } else if (choice == GTK_RESPONSE_APPLY) {
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex_reemplazo);
            system(edit_cmd);
            g_free(edit_cmd);
            
            GtkWidget *compile_dialog = gtk_dialog_new_with_buttons(
                "Recompile PDF",
                GTK_WINDOW(windowReemplazo),
                GTK_DIALOG_MODAL,
                "Compile now",
                GTK_RESPONSE_YES,
                "Later",
                GTK_RESPONSE_NO,
                NULL
            );
            
            GtkWidget *compile_content = gtk_dialog_get_content_area(GTK_DIALOG(compile_dialog));
            GtkWidget *compile_label = gtk_label_new("Do you want to compile the PDF now?");
            gtk_container_add(GTK_CONTAINER(compile_content), compile_label);
            gtk_widget_show_all(compile_dialog);
            
            gint compile_response = gtk_dialog_run(GTK_DIALOG(compile_dialog));
            gtk_widget_destroy(compile_dialog);
            
            if (compile_response == GTK_RESPONSE_YES) {
                compile_latex_file(last_selected_tex_reemplazo);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

// --- Generación de LaTeX ---
void generar_reporte_latex_corregido(Equipment *e, int vida_util, SolucionReemplazo *sol, const char *filename) {
    gchar *dir = g_build_filename(g_get_current_dir(), "ReportsEquipment", NULL);
    g_mkdir_with_parents(dir, 0755);
    
    gchar *tex_path = g_build_filename(dir, filename, NULL);
    
    FILE *f = fopen(tex_path, "w");
    if (!f) {
        g_printerr("No se pudo crear el archivo LaTeX: %s\n", tex_path);
        g_free(dir);
        g_free(tex_path);
        return;
    }
    
    int vida_util_ajustada = (vida_util > e->n) ? e->n : vida_util;
    
    // Encabezado del documento LaTeX
    fprintf(f, "\\documentclass[12pt]{article}\n");
    fprintf(f, "\\usepackage[spanish]{babel}\n");
    fprintf(f, "\\usepackage[utf8]{inputenc}\n");
    fprintf(f, "\\usepackage{amsmath}\n");
    fprintf(f, "\\usepackage{graphicx}\n");
    fprintf(f, "\\usepackage{booktabs}\n");
    fprintf(f, "\\usepackage{array}\n");
    fprintf(f, "\\usepackage{multirow}\n");
    fprintf(f, "\\usepackage{float}\n");
    fprintf(f, "\\usepackage{longtable}\n");
    fprintf(f, "\\usepackage{subcaption}\n");
    fprintf(f, "\\usepackage{wrapfig}\n");
    fprintf(f, "\\usepackage{tikz}\n");
    fprintf(f, "\\usetikzlibrary{arrows.meta, positioning, shapes.geometric}\n");
    fprintf(f, "\\title{Proyecto 3: Reemplazo de Equipos}\n");
    fprintf(f, "\\author{Emily Sanchez \\\\ Viviana Vargas \\\\[1cm] Curso: Investigación de Operaciones \\\\ II Semestre 2025}\n");
    fprintf(f, "\\date{\\today}\n\n");
    
    fprintf(f, "\\begin{document}\n\n");
    
    // Portada
    fprintf(f, "\\maketitle\n");
    fprintf(f, "\\newpage\n");
    
    // Sección 1: Descripción del problema
    fprintf(f, "\\section*{Problema de Reemplazo de Equipos}\n");
    fprintf(f, "El algoritmo de reemplazo de equipos se utiliza en Investigación de Operaciones para decidir cuándo conviene reemplazar una máquina o equipo que se deteriora con el tiempo.\\\\\n");
    fprintf(f, "La idea básica es comparar dos tipos de costos:\\\\\n");
    fprintf(f, "\\begin{itemize}\n");
    fprintf(f, "\\item \\textbf{Costo de mantener el equipo actual:} Incluye reparaciones, mantenimiento y costos de operación, que normalmente aumentan con los años de uso.\\\\\n");
    fprintf(f, "\\item \\textbf{Costo de reemplazarlo por uno nuevo:} Incluye el costo inicial de adquisición y el valor de rescate (lo obtenido al vender el equipo viejo).\\\\\n");
    fprintf(f, "\\end{itemize}\n");
    fprintf(f, "El objetivo es minimizar el costo promedio anual (o el valor presente de los costos) a lo largo del tiempo.\\\\\n");
    fprintf(f, "\\textbf{Variaciones comunes del problema:}\\\\\n");
    fprintf(f, "\\begin{itemize}\n");
    fprintf(f, "\\item \\textbf{Ganancias por año:} La productividad del equipo disminuye con la edad, afectando los ingresos.\\\\\n");
    fprintf(f, "\\item \\textbf{Inflación:} Los precios de adquisición y mantenimiento cambian según el año.\\\\\n");
    fprintf(f, "\\item \\textbf{Nuevas tecnologías:} Equipos más modernos pueden ofrecer mejores rendimientos y menores costos operativos.\\\\\n");
    fprintf(f, "\\end{itemize}\n");
    fprintf(f, "\\textbf{Fórmula del costo:} $C_{t,j} = \\text{Compra} + \\sum_{k=1}^{j-t} \\text{Mantenimiento}_k - \\text{Venta}_{j-t}$\\\\\n");
    fprintf(f, "\\textbf{Algoritmo:} Programación Dinámica \\\\\n");
    fprintf(f, "\\textbf{Función recursiva:} $g(t) = \\min\\limits_{j=t+1}^{\\min(t+\\text{vida útil}, n)} \\{C_{t,j} + g(j)\\}$ con $g(n) = 0$\\\\\n\n");
    
    // Sección 2: Datos del problema
    fprintf(f, "\\section*{Datos del Problema}\n");
    fprintf(f, "\\begin{itemize}\n");
    fprintf(f, "\\item Costo inicial (compra): \\$%.2f\n", e->costo[0]);
    fprintf(f, "\\item Plazo del proyecto: %d años\n", e->n);
    fprintf(f, "\\item Vida útil del equipo: %d años\n", vida_util);
    fprintf(f, "\\end{itemize}\n\n");
    
    fprintf(f, "\\begin{table}[H]\n");
    fprintf(f, "\\centering\n");
    fprintf(f, "\\caption{Datos del equipo por año de uso}\n");
    fprintf(f, "\\begin{tabular}{ccc}\n");
    fprintf(f, "\\toprule\n");
    fprintf(f, "Año de Uso & Mantenimiento & Valor Residual \\\\\n");
    fprintf(f, "\\midrule\n");
    for (int i = 0; i < e->n && i < vida_util_ajustada; i++) {
        fprintf(f, "%d & \\$%.2f & \\$%.2f \\\\\n", 
                i + 1, e->costo_mant[i], e->valor_residual[i]);
    }
    fprintf(f, "\\bottomrule\n");
    fprintf(f, "\\end{tabular}\n");
    fprintf(f, "\\end{table}\n\n");
    
    // Sección 3: Cálculo de costos C(t,j) 
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\section*{Cálculo de Costos $C_{t,j}$}\n");
    
    fprintf(f, "\\begin{longtable}{cccc}\n");
    fprintf(f, "\\caption{Cálculo detallado de costos por período} \\\\\n");
    fprintf(f, "\\toprule\n");
    fprintf(f, "Período (t-j) & Duración & Fórmula & Costo \\\\\n");
    fprintf(f, "\\midrule\n");
    fprintf(f, "\\endfirsthead\n");

    fprintf(f, "\\multicolumn{4}{c}{\\tablename\\ \\thetable\\ -- Continúa} \\\\\n");
    fprintf(f, "\\toprule\n");
    fprintf(f, "Período (t-j) & Duración & Fórmula & Costo \\\\\n");
    fprintf(f, "\\midrule\n");
    fprintf(f, "\\endhead\n");

    fprintf(f, "\\midrule\n");
    fprintf(f, "\\multicolumn{4}{r}{Continúa en la siguiente página} \\\\\n");
    fprintf(f, "\\endfoot\n");

    fprintf(f, "\\bottomrule\n");
    fprintf(f, "\\endlastfoot\n");
    
    for (int t = 0; t < e->n; t++) {
        for (int j = t + 1; j <= e->n; j++) {
            int duracion = j - t;
            if (duracion <= vida_util_ajustada) {
                double costo = calcular_costo_periodo_correcto(e, t, j, vida_util);
                fprintf(f, "%d-%d & %d año%s & $%.0f", t, j, duracion, duracion > 1 ? "s" : "", e->costo[0]);
                for (int k = 0; k < duracion; k++) {
                    if (k < e->n) {
                        fprintf(f, " + %.0f", e->costo_mant[k]);
                    }
                }
                fprintf(f, " - %.0f$ & \\$%.2f \\\\\n", e->valor_residual[duracion - 1], costo);
            }
        }
    }
    fprintf(f, "\\end{longtable}\n\n");
    
    /// Sección 4: Cálculo de g(t) 
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\section*{Cálculo de $g(t)$ (Programación Dinámica)}\n");
    fprintf(f, "\\begin{itemize}\n");
    fprintf(f, "\\item $g(%d) = 0$ (caso base)\n", e->n);

    double *g_calculado = g_new0(double, e->n + 1);
    g_calculado[e->n] = 0.0;
    GPtrArray **opciones_optimas = g_new0(GPtrArray*, e->n + 1);
    for (int i = 0; i <= e->n; i++) {
        opciones_optimas[i] = g_ptr_array_new();
    }

    for (int t = e->n - 1; t >= 0; t--) {
        fprintf(f, "\\item $g(%d) = \\min\\{ ", t);
        
        double min_costo = 1e20;
        int count = 0;
        
        for (int j = t + 1; j <= e->n; j++) {
            int duracion = j - t;
            if (duracion <= vida_util_ajustada) {
                double costo_periodo = calcular_costo_periodo_correcto(e, t, j, vida_util);
                double costo_total = costo_periodo + g_calculado[j];
                
                if (costo_total < min_costo - 1e-10) { 
                    min_costo = costo_total;
                    g_ptr_array_set_size(opciones_optimas[t], 0);
                    int *j_optimo = g_new(int, 1);
                    *j_optimo = j;
                    g_ptr_array_add(opciones_optimas[t], j_optimo);
                } else if (fabs(costo_total - min_costo) < 1e-10) {
                    int *j_optimo = g_new(int, 1);
                    *j_optimo = j;
                    g_ptr_array_add(opciones_optimas[t], j_optimo);
                }
            }
        }
        for (int j = t + 1; j <= e->n; j++) {
            int duracion = j - t;
            if (duracion <= vida_util_ajustada) {
                double costo_periodo = calcular_costo_periodo_correcto(e, t, j, vida_util);
                double costo_total = costo_periodo + g_calculado[j];
                
                if (count > 0) fprintf(f, ", ");
                gboolean es_optima = FALSE;
                for (guint k = 0; k < opciones_optimas[t]->len; k++) {
                    int *j_opt = (int*)g_ptr_array_index(opciones_optimas[t], k);
                    if (*j_opt == j) {
                        es_optima = TRUE;
                        break;
                    }
                }
                
                if (es_optima) {
                    fprintf(f, "\\mathbf{C_{%d,%d} + g(%d) = %.2f}", t, j, j, costo_total);
                } else {
                    fprintf(f, "C_{%d,%d} + g(%d) = %.2f", t, j, j, costo_total);
                }
                
                count++;
            }
        }
        
        g_calculado[t] = min_costo;
        
        // Reportar empates si existen
        if (opciones_optimas[t]->len > 1) {
            fprintf(f, "\\} = \\$%.2f$ \\textbf{(Empate: ", min_costo);
            for (guint k = 0; k < opciones_optimas[t]->len; k++) {
                int *j_opt = (int*)g_ptr_array_index(opciones_optimas[t], k);
                if (k > 0) fprintf(f, ", ");
                fprintf(f, "j=%d", *j_opt);
            }
            fprintf(f, ")}\n");
        } else if (opciones_optimas[t]->len == 1) {
            int *j_opt = (int*)g_ptr_array_index(opciones_optimas[t], 0);
            fprintf(f, "\\} = \\$%.2f$ \\textbf{(j=%d)}\n", min_costo, *j_opt);
        } else {
            fprintf(f, "\\} = \\$%.2f$\n", min_costo);
        }
    }
    fprintf(f, "\\end{itemize}\n\n");

    // Agregar una sección explicativa sobre empates
    fprintf(f, "\\subsection*{Empates}\n");
    fprintf(f, "Se han resaltado en \\textbf{negrita} las opciones óptimas.\\\\\n");

    gboolean hay_empates = FALSE;
    for (int t = 0; t < e->n; t++) {
        if (opciones_optimas[t]->len > 1) {
            if (!hay_empates) {
                fprintf(f, "\\textbf{Empates encontrados:}\n");
                fprintf(f, "\\begin{itemize}\n");
                hay_empates = TRUE;
            }
            fprintf(f, "\\item En $g(%d)$: múltiples opciones óptimas con j = ", t);
            for (guint k = 0; k < opciones_optimas[t]->len; k++) {
                int *j_opt = (int*)g_ptr_array_index(opciones_optimas[t], k);
                if (k > 0) fprintf(f, ", ");
                fprintf(f, "%d", *j_opt);
            }
            fprintf(f, " (costo: \\$%.2f)\n", g_calculado[t]);
        }
    }

    if (hay_empates) {
        fprintf(f, "\\end{itemize}\n");
        fprintf(f, "Los empates indican que existen múltiples estrategias óptimas para reemplazar el equipo.\\\\\n");
    } else {
        fprintf(f, "\\textbf{No se encontraron empates.} Existe una única estrategia óptima para cada año de inicio.\\\\\n");
    }

    // Limpiar memoria
    for (int i = 0; i <= e->n; i++) {
        for (guint j = 0; j < opciones_optimas[i]->len; j++) {
            int *j_opt = (int*)g_ptr_array_index(opciones_optimas[i], j);
            g_free(j_opt);
        }
        g_ptr_array_free(opciones_optimas[i], TRUE);
    }
    g_free(opciones_optimas);
    g_free(g_calculado);
    
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\section*{Solución Óptima}\n");
    fprintf(f, "\\textbf{Costo mínimo total:} \\$%.2f\\\\\n", sol->costo_minimo);
    fprintf(f, "\\textbf{Planes óptimos encontrados:} %d\n", sol->planes->len);

    
    if (sol->planes->len > 0) {
        gchar *base_nombre = g_strndup(filename, strlen(filename) - 4); 
        
        fprintf(f, "\\subsection*{Grafos de Planes Óptimos}\n");
        fprintf(f, "A continuación se presentan los grafos de \\emph{saltos de rana} para cada plan óptimo encontrado.\n\n");

        for (guint i = 0; i < sol->planes->len; i++) {
            char *plan = (char*)g_ptr_array_index(sol->planes, i);
            
            gchar *nombre_grafo = g_strdup_printf("%s_plan_%d", base_nombre, i+1);
            gchar *ruta_grafo = g_build_filename(dir, nombre_grafo, NULL);
            gboolean grafo_generado = generar_grafo_saltos_rana_completo(plan, ruta_grafo, e->n);
            
            if (grafo_generado) {
                fprintf(f, "\\begin{figure}[H]\n");
                fprintf(f, "\\centering\n");
                fprintf(f, "\\includegraphics[width=0.8\\textwidth]{%s.png}\n", nombre_grafo);
                fprintf(f, "\\caption{Plan Óptimo %d: \\texttt{%s}}\n", i+1, plan);
                fprintf(f, "\\label{fig:plan%d}\n", i+1);
                fprintf(f, "\\end{figure}\n\n");
                fprintf(f, "\\textbf{Plan %d:} \\texttt{%s}\n", i+1, plan);
                
                // Análisis detallado del plan
                gchar **saltos = g_strsplit(plan, "-", -1);
                if (saltos) {
                    fprintf(f, "\\begin{itemize}\\small\n");
                    for (int j = 0; saltos[j] && saltos[j+1]; j++) {
                        int inicio = atoi(saltos[j]);
                        int fin = atoi(saltos[j+1]);
                        int duracion = fin - inicio;
                        double costo = calcular_costo_periodo_correcto(e, inicio, fin, vida_util);
                        fprintf(f, "\\item Período %d-%d: %d año%s, Costo: \\$%.2f\n", inicio, fin, duracion, duracion > 1 ? "s" : "", costo);

                    }
                    fprintf(f, "\\end{itemize}\n");
                    g_strfreev(saltos);
                }
                fprintf(f, "\n");
            }
            
            g_free(nombre_grafo);
            g_free(ruta_grafo);
        }
        
        g_free(base_nombre);
    } else {
        fprintf(f, "No se encontraron planes óptimos.\n");
    }
    
    fprintf(f, "\\end{document}\n");
    fclose(f);
    
    g_free(dir);
    g_free(tex_path);
}

void on_exit_reemplazo_clicked(GtkButton *button, gpointer data) {
    gtk_main_quit();
}


void on_calc_reemplazo_clicked(GtkButton *button, gpointer data) {
    Equipment e;
    if (!read_table(&e)) return;
    int plazo_proyecto = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vidaSpin));
    int vida_util_equipo = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(plazoSpin));
    e.n = plazo_proyecto;
    SolucionReemplazo *sol = equipo_replacement_algorithm_corregido(&e, vida_util_equipo);
    const gchar *nombre_archivo = NULL;
    if (fileNameReemplazo && GTK_IS_ENTRY(fileNameReemplazo)) {
        nombre_archivo = gtk_entry_get_text(GTK_ENTRY(fileNameReemplazo));
    }
    
    gchar *nombre_base = NULL;
    if (nombre_archivo && *nombre_archivo) {
        if (g_str_has_suffix(nombre_archivo, ".csv") || g_str_has_suffix(nombre_archivo, ".tex")) {
            gchar *temp = g_strndup(nombre_archivo, strlen(nombre_archivo) - 4);
            nombre_base = g_strdup(temp);
            g_free(temp);
        } else {
            nombre_base = g_strdup(nombre_archivo);
        }
    } else {
        nombre_base = g_strdup("reporte_reemplazo");
    }
    gchar *nombre_tex = g_strconcat(nombre_base, ".tex", NULL);
    generar_reporte_latex_corregido(&e, vida_util_equipo, sol, nombre_tex);    
    gchar *path = set_path_csv();
    gboolean saved = save_to_csv(path);
    gchar *tex_file = g_build_filename(g_get_current_dir(), "ReportsEquipment", nombre_tex, NULL);
    compile_latex_file(tex_file);
    
    // Liberar memoria
    g_free(nombre_base);
    g_free(nombre_tex);
    g_ptr_array_free(sol->planes, TRUE);
    g_free(sol);
    g_free(path);
    g_free(tex_file);
    free_equipment(&e);
}


void on_plazoSpin_value_changed(GtkSpinButton *spin, gpointer user_data) {
    int plazo = gtk_spin_button_get_value_as_int(spin);

    gboolean profitSelected = FALSE;
    if (profitCheck && GTK_IS_TOGGLE_BUTTON(profitCheck)) {
        profitSelected = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(profitCheck));
    }

    build_table(plazo, profitSelected);
}

void on_profitCheck_toggled(GtkCheckButton *check, gpointer user_data){
    profitSelected = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check));
    
    if (plazoSpin) {
        int plazo = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(plazoSpin));
        build_table(plazo, profitSelected);
    }
}

// Inicializar tabla de equipo (placeholder)
void init_equipo_table(int vida) {
    g_print("Inicializando tabla de equipo con vida = %d\n", vida);
}

void on_load_to_grid_reemplazo(GtkButton *button, gpointer data) {
    const char *path = selected_csv_path;
    if (!path || !*path) {
        GtkWidget *dlg = gtk_message_dialog_new(
            GTK_WINDOW(windowReemplazo), GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
            "Seleccione un archivo CSV primero."
        );
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return;
    }
    gboolean ok = load_from_csv(path);
}

G_MODULE_EXPORT void on_save_reemplazo_clicked(GtkWidget *saveProblem, gpointer data) {
    gchar *default_folder = g_build_filename(g_get_current_dir(), "Saved_Equipment", NULL);
    g_mkdir_with_parents(default_folder, 0755);

    GtkWidget *dlg = gtk_file_chooser_dialog_new("Guardar CSV",GTK_WINDOW(windowReemplazo),GTK_FILE_CHOOSER_ACTION_SAVE,"Cancelar", GTK_RESPONSE_CANCEL,"Guardar",  GTK_RESPONSE_ACCEPT,NULL
    );

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), default_folder);

    const gchar *name_entry = (fileNameReemplazo && GTK_IS_ENTRY(fileNameReemplazo))? gtk_entry_get_text(GTK_ENTRY(fileNameReemplazo)): "";
    gchar *suggested = set_extension((name_entry && *name_entry) ? name_entry : "equipment_replacement.csv");
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), suggested);

    GtkFileFilter *ff = gtk_file_filter_new();
    gtk_file_filter_set_name(ff, "CSV (*.csv)");
    gtk_file_filter_add_pattern(ff, "*.csv");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), ff);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *picked = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (picked) {
            gchar *final_path = set_extension(g_path_get_basename(picked));
            gchar *dir = g_path_get_dirname(picked);
            gchar *full = g_build_filename(dir, final_path, NULL);

            gboolean saved = save_to_csv(full);

            GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(windowReemplazo), GTK_DIALOG_MODAL,saved ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,saved ? "Problem saved in:\n%s" : "Couldn't save:\n%s",full);
            gtk_dialog_run(GTK_DIALOG(msg));
            gtk_widget_destroy(msg);

            if (saved && fileNameReemplazo && GTK_IS_ENTRY(fileNameReemplazo)) {
                gtk_entry_set_text(GTK_ENTRY(fileNameReemplazo), final_path);
            }

            g_free(full);
            g_free(dir);
            g_free(final_path);
            g_free(picked);
        }
    }

    gtk_widget_destroy(dlg);
    g_free(suggested);
    g_free(default_folder);
}

void on_file_load_reemplazo(GtkFileChooserButton *chooser, gpointer data) {
    if (selected_csv_path) { 
        g_free(selected_csv_path); 
        selected_csv_path = NULL; 
    }
    selected_csv_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));

    gtk_widget_set_sensitive(loadToGridReemplazo, TRUE);
    gtk_widget_set_sensitive(saveReemplazo, TRUE);

    if (selected_csv_path && fileNameReemplazo && GTK_IS_ENTRY(fileNameReemplazo)) {
        gchar *base = g_path_get_basename(selected_csv_path);
        gtk_entry_set_text(GTK_ENTRY(fileNameReemplazo), base ? base : "");
        g_free(base);
    }
}

// MAIN
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    GtkBuilder *builder = gtk_builder_new_from_file("ReemplazoEquipos.glade");
    
    windowReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "windowReemplazo"));
    
    g_signal_connect(windowReemplazo, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_builder_connect_signals(builder, NULL);
    
    costoEntry = GTK_WIDGET(gtk_builder_get_object(builder, "costoEntry"));
    plazoSpin = GTK_WIDGET(gtk_builder_get_object(builder, "plazoSpin"));
    vidaSpin = GTK_WIDGET(gtk_builder_get_object(builder, "vidaSpin"));
    scrollReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "scrollReemplazo"));
    calcReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "calcReemplazo"));
    editLatexReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "editLatexReemplazo"));
    fileLoadReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "fileLoadReemplazo"));
    loadToGridReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "loadToGridReemplazo"));
    saveReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "saveReemplazo"));
    fileNameReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "fileNameReemplazo"));
    exitReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "exitReemplazo"));
    profitCheck = GTK_WIDGET(gtk_builder_get_object(builder, "profitCheck"));
    
    GtkWidget *costoLabel = GTK_WIDGET(gtk_builder_get_object(builder, "costoLabel"));
    GtkWidget *plazoLabel = GTK_WIDGET(gtk_builder_get_object(builder, "plazoLabel"));
    GtkWidget *vidaLabel = GTK_WIDGET(gtk_builder_get_object(builder, "vidaLabel"));
    GtkWidget *loadLabelReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "loadLabelReemplazo"));
    GtkWidget *nameLabelReemplazo = GTK_WIDGET(gtk_builder_get_object(builder, "nameLabelReemplazo"));


    set_css(cssProvider, windowReemplazo);
    set_css(cssProvider, fileLoadReemplazo);
    set_css(cssProvider, exitReemplazo);
    set_css(cssProvider, scrollReemplazo);
    set_css(cssProvider, calcReemplazo);
    set_css(cssProvider, editLatexReemplazo);
    set_css(cssProvider, loadToGridReemplazo);
    set_css(cssProvider, saveReemplazo);
  
    g_signal_connect(editLatexReemplazo, "clicked", G_CALLBACK(on_select_latex_file_reemplazo), NULL);
    g_signal_connect(exitReemplazo, "clicked", G_CALLBACK(on_exit_reemplazo_clicked), NULL);
    g_signal_connect(calcReemplazo, "clicked", G_CALLBACK(on_calc_reemplazo_clicked), NULL);
    g_signal_connect(loadToGridReemplazo, "clicked", G_CALLBACK(on_load_to_grid_reemplazo), NULL);
    g_signal_connect(saveReemplazo, "clicked", G_CALLBACK(on_save_reemplazo_clicked), NULL);
    g_signal_connect(fileLoadReemplazo, "file-set", G_CALLBACK(on_file_load_reemplazo), NULL);
    //g_signal_connect(vidaSpin, "value-changed", G_CALLBACK(on_vida_changed), NULL);
    g_signal_connect(plazoSpin, "value-changed", G_CALLBACK(on_plazoSpin_value_changed), NULL);
    gtk_widget_set_sensitive(loadToGridReemplazo, FALSE);
    gtk_widget_set_sensitive(saveReemplazo, FALSE);
    
    //init_equipo_table(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vidaSpin)));
    
    gtk_widget_show_all(windowReemplazo);
    
    gtk_main();

    return EXIT_SUCCESS;
}