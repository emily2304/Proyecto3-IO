/* Proyecto 1 IO -PR01
Estudiantes:
Emily Sanchez -
Viviana Vargas -
*/

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <math.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

//--- GTK Variables ----
//Window 1
GtkWidget   *window1;
GtkWidget   *fixed2;
GtkWidget   *title;
GtkWidget   *description;
GtkWidget   *instruction;
GtkWidget   *spinNodes;
GtkWidget   *fileLoad;
GtkWidget   *loadLabel;
GtkWidget   *exitButton1;
GtkWidget   *scrollWindow;
GtkWidget   *createSolution;
GtkWidget   *fileName;
GtkWidget *editLatexButton;
GtkWidget   *loadToGrid;
GtkWidget   *saveProblem;

//Builders
GtkBuilder  *builder;
GtkCssProvider *cssProvider;
//Dynamic Widgets
GtkWidget *current_grid = NULL;

//Variables Globales
static GPtrArray *col_headers = NULL; 
static GPtrArray *row_headers = NULL; 
static gint current_n = 0;
static gboolean syncing_headers = FALSE;
static int **matrix = NULL;
static int **path_matrix = NULL;
gchar *last_selected_tex = NULL;
static char *filepath = NULL;

//Infinito 
#define INFINITO 9999999

// Declaración adelantada de funciones
//static void read_matrix_input(void);
//void on_loadProblem_clicked(GtkWidget *loadProblem, gpointer data);

//Función para que se utilice el archivo .css como proveedor de estilos.
void set_css (GtkCssProvider *cssProvider, GtkWidget *widget){
    GtkStyleContext *styleContext = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(styleContext,GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

// --- Helpers ---

static gchar* value_to_token(int v) {
    if (v == INFINITO) {
        return g_strdup("INF");
    }
    return g_strdup_printf("%d", v);
}

static int token_to_value(const gchar *token) {
    if (!token || !*token) {
        return INFINITO;
    }
    if (g_utf8_collate(token, "∞") == 0) {
        return INFINITO;
    }
    if (strcasecmp(token, "I") == 0) {
        return INFINITO;
    }
    if (strcasecmp(token, "INF") == 0){
        return INFINITO;
    }
    return atoi(token); 
    
}

static inline int value_validator(const gchar *text) {
    if (!text || !*text){
        return INFINITO;
    }
    if (g_utf8_collate(text, "∞") == 0){
        return INFINITO;
    }
    if (strcasecmp(text, "I") == 0 || strcasecmp(text, "INF") == 0){
        return INFINITO;
    }
    int val = atoi(text);
    if (val < 0){
        return INFINITO;
    }
    return val;
}

static gchar* index_to_label(gint index) {
    GString *s = g_string_new(NULL);
    gint n = index;
    while (n > 0) {
        n--;
        gint rem = n % 26;
        g_string_prepend_c(s, 'A' + rem);
        n /= 26;
    }
    return g_string_free(s, FALSE);
}

// Normaliza a solo letras de la A a la Z (Mayúscula). 
static gchar* name_label(const gchar *in, gint index) {
    GString *s = g_string_new(NULL);
    for (const char *p = in; *p; ++p) {
        if (g_ascii_isalpha(*p)) g_string_append_c(s, g_ascii_toupper(*p));
    }
    if (s->len == 0) {
        g_string_free(s, TRUE);
        return index_to_label(index);
    }
    return g_string_free(s, FALSE);
}

static void on_header_changed(GtkEditable *editable, gpointer user_data) {
    if (syncing_headers) {
        return;
    }
    gboolean is_col = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(editable), "is_col"));
    int index        = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(editable), "index"));
    
    if (index <= 0 || index > current_n) {
        return;
    }

    const gchar *filename_2 = gtk_entry_get_text(GTK_ENTRY(editable));
    gchar *clean = name_label(filename_2, index);

    GtkEntry *peer = GTK_ENTRY(
        g_ptr_array_index(is_col ? row_headers : col_headers, index)
    );
    if (!peer) { 
        g_free(clean); return; 
    }

    syncing_headers = TRUE;
    gtk_entry_set_text(GTK_ENTRY(editable), clean);
    gtk_entry_set_text(peer, clean);                
    syncing_headers = FALSE;

    g_free(clean);
}

static void read_matrix_input(void) {
    if (matrix != NULL) {
        for (int i = 0; i < current_n; i++) free(matrix[i]);
        free(matrix);
        matrix = NULL;
    }

    if (!current_grid || current_n <= 0) {
        return;
    }

    matrix = malloc(current_n * sizeof(int*));
    if (!matrix) { 
        //g_printerr("Malloc falló. \n"); 
        return; 
    }

    for (int i = 0; i < current_n; i++) {
        matrix[i] = calloc(current_n, sizeof(int));   // <<<< ojo: current_n
        if (!matrix[i]) {
            //g_printerr("Calloc falló en la fila %d\n", i);
            for (int k = 0; k < i; k++) free(matrix[k]);
            free(matrix);
            matrix = NULL;
            return;
        }
    }

    for (int row = 1; row <= current_n; row++) {
        for (int col = 1; col <= current_n; col++) {
            GtkWidget *entry = gtk_grid_get_child_at(GTK_GRID(current_grid), col, row);
            if (!entry) {
                matrix[row-1][col-1] = INFINITO;
                continue;
            }

            const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
            int value = value_validator(text);
            
            if (row == col) {
                value = 0;
            }
            matrix[row-1][col-1] = value;
            int x = matrix[row-1][col-1];
        }
    }
}

// --- Algoritmo de Floyd ---
void floyd_algorithm(int n, int **dist, int **path) {
    // Inicializar matriz de caminos
    for (int i = 0; i < n; i++) {
        for ( int j = 0; j < n; j++) {
            if (i != j && dist[i][j] != INFINITO) {
                path[i][j] = i;
            } else {
                path[i][j] = -1;
            }
        }
    }
    
    // Algoritmo de Floyd
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (dist[i][k] != INFINITO && dist[k][j] != INFINITO && 
                    dist[i][j] > dist[i][k] + dist[k][j]) {
                    dist[i][j] = dist[i][k] + dist[k][j];
                    path[i][j] = path[k][j];
                }
            }
        }
    }
}

// --- Generación de archivo LaTeX ---
gboolean generate_latex_report(const char *filename, int n, int **initial_matrix, int **final_matrix, int **final_path_matrix, GPtrArray *row_headers, GPtrArray *col_headers) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        return FALSE;
    }
    
    // Encabezado del documento latex
    fprintf(f, "\\documentclass[12pt]{article}\n");
    fprintf(f, "\\usepackage[utf8]{inputenc}\n");
    fprintf(f, "\\usepackage[spanish]{babel}\n");
    fprintf(f, "\\usepackage{graphicx}\n");
    fprintf(f, "\\usepackage{amsmath}\n");
    fprintf(f, "\\usepackage{amssymb}\n");
    fprintf(f, "\\usepackage{array}\n");
    fprintf(f, "\\usepackage{xcolor}\n");
    fprintf(f, "\\usepackage{geometry}\n");
    fprintf(f, "\\usepackage{fancyhdr}\n");
    fprintf(f, "\\usepackage{lastpage}\n");
    fprintf(f, "\\usepackage{booktabs}\n");
    fprintf(f, "\\usepackage{colortbl}\n");
    fprintf(f, "\\usepackage{caption}\n");
    fprintf(f, "\\usepackage{multirow}\n");
    fprintf(f, "\\geometry{margin=1in}\n\n");
    fprintf(f, "\\usepackage{float}\n");
    
    // Definición de colores
    fprintf(f, "\\definecolor{lightblue}{RGB}{200,230,255}\n");
    fprintf(f, "\\definecolor{lightgreen}{RGB}{220,255,220}\n");
    fprintf(f, "\\definecolor{lightred}{RGB}{255,220,220}\n");
    fprintf(f, "\\definecolor{lightyellow}{RGB}{255,255,200}\n");
    
    // Configuración de encabezados y pies de página
    fprintf(f, "\\pagestyle{fancy}\n");
    fprintf(f, "\\fancyhf{}\n");
    fprintf(f, "\\fancyhead[L]{Algoritmo de Floyd - Solución}\n");
    fprintf(f, "\\fancyhead[R]{\\thepage\\ de \\pageref{LastPage}}\n");
    fprintf(f, "\\renewcommand{\\headrulewidth}{0.4pt}\n");
    fprintf(f, "\\renewcommand{\\footrulewidth}{0.4pt}\n\n");
    
    fprintf(f, "\\title{Proyecto 1: Rutas Optimas (Algoritmo de Floyd)}\n");
    fprintf(f, "\\author{Emily Sanchez \\\\ Viviana Vargas \\\\[1cm] Curso: Investigación de Operaciones \\\\ II Semestre 2025}\n");
    fprintf(f, "\\date{\\today}\n\n");
    
    fprintf(f, "\\begin{document}\n\n");
    
    // Portada
    fprintf(f, "\\maketitle\n");
    fprintf(f, "\\thispagestyle{empty}\n");
    fprintf(f, "\\newpage\n");
    fprintf(f, "\\setcounter{page}{1}\n\n");
    
    // Introducción al algoritmo
    fprintf(f, "\\section{Introducción}\n");
    fprintf(f, "El algoritmo de Floyd-Warshall es un algoritmo para encontrar los caminos más cortos en un grafo ponderado. Fue publicado por Robert Floyd en 1962.\\\\\n");
    fprintf(f, "El algoritmo de Floyd se basa en el principio de la Programación Dinámica.\\\\\n");
    fprintf(f,"El algoritmo comienza con una tabla llamada G(0) que muestra las distancias directas entre cada nodo. Si dos nodos no están conectados directamente, la tabla marca esa distancia como infinito. Luego verifica si pasar por un nodo intermedio puede hacer que el camino entre dos nodos sea más corto.\\\\\n");
    fprintf(f,"El proceso se repite hasta que todos los nodos intermedios posibles hayan sido probados (es decir, habrá una tabla G(k) para cada nodo k). Al final, la tabla P muestra la distancia más corta posible entre cada par de nodos.\\\\\n");
    fprintf(f,"Podemos visualizar estos problemas con distancias entre ciudades: ¿qué pasa si quiero ir directamente de la ciudad A a la ciudad C? ¿Sería más corto ir directamente de A a C o ir de A a B y de B a C?\\\\\n");
    fprintf(f, "\\textbf{Complejidad espacial:} $O(n^2)$\\\\\n");
    fprintf(f, "\\textbf{Complejidad temporal:} $O(n^3)$\\\\\n");
    
    // Grafo
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\section{Descripción del Problema}\n");
    fprintf(f, "Grafo con %d nodos:\n\n", n);
    
    fprintf(f, "\\begin{itemize}\n");
    for (int i = 0; i < n; i++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1)));
        fprintf(f, "\\item Nodo %c: %s\n", 'A' + i, name);
    }
    fprintf(f, "\\end{itemize}\n\n");
    
    // Incluir imagen del grafo
    fprintf(f, "\\begin{figure}[h!]\n");
    fprintf(f, "\\centering\n");
    fprintf(f,"\\includegraphics[width=0.5\\textwidth,keepaspectratio]{grafo.png}\n");
    fprintf(f, "\\caption{Representación del grafo original}\n");
    fprintf(f, "\\end{figure}\n\n");
    
    // Tabla inicial D(0)
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\section{Procedimiento del Algoritmo}\n");
    fprintf(f, "\\subsection{Matriz de Distancias Inicial D(0)}\n");
    fprintf(f, "\\begin{table}[h!]\n");
    fprintf(f, "\\centering\n");
    fprintf(f, "\\begin{tabular}{|c|");
    for (int j = 0; j < n; j++) fprintf(f, "c|");
    fprintf(f, "}\n\\hline\n");
    fprintf(f, " & ");
    for (int j = 0; j < n; j++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(col_headers, j+1)));
        fprintf(f, "%s", name);
        if (j < n-1) fprintf(f, " & ");
    }
    fprintf(f, " \\\\\\hline\n");
    
    for (int i = 0; i < n; i++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1)));
        fprintf(f, "%s & ", name);
        for (int j = 0; j < n; j++) {
            if (initial_matrix[i][j] == INFINITO) {
                fprintf(f, "$\\infty$");  
            } else {
                fprintf(f, "%d", initial_matrix[i][j]);
            }
            if (j < n-1) fprintf(f, " & ");
        }
        fprintf(f, " \\\\\\hline\n");
    }
    fprintf(f, "\\end{tabular}\n");
    fprintf(f, "\\caption{Matriz de distancias inicial D(0)}\n");
    fprintf(f, "\\end{table}\n\n");
    
    // Tabla P inicial
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\subsection{Matriz de Caminos Inicial P(0)}\n");
    fprintf(f, "\\begin{table}[h!]\n");
    fprintf(f, "\\centering\n");
    fprintf(f, "\\begin{tabular}{|c|");
    for (int j = 0; j < n; j++) fprintf(f, "c|");
    fprintf(f, "}\n\\hline\n");
    fprintf(f, " & ");
    for (int j = 0; j < n; j++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(col_headers, j+1)));
        fprintf(f, "%s", name);
        if (j < n-1) fprintf(f, " & ");
    }
    fprintf(f, " \\\\\\hline\n");
    
    for (int i = 0; i < n; i++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1)));
        fprintf(f, "%s & ", name);
        for (int j = 0; j < n; j++) {
            if (i == j) {
                fprintf(f, "-");
            } else if (initial_matrix[i][j] != INFINITO) {
                fprintf(f, "%s", gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1))));
            } else {
                fprintf(f, "-");
            }
            if (j < n-1) fprintf(f, " & ");
        }
        fprintf(f, " \\\\\\hline\n");
    }
    fprintf(f, "\\end{tabular}\n");
    fprintf(f, "\\caption{Matriz de caminos inicial P(0)}\n");
    fprintf(f, "\\end{table}\n\n");
    
    // Crear copias para las iteraciones
    int **dist = malloc(n * sizeof(int*));
    int **path = malloc(n * sizeof(int*));
    for (int i = 0; i < n; i++) {
        dist[i] = malloc(n * sizeof(int));
        path[i] = malloc(n * sizeof(int));
        for (int j = 0; j < n; j++) {
            dist[i][j] = initial_matrix[i][j];
            if (i != j && initial_matrix[i][j] != INFINITO) {
                path[i][j] = i;
            } else {
                path[i][j] = -1;
            }
        }
    }
    
    // Iteraciones del algoritmo
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\subsection{Iteraciones del Algoritmo}\n");
    for (int k = 0; k < n; k++) {
        fprintf(f, "\\subsubsection{Iteración %d (k = %d) - Nodo intermedio: %s}\n", 
                k+1, k+1, gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, k+1))));
        
        // Crear matrices temporales para detectar cambios
        int **old_dist = malloc(n * sizeof(int*));
        int **old_path = malloc(n * sizeof(int*));
        for (int i = 0; i < n; i++) {
            old_dist[i] = malloc(n * sizeof(int));
            old_path[i] = malloc(n * sizeof(int));
            for (int j = 0; j < n; j++) {
                old_dist[i][j] = dist[i][j];
                old_path[i][j] = path[i][j];
            }
        }
        
        // Aplicar algoritmo
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (dist[i][k] != INFINITO && dist[k][j] != INFINITO && 
                    dist[i][j] > dist[i][k] + dist[k][j]) {
                    dist[i][j] = dist[i][k] + dist[k][j];
                    path[i][j] = path[k][j];
                }
            }
        }
        
        // Mostrar matriz D(k+1)
        fprintf(f, "\\begin{table}[h!]\n");
        fprintf(f, "\\centering\n");
        fprintf(f, "\\begin{tabular}{|c|");
        for (int j = 0; j < n; j++) fprintf(f, "c|");
        fprintf(f, "}\n\\hline\n");
        fprintf(f, " & ");
        for (int j = 0; j < n; j++) {
            const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(col_headers, j+1)));
            fprintf(f, "%s", name);
            if (j < n-1) fprintf(f, " & ");
        }
        fprintf(f, " \\\\\\hline\n");
        
        for (int i = 0; i < n; i++) {
            const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1)));
            fprintf(f, "%s & ", name);
            for (int j = 0; j < n; j++) {
                // Resaltar cambios con color
                if (dist[i][j] != old_dist[i][j]) {
                    fprintf(f, "\\cellcolor{lightgreen} ");
                }
                
                if (dist[i][j] == INFINITO) {
                    fprintf(f, "$\\infty$"); 
                } else {
                    fprintf(f, "%d", dist[i][j]);
                }
                
                if (j < n-1) fprintf(f, " & ");
            }
            fprintf(f, " \\\\\\hline\n");
        }
        fprintf(f, "\\end{tabular}\n");
        fprintf(f, "\\caption{Matriz de distancias D(%d) - Cambios resaltados en verde}\n", k+1);
        fprintf(f, "\\end{table}\n\n");
        
        // Mostrar matriz P(k+1)
        fprintf(f, "\\begin{table}[h!]\n");
        fprintf(f, "\\centering\n");
        fprintf(f, "\\begin{tabular}{|c|");
        for (int j = 0; j < n; j++) fprintf(f, "c|");
        fprintf(f, "}\n\\hline\n");
        fprintf(f, " & ");
        for (int j = 0; j < n; j++) {
            const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(col_headers, j+1)));
            fprintf(f, "%s", name);
            if (j < n-1) fprintf(f, " & ");
        }
        fprintf(f, " \\\\\\hline\n");
        
        for (int i = 0; i < n; i++) {
            const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1)));
            fprintf(f, "%s & ", name);
            for (int j = 0; j < n; j++) {
                // Resaltar cambios con color
                if (path[i][j] != old_path[i][j]) {
                    fprintf(f, "\\cellcolor{lightblue} ");
                }
                
                if (path[i][j] == -1) {
                    fprintf(f, "-");
                } else {
                    const gchar *node_name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, path[i][j]+1)));
                    fprintf(f, "%s", node_name);
                }
                
                if (j < n-1) fprintf(f, " & ");
            }
            fprintf(f, " \\\\\\hline\n");
        }
        fprintf(f, "\\end{tabular}\n");
        fprintf(f, "\\caption{Matriz de caminos P(%d) - Cambios resaltados en azul}\n", k+1);
        fprintf(f, "\\end{table}\n\n");
        
        // Liberar memoria de las matrices temporales
        for (int i = 0; i < n; i++) {
            free(old_dist[i]);
            free(old_path[i]);
        }
        free(old_dist);
        free(old_path);
    }
    
    // Liberar memoria de las copias
    for (int i = 0; i < n; i++) {
        free(dist[i]);
        free(path[i]);
    }
    free(dist);
    free(path);
    
    // Tablas finales
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\section{Resultados Finales}\n");
    
    // Matriz de distancias final D(n)
    fprintf(f, "\\subsection{Matriz de Distancias Final D(%d)}\n", n);
    fprintf(f, "\\begin{table}[h!]\n");
    fprintf(f, "\\centering\n");
    fprintf(f, "\\begin{tabular}{|c|");
    for (int j = 0; j < n; j++) fprintf(f, "c|");
    fprintf(f, "}\n\\hline\n");
    fprintf(f, " & ");
    for (int j = 0; j < n; j++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(col_headers, j+1)));
        fprintf(f, "%s", name);
        if (j < n-1) fprintf(f, " & ");
    }
    fprintf(f, " \\\\\\hline\n");
    
    for (int i = 0; i < n; i++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1)));
        fprintf(f, "%s & ", name);
        for (int j = 0; j < n; j++) {
            if (final_matrix[i][j] == INFINITO) {
                fprintf(f, "$\\infty$");  
            } else {
                fprintf(f, "%d", final_matrix[i][j]);
            }
            if (j < n-1) fprintf(f, " & ");
        }
        fprintf(f, " \\\\\\hline\n");
    }
    fprintf(f, "\\end{tabular}\n");
    fprintf(f, "\\caption{Matriz de distancias final D(%d)}\n", n);
    fprintf(f, "\\end{table}\n\n");
    
    // Matriz de caminos final P(n)
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\subsection{Matriz de Caminos Final P(%d)}\n", n);
    fprintf(f, "\\begin{table}[h!]\n");
    fprintf(f, "\\centering\n");
    fprintf(f, "\\begin{tabular}{|c|");
    for (int j = 0; j < n; j++) fprintf(f, "c|");
    fprintf(f, "}\n\\hline\n");
    fprintf(f, " & ");
    for (int j = 0; j < n; j++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(col_headers, j+1)));
        fprintf(f, "%s", name);
        if (j < n-1) fprintf(f, " & ");
    }
    fprintf(f, " \\\\\\hline\n");
    
    for (int i = 0; i < n; i++) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1)));
        fprintf(f, "%s & ", name);
        for (int j = 0; j < n; j++) {
            if (final_path_matrix[i][j] == -1) {
                fprintf(f, "-");
            } else {
                const gchar *node_name = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, final_path_matrix[i][j]+1)));
                fprintf(f, "%s", node_name);
            }
            if (j < n-1) fprintf(f, " & ");
        }
        fprintf(f, " \\\\\\hline\n");
    }
    fprintf(f, "\\end{tabular}\n");
    fprintf(f, "\\caption{Matriz de caminos final P(%d)}\n", n);
    fprintf(f, "\\end{table}\n\n");
    
    // Rutas óptimas
    fprintf(f, "\\clearpage\n");
    fprintf(f, "\\subsection{Rutas Óptimas}\n");
    fprintf(f, "\\begin{itemize}\n");

    gboolean has_valid_routes = FALSE;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j && final_matrix[i][j] != INFINITO) {
                const gchar *from = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, i+1)));
                const gchar *to = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, j+1)));
                
                // Reconstruir ruta primero para verificar si es válida
                int path_stack[n];
                int path_index = 0;
                int current = j;
                gboolean valid_route = TRUE;
                
                while (current != i && current != -1 && path_index < n) {
                    path_stack[path_index++] = current;
                    current = final_path_matrix[i][current];
                    if (current == -1 && path_index == 0) {
                        valid_route = FALSE;
                        break;
                    }
                }
                
                if (valid_route && path_index > 0) {
                    fprintf(f, "\\item \\textbf{%s → %s:} Distancia: %d, Ruta: ", from, to, final_matrix[i][j]);
                    fprintf(f, "%s", from);
                    for (int k = path_index - 1; k >= 0; k--) {
                        if (path_stack[k] >= 0 && path_stack[k] < n) {
                            const gchar *node = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(row_headers, path_stack[k]+1)));
                            fprintf(f, " → %s", node);
                        }
                    }
                    fprintf(f, "\n");
                    has_valid_routes = TRUE;
                }
            }
        }
    }

    if (!has_valid_routes) {
        fprintf(f, "\\item No hay rutas válidas entre los nodos.\n");
    }

    fprintf(f, "\\end{itemize}\n");
    
    fprintf(f, "\\end{document}\n");
    fclose(f);
    return TRUE;
}

// Generación de imagen del grafo usando Graphviz 
static gboolean generate_graph_image(const char *dotfile, const char *pngfile,
                                     int n, int **matrix, GPtrArray *row_headers) {
    FILE *dot = fopen(dotfile, "w");
    if (!dot) return FALSE;

    // Grafo dirigido
    fprintf(dot, "digraph G {\n");
    fprintf(dot, "  rankdir=LR;\n"); 
    fprintf(dot, "  node [shape=circle, style=filled, color=lightblue];\n");

    // Nodos
    for (int i = 0; i < n; i++) {
        const gchar *name = gtk_entry_get_text(
            GTK_ENTRY(g_ptr_array_index(row_headers, i + 1)));
        fprintf(dot, "  \"%s\";\n", name);
    }

    // Aristas
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue; 
            int w = matrix[i][j];
            if (w > 0 && w < INFINITO) {
                const gchar *from = gtk_entry_get_text(
                    GTK_ENTRY(g_ptr_array_index(row_headers, i + 1)));
                const gchar *to = gtk_entry_get_text(
                    GTK_ENTRY(g_ptr_array_index(row_headers, j + 1)));
                fprintf(dot, "  \"%s\" -> \"%s\" [label=\"%d\"];\n", from, to, w);
            }
        }
    }

    fprintf(dot, "}\n");
    fclose(dot);

    // Generar png
    gchar *cmd = g_strdup_printf("dot -Tpng \"%s\" -o \"%s\"", dotfile, pngfile);
    int r = system(cmd);
    g_free(cmd);

    return (r == 0);
}


// --- File Management ----

static gchar* set_extension(const gchar *name) {
    if (!name) {
        name = "";
    }
    gchar *trim = g_strstrip(g_strdup(name));
    if (*trim == '\0') { 
        g_free(trim); 
        return g_strdup("problema.csv"); 
    }
    gchar *lower = g_ascii_strdown(trim, -1);
    gboolean has_csv = g_str_has_suffix(lower, ".csv");
    g_free(lower);

    if (has_csv) {
        return trim;
    }
    gchar *with = g_strconcat(trim, ".csv", NULL);
    g_free(trim);
    return with;
}

static gboolean write_to_csv(const char *filepath) {
    if (!current_grid || current_n <= 0) return FALSE;

    read_matrix_input();

    FILE *f = fopen(filepath, "w");
    if (!f) {
        return FALSE;
    }

    //Nombres de Columnas
    fprintf(f, ",");
    for (int col = 1; col <= current_n; col++) {
        GtkWidget *cell = g_ptr_array_index(col_headers, col);
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(cell));
        fprintf(f, " %s", name && *name ? name : "");
        if (col < current_n) {
            fprintf(f, ",");
        }
    }
    fprintf(f, "\n");

    //Filas
    for (int row = 1; row <= current_n; row++) {
        GtkWidget *cell = g_ptr_array_index(row_headers, row);
        const gchar *rname = gtk_entry_get_text(GTK_ENTRY(cell));
        fprintf(f, "%s,", rname && *rname ? rname : "");

        for (int col = 1; col <= current_n; col++) {
            int v = matrix[row-1][col-1];
            gchar *token = value_to_token(v);
            fprintf(f, "%s", token);
            g_free(token);
            if (col < current_n) {
                fprintf(f, ",");
            }
        }
        fprintf(f, "\n");
    }

    fclose(f);
    return TRUE;
}

static void show_error(const char *mensaje) {
    GtkWidget *md = gtk_message_dialog_new(GTK_WINDOW(window1),
        GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", mensaje);
    gtk_dialog_run(GTK_DIALOG(md));
    gtk_widget_destroy(md);
}

static void show_info(const char *mensaje) {
    GtkWidget *md = gtk_message_dialog_new(GTK_WINDOW(window1), GTK_DIALOG_MODAL, 
                                          GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", mensaje);
    gtk_dialog_run(GTK_DIALOG(md));
    gtk_widget_destroy(md);
}



static void build_matrix_grid(GtkWidget *scrolled, int n) {
    // Limpiar grid anterior
    if (current_grid) {
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(scrolled));
        if (child) gtk_container_remove(GTK_CONTAINER(scrolled), child);
        current_grid = NULL;
    }
    if (col_headers) { 
        g_ptr_array_free(col_headers, TRUE); col_headers = NULL; 
    }
    if (row_headers) { 
        g_ptr_array_free(row_headers, TRUE); row_headers = NULL; 
    }
    //gtk_entry_set_text(GTK_ENTRY(fileName), "");
    current_n = n;
    col_headers = g_ptr_array_sized_new(n+1);
    row_headers = g_ptr_array_sized_new(n+1);
    g_ptr_array_set_size(col_headers, n+1); 
    g_ptr_array_set_size(row_headers, n+1);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);

    //Esquina superior izquierda (0,0)
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(" "), 0, 0, 1, 1);

    // Hacer los headers de columnas editables
    for (int col = 1; col <= n; col++) {
        GtkWidget *cell = gtk_entry_new();
        gchar *text = index_to_label(col);  
        gtk_entry_set_text(GTK_ENTRY(cell), text);
        g_free(text);


        g_object_set_data(G_OBJECT(cell), "is_col", GINT_TO_POINTER(TRUE));
        g_object_set_data(G_OBJECT(cell), "index",    GINT_TO_POINTER(col));
        g_signal_connect(cell, "changed", G_CALLBACK(on_header_changed), NULL);

        g_ptr_array_index(col_headers, col) = cell;

        GtkStyleContext *context = gtk_widget_get_style_context(cell);
        gtk_style_context_add_class(context, "matrix-cell");

        gtk_grid_attach(GTK_GRID(grid), cell, col, 0, 1, 1);
    }

    //Headers de filas y celdas
    for (int row = 1; row <= n; row++) {
        GtkWidget *cell = gtk_entry_new();
        gchar *text = index_to_label(row); 
        gtk_entry_set_text(GTK_ENTRY(cell), text);
        g_free(text);

        g_object_set_data(G_OBJECT(cell), "is_col", GINT_TO_POINTER(FALSE));
        g_object_set_data(G_OBJECT(cell), "index",    GINT_TO_POINTER(row));
        g_signal_connect(cell, "changed", G_CALLBACK(on_header_changed), NULL);

        g_ptr_array_index(row_headers, row) = cell;
        gtk_grid_attach(GTK_GRID(grid), cell, 0, row, 1, 1);
        
        for (int col = 1; col <= n; col++) {
            GtkWidget *entry = gtk_entry_new();
            gtk_entry_set_width_chars(GTK_ENTRY(entry), 6);
            gtk_entry_set_alignment(GTK_ENTRY(entry), 0.5);
            
            // Si la columna y la fila son iguales, es diagonal. Se agrega el 0 y se bloquea la celda.
            if (row == col) {
                gtk_entry_set_text(GTK_ENTRY(entry), "0");
                gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
                gtk_widget_set_sensitive(entry, FALSE);
            } else {
                gtk_entry_set_text(GTK_ENTRY(entry), "∞");
            }
            
            g_object_set_data(G_OBJECT(entry), "row", GINT_TO_POINTER(row));
            g_object_set_data(G_OBJECT(entry), "col", GINT_TO_POINTER(col));
            gtk_grid_attach(GTK_GRID(grid), entry, col, row, 1, 1);
        }
    }

    gtk_widget_show_all(grid);
    gtk_container_add(GTK_CONTAINER(scrolled), grid);
    current_grid = grid;

    //Asegurarse que las columnas y filas mantengan el mismo nombre
    syncing_headers = TRUE;
    for (int k = 1; k <= n; k++) {
        const gchar *t = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(col_headers, k)));
        gtk_entry_set_text(GTK_ENTRY(g_ptr_array_index(row_headers, k)), t);
    }
    syncing_headers = FALSE;
}

G_MODULE_EXPORT void on_spinNodes_value_changed(GtkSpinButton *spin, gpointer user_data) {
    int n = gtk_spin_button_get_value_as_int(spin);
    if (n < 1){
        n = 1;
    } 
    build_matrix_grid(scrollWindow, n);
}

static void set_spin_value(GtkSpinButton *spin, int n) {
    gulong hid = g_signal_handler_find(spin, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       (gpointer)on_spinNodes_value_changed, NULL);
    if (hid) {
        g_signal_handler_block(spin, hid);
    }
    gtk_spin_button_set_value(spin, n);
    if (hid) {
        g_signal_handler_unblock(spin, hid);
    }
}

// Función para seleccionar archivo latex
void on_select_latex_file(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Seleccionar archivo LaTeX",
                                        GTK_WINDOW(window1),
                                        action,
                                        "Cancelar",
                                        GTK_RESPONSE_CANCEL,
                                        "Abrir",
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

    // Filtro para archivos .tex
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Archivos LaTeX (*.tex)");
    gtk_file_filter_add_pattern(filter, "*.tex");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // Directorio por defecto a Reports carpeta de archivos
    gchar *reports_dir = g_build_filename(g_get_current_dir(), "Reports", NULL);
    if (g_file_test(reports_dir, G_FILE_TEST_IS_DIR)) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), reports_dir);
    }
    g_free(reports_dir);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        if (last_selected_tex) g_free(last_selected_tex);
        last_selected_tex = gtk_file_chooser_get_filename(chooser);
        
        // Preguntar si quiere editar o compilar
        GtkWidget *choice_dialog = gtk_dialog_new_with_buttons(
            "¿Qué deseas hacer?",
            GTK_WINDOW(window1),
            GTK_DIALOG_MODAL,
            "Editar",
            GTK_RESPONSE_YES,
            "Compilar",
            GTK_RESPONSE_NO,
            "Ambos",
            GTK_RESPONSE_APPLY,
            NULL
        );
        
        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(choice_dialog));
        GtkWidget *label = gtk_label_new("Selecciona una opción para el archivo LaTeX:");
        gtk_container_add(GTK_CONTAINER(content_area), label);
        gtk_widget_show_all(choice_dialog);
        
        gint choice = gtk_dialog_run(GTK_DIALOG(choice_dialog));
        gtk_widget_destroy(choice_dialog);
        
        if (choice == GTK_RESPONSE_YES) {
            // Solo editar
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
        } else if (choice == GTK_RESPONSE_NO) {
            // Solo compilar
            compile_latex_file(last_selected_tex);
        } else if (choice == GTK_RESPONSE_APPLY) {
            // Editar y luego compilar
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
            
            // Preguntar después de editar si quiere compilar
            GtkWidget *compile_dialog = gtk_dialog_new_with_buttons(
                "Recompilar PDF",
                GTK_WINDOW(window1),
                GTK_DIALOG_MODAL,
                "Compilar ahora",
                GTK_RESPONSE_YES,
                "Después",
                GTK_RESPONSE_NO,
                NULL
            );
            
            GtkWidget *compile_content = gtk_dialog_get_content_area(GTK_DIALOG(compile_dialog));
            GtkWidget *compile_label = gtk_label_new("¿Deseas compilar el PDF ahora?");
            gtk_container_add(GTK_CONTAINER(compile_content), compile_label);
            gtk_widget_show_all(compile_dialog);
            
            gint compile_response = gtk_dialog_run(GTK_DIALOG(compile_dialog));
            gtk_widget_destroy(compile_dialog);
            
            if (compile_response == GTK_RESPONSE_YES) {
                compile_latex_file(last_selected_tex);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

// Función para compilar archivo latex
void compile_latex_file(const gchar *tex_file) {
    if (!tex_file || !g_file_test(tex_file, G_FILE_TEST_EXISTS)) {
        show_error("Archivo LaTeX no válido o no existe.");
        return;
    }
    
    gchar *dir = g_path_get_dirname(tex_file);
    gchar *base_name = g_path_get_basename(tex_file);
    
    // Compilar
    gchar *cmd = g_strdup_printf("cd \"%s\" && pdflatex -interaction=nonstopmode -halt-on-error \"%s\" > pdflatex_output.txt 2>&1", 
                                dir, base_name);
    int result = system(cmd);
    g_free(cmd);
    
    if (result == 0) {
        // Verificar si se creó el pdf
        gchar *pdf_file = g_strdup(tex_file);
        gchar *dot = strrchr(pdf_file, '.');
        if (dot) *dot = '\0';
        gchar *actual_pdf = g_strconcat(pdf_file, ".pdf", NULL);
        g_free(pdf_file);
        
        if (g_file_test(actual_pdf, G_FILE_TEST_EXISTS)) {
            // Abrir el pdf
            gchar *open_cmd = g_strdup_printf("evince --presentation \"%s\" 2>/dev/null &", actual_pdf);
            system(open_cmd);
            g_free(open_cmd);
            
            show_info("PDF compilado exitosamente.");
        } else {
            show_error("Se compiló pero no se generó el PDF. Revisa el archivo LaTeX.");
        }
        g_free(actual_pdf);
    } else {
        show_error("Error al compilar el archivo LaTeX. Revisa la sintaxis.");
    }
    
    g_free(dir);
    g_free(base_name);
}




void file_selected(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        return;
    }

    char line[2048];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }

    const char *delims = ",;\t\r\n";
    char *saveptr = NULL;

    char *colnames[512];
    int n = 0;

    for (char *token = strtok_r(line, delims, &saveptr);
         token != NULL;
         token = strtok_r(NULL, delims, &saveptr)) {
        colnames[n++] = g_strdup(g_strstrip(token));
    }
    if (n <= 0) { 
        fclose(f); 
        return; 
    }

    set_spin_value(GTK_SPIN_BUTTON(spinNodes), n);
    build_matrix_grid(scrollWindow, n); 

    syncing_headers = TRUE;
    for (int col = 1; col <= n; col++) {
        const char *name = colnames[col-1] ? colnames[col-1] : "";
        gtk_entry_set_text(GTK_ENTRY(g_ptr_array_index(col_headers, col)), name);
        gtk_entry_set_text(GTK_ENTRY(g_ptr_array_index(row_headers,  col)), name);
    }
    syncing_headers = FALSE;

    if (matrix) { for (int i=0;i<current_n;i++) free(matrix[i]); free(matrix); matrix=NULL; }
    current_n = n;
    matrix = malloc(n * sizeof(int*));
    for (int i = 0; i < n; i++) matrix[i] = calloc(n, sizeof(int));

    int row = 0;
    while (row < n && fgets(line, sizeof(line), f)) {
        char *saveptr2 = NULL;
        char *t = strtok_r(line, delims, &saveptr2);       
        const char *rname = t ? g_strstrip(t) : "";
        syncing_headers = TRUE;
        gtk_entry_set_text(GTK_ENTRY(g_ptr_array_index(row_headers, row+1)), rname);
        gtk_entry_set_text(GTK_ENTRY(g_ptr_array_index(col_headers, row+1)), rname);
        syncing_headers = FALSE;

        for (int col = 0; col < n; col++) {
            char *cell = strtok_r(NULL, delims, &saveptr2);
            int v = token_to_value(cell);
            if (row == col) v = 0;  

            matrix[row][col] = v;

            GtkWidget *entry = gtk_grid_get_child_at(GTK_GRID(current_grid), col+1, row+1);
            if (!entry) continue;
            if (v == INFINITO) gtk_entry_set_text(GTK_ENTRY(entry), "I");
            else {
                char buf[32]; snprintf(buf, sizeof(buf), "%d", v);
                gtk_entry_set_text(GTK_ENTRY(entry), buf);
            }
        }
        row++;
    }

    fclose(f);
    for (int i = 0; i < n; i++) if (colnames[i]) g_free(colnames[i]);
}

/*
GSignals (Botones)
*/


void on_createSolution_clicked (GtkWidget *createSolution, gpointer data){
    read_matrix_input();
    const gchar *filename_2 = gtk_entry_get_text(GTK_ENTRY(fileName));
    gchar *fname = set_extension(filename_2);
    gchar *dir2 = g_build_filename(g_get_current_dir(), "Saved_Problems", NULL);
    if (current_n <= 0 || matrix == NULL) {
        show_error("No hay datos de matriz para procesar.");
        return;
    }
    
    // Crear copia de la matriz para el algoritmo
    int **dist = malloc(current_n * sizeof(int*));
    int **path = malloc(current_n * sizeof(int*));
    
    for (int i = 0; i < current_n; i++) {
        dist[i] = malloc(current_n * sizeof(int));
        path[i] = malloc(current_n * sizeof(int));
        for (int j = 0; j < current_n; j++) {
            dist[i][j] = matrix[i][j];
            path[i][j] = -1;
        }
    }
    
    // Ejecutar algoritmo de Floyd
    floyd_algorithm(current_n, dist, path);
    
    // Generar reporte latex
    const gchar *raw = gtk_entry_get_text(GTK_ENTRY(fileName));
    gchar *base_name = g_strdup(raw);
    if (!base_name || !*base_name) {
        base_name = g_strdup("floyd_solution");
    }
    
    // Crear directorio para reportes
    gchar *dir = g_build_filename(g_get_current_dir(), "Reports", NULL);
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        show_error("No se pudo crear la carpeta de reportes.");
        g_free(dir);
        g_free(base_name);
        g_free(fname);
        return;
    }

    gchar *tex_file = g_build_filename(dir, g_strconcat(base_name, ".tex", NULL), NULL);
    gchar *pdf_file = g_build_filename(dir, g_strconcat(base_name, ".pdf", NULL), NULL);

    // Crear la imagen del grafo
    gchar *dotfile = g_build_filename(dir, "grafo.dot", NULL);
    gchar *pngfile = g_build_filename(dir, "grafo.png", NULL);
    
    if (!generate_graph_image(dotfile, pngfile, current_n, matrix, row_headers)) {
        show_error("No se pudo generar la imagen del grafo.");
    }
    
    if (generate_latex_report(tex_file, current_n, matrix, dist, path, row_headers, col_headers)) {
        // Compilar latex a PDF
        gchar *cmd = g_strdup_printf("cd \"%s\" && pdflatex -interaction=nonstopmode -halt-on-error \"%s\" > pdflatex_output.txt 2>&1", dir, tex_file);
        int result = system(cmd);
        g_free(cmd);
        
        // Verificar si se generó el pdf
        if (result == 0 && g_file_test(pdf_file, G_FILE_TEST_EXISTS)) {
            // Abrir el PDF con evince
            gchar *open_cmd = g_strdup_printf("evince --presentation \"%s\" 2>/dev/null &", pdf_file);
            int open_result = system(open_cmd);
            g_free(open_cmd);
            
            if (open_result != 0) {
                // Si evince falla, intenta abrir con otra app
                open_cmd = g_strdup_printf("xdg-open \"%s\" 2>/dev/null &", pdf_file);
                system(open_cmd);
                g_free(open_cmd);
            }
            
            show_info("Reporte generado y abierto exitosamente.");
        } else {
            // Leer el archivo de salida de pdflatex para diagnosticar el error
            gchar *output_file = g_build_filename(dir, "pdflatex_output.txt", NULL);
            if (g_file_test(output_file, G_FILE_TEST_EXISTS)) {
                GError *error = NULL;
                gchar *contents = NULL;
                gsize length = 0;
                
                if (g_file_get_contents(output_file, &contents, &length, &error)) {
                    gchar *last_lines = contents;
                    if (length > 500) {
                        last_lines = contents + length - 500;
                    }
                    
                    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window1),
                        GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                        "Error al compilar LaTeX. Últimas líneas del log:\n\n%s", last_lines);
                    gtk_dialog_run(GTK_DIALOG(dialog));
                    gtk_widget_destroy(dialog);
                    
                    g_free(contents);
                } else {
                    show_error("Error al compilar LaTeX. No se pudo leer el archivo de log.");
                    g_error_free(error);
                }
            } else {
                show_error("Error al compilar LaTeX. No se generó el archivo PDF.");
            }
            g_free(output_file);
        }
    } else {
        show_error("Error al generar el documento LaTeX.");
    }

    gchar *save_path = g_build_filename(dir2, fname, NULL);
    
    if (write_to_csv(save_path)) {
        gchar *base = g_path_get_basename(save_path);
        gtk_entry_set_text(GTK_ENTRY(fileName), base);
        g_free(base);
    }


    // Liberar memoria
    for (int i = 0; i < current_n; i++) {
        free(dist[i]);
        free(path[i]);
    }
    free(dist);
    free(path);
    g_free(dir);
    g_free(dir2);
    g_free(base_name);
    g_free(tex_file);
    g_free(pdf_file);
    g_free(dotfile);
    g_free(pngfile);
    g_free(save_path);
    g_free(fname);
}

void on_saveProblem_clicked (GtkWidget *saveProblem, gpointer data){
    read_matrix_input();

    if (filepath && *filepath) {
        if (write_to_csv(filepath)) {
            gchar *base = g_path_get_basename(filepath);
            gtk_entry_set_text(GTK_ENTRY(fileName), base);
            g_free(base);
        }
        return;
    }

    const gchar *filename_2 = gtk_entry_get_text(GTK_ENTRY(fileName));
    gchar *fname = set_extension(filename_2);
    gchar *dir = g_build_filename(g_get_current_dir(), "Saved_Problems", NULL);
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        g_free(dir); g_free(fname);
        return;
    }
    gchar *save_path = g_build_filename(dir, fname, NULL);
    if (write_to_csv(save_path)) {
        if (filepath) g_free(filepath);
        filepath = g_strdup(save_path);
        gchar *base = g_path_get_basename(save_path);
        gtk_entry_set_text(GTK_ENTRY(fileName), base);
        g_free(base);
    } 
    g_free(save_path); 
    g_free(dir); 
    g_free(fname);
}

G_MODULE_EXPORT void on_fileLoad_file_set (GtkWidget *loadProblem, gpointer data){
	gtk_widget_set_sensitive(loadToGrid, TRUE);
    gtk_widget_set_sensitive(saveProblem, TRUE);
}

// Función para cargar problema 
void on_loadToGrid_clicked (GtkWidget *loadToGrid, gpointer data){
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fileLoad));
    if (filename) {
        if (filepath) { 
            g_free(filepath); 
            filepath = NULL; 
        }
        //gtk_entry_set_text(GTK_ENTRY(fileName), "");
        filepath = g_strdup(filename);
        file_selected(filename);
        gchar *basename = g_path_get_basename(filename);
        gtk_entry_set_text(GTK_ENTRY(fileName), basename);
        g_free(filename);
        g_free(basename);
    }
}

//Función de acción para el botón de 'Exit' que cierra todo el programa.
void on_exitButton_clicked (GtkButton *exitButton1, gpointer data){
    gtk_main_quit();
    gtk_main_quit();
}

// Función para el botón de editar latex
void on_editLatex_clicked(GtkWidget *editLatex, gpointer data) {
    // Primero verificar si ya hay un archivo seleccionado 
    if (last_selected_tex && g_file_test(last_selected_tex, G_FILE_TEST_EXISTS)) {
        GtkWidget *dialog = gtk_dialog_new_with_buttons(
            "Archivo LaTeX encontrado",
            GTK_WINDOW(window1),
            GTK_DIALOG_MODAL,
            "Usar archivo anterior",
            GTK_RESPONSE_YES,
            "Seleccionar nuevo",
            GTK_RESPONSE_NO,
            NULL
        );
        
        gchar *message = g_strdup_printf("¿Deseas usar el archivo anterior?\n%s", last_selected_tex);
        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget *label = gtk_label_new(message);
        gtk_container_add(GTK_CONTAINER(content), label);
        gtk_widget_show_all(dialog);
        
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_free(message);
        
        if (response == GTK_RESPONSE_YES) {
            // Usar archivo anterior
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
            return;
        }
    }
    
    // Si no hay archivo anterior o el usuario quiere seleccionar nuevo
    on_select_latex_file(editLatex, data);
}

// Función para limpiar recursos
void cleanup_resources() {
    if (last_selected_tex) {
        g_free(last_selected_tex);
        last_selected_tex = NULL;
    }
}

//Main
int main (int argc, char *argv[]){
    gtk_init(&argc, &argv);
    
    builder =  gtk_builder_new_from_file ("Floyd.glade");
    
    window1 = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
    
    g_signal_connect(window1, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_builder_connect_signals(builder, NULL);
    
    fixed2 = GTK_WIDGET(gtk_builder_get_object(builder, "fixed2"));
    title = GTK_WIDGET(gtk_builder_get_object(builder, "title"));
    description = GTK_WIDGET(gtk_builder_get_object(builder, "description"));
    instruction = GTK_WIDGET(gtk_builder_get_object(builder, "instruction"));
    spinNodes = GTK_WIDGET(gtk_builder_get_object(builder, "spinNodes"));
    exitButton1 = GTK_WIDGET(gtk_builder_get_object(builder, "exitButton1"));
    fileLoad = GTK_WIDGET(gtk_builder_get_object(builder, "fileLoad"));
    loadLabel = GTK_WIDGET(gtk_builder_get_object(builder, "loadLabel"));
    scrollWindow = GTK_WIDGET(gtk_builder_get_object(builder, "scrollWindow"));
    createSolution = GTK_WIDGET(gtk_builder_get_object(builder, "createSolution"));
    fileName = GTK_WIDGET(gtk_builder_get_object(builder, "fileName"));
    loadToGrid = GTK_WIDGET(gtk_builder_get_object(builder, "loadToGrid"));
    saveProblem = GTK_WIDGET(gtk_builder_get_object(builder, "saveProblem"));
    editLatexButton = GTK_WIDGET(gtk_builder_get_object(builder, "editLatexButton"));


    cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider, "theme.css", NULL);

    set_css(cssProvider, window1);
    set_css(cssProvider, fileLoad);
    set_css(cssProvider, exitButton1);
    set_css(cssProvider, scrollWindow);
    set_css(cssProvider, createSolution);
    set_css(cssProvider, editLatexButton);

    g_signal_connect(editLatexButton, "clicked", G_CALLBACK(on_editLatex_clicked), NULL);
    g_signal_connect(exitButton1, "clicked", G_CALLBACK(on_exitButton_clicked), NULL);
    g_signal_connect(createSolution, "clicked", G_CALLBACK(on_createSolution_clicked), NULL);
    g_signal_connect(loadToGrid, "clicked", G_CALLBACK(on_loadToGrid_clicked), NULL);
    g_signal_connect(saveProblem, "clicked", G_CALLBACK(on_saveProblem_clicked), NULL);
    gtk_widget_set_sensitive(loadToGrid, FALSE);
    gtk_widget_set_sensitive(saveProblem, FALSE);
    
    
    gtk_widget_show(window1);
    
    gtk_main();

    return EXIT_SUCCESS;
}
