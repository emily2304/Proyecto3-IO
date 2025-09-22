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



// --- Algoritmo de reemplazo de equipos ---
double equipo_replacement_algorithm(int n, double *costo, double *valor_residual, double *costo_mant, double *beneficio, double W) {
    dp_equipo = (double **)malloc((n + 1) * sizeof(double *));
    decision_equipo = (int **)malloc((n + 1) * sizeof(int *));
    for (int i = 0; i <= n; i++) {
        dp_equipo[i] = (double *)malloc((int)(W + 1) * sizeof(double));
        decision_equipo[i] = (int *)malloc((int)(W + 1) * sizeof(int));
    }

    for (int i = 0; i <= n; i++) {
        for (int w = 0; w <= W; w++) {
            if (i == 0 || w == 0) {
                dp_equipo[i][w] = 0;
            } else if (costo[i - 1] <= w) {
                double incl = beneficio[i - 1] - costo_mant[i - 1] - valor_residual[i - 1]
                              + dp_equipo[i - 1][(int)(w - costo[i - 1])];
                double excl = dp_equipo[i - 1][w];
                if (incl > excl) {
                    dp_equipo[i][w] = incl;
                    decision_equipo[i][w] = 1;
                } else {
                    dp_equipo[i][w] = excl;
                    decision_equipo[i][w] = 0;
                }
            } else {
                dp_equipo[i][w] = dp_equipo[i - 1][w];
                decision_equipo[i][w] = 0;
            }
        }
    }

    double result = dp_equipo[n][(int)W];
    return result;
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

    dialog = gtk_file_chooser_dialog_new("Select LaTeX File",
                                        GTK_WINDOW(windowReemplazo),
                                        action,
                                        "Cancel",
                                        GTK_RESPONSE_CANCEL,
                                        "Open",
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

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

void on_exit_reemplazo_clicked(GtkButton *button, gpointer data) {
    gtk_main_quit();
}

void on_calc_reemplazo_clicked(GtkButton *button, gpointer data) {
    Equipment e;
    if (!read_table(&e)) return;

    free_dp_and_decisions(e.n, (int)e.W);


    double best = equipo_replacement_algorithm(
        e.n, e.costo, e.valor_residual, e.costo_mant, e.beneficio, e.W
    );

    gchar *path = set_path_csv();
    gboolean saved = save_to_csv(path);

    g_free(path);
}

void on_vida_changed(GtkSpinButton *spin, gpointer data) {
    g_print("Vida cambiada: %d\n", gtk_spin_button_get_value_as_int(spin));
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

void on_save_reemplazo_clicked(GtkButton *button, gpointer data) {
    g_print("Save reemplazo clicked\n");
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
    g_signal_connect(vidaSpin, "value-changed", G_CALLBACK(on_vida_changed), NULL);
    g_signal_connect(plazoSpin, "value-changed", G_CALLBACK(on_plazoSpin_value_changed), NULL);
    gtk_widget_set_sensitive(loadToGridReemplazo, FALSE);
    gtk_widget_set_sensitive(saveReemplazo, FALSE);
    
    //init_equipo_table(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vidaSpin)));
    
    gtk_widget_show_all(windowReemplazo);
    
    gtk_main();

    return EXIT_SUCCESS;
}