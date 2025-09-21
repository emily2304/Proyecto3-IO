/* Proyecto 2 IO - Reemplazo de Equipos
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
GtkCssProvider *cssProvider;

// --- Variables globales ---
double **dp_equipo;
int **decision_equipo;


void set_css(GtkCssProvider *cssProvider, GtkWidget *widget);

//Función para que se utilice el archivo .css como proveedor de estilos.
void set_css(GtkCssProvider *cssProvider, GtkWidget *widget) {
    GtkStyleContext *styleContext = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(styleContext, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
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



// --- Callbacks faltantes (placeholder) ---
void on_select_latex_file_reemplazo(GtkButton *button, gpointer data) {
    g_print("Botón seleccionar archivo LaTeX presionado\n");
}

void on_exit_reemplazo_clicked(GtkButton *button, gpointer data) {
    g_print("Botón salir presionado\n");
    gtk_main_quit();
}

void on_calc_reemplazo_clicked(GtkButton *button, gpointer data) {
    g_print("Botón calcular reemplazo presionado\n");
}

void on_vida_changed(GtkSpinButton *spin, gpointer data) {
    g_print("Vida cambiada: %d\n", gtk_spin_button_get_value_as_int(spin));
}

void on_plazo_changed(GtkSpinButton *spin, gpointer data) {
    g_print("Plazo cambiado: %d\n", gtk_spin_button_get_value_as_int(spin));
}

// Inicializar tabla de equipo (placeholder)
void init_equipo_table(int vida) {
    g_print("Inicializando tabla de equipo con vida = %d\n", vida);
}

void on_load_to_grid_reemplazo(GtkButton *button, gpointer data) {
    g_print("Load to grid clicked\n");
}

void on_save_reemplazo_clicked(GtkButton *button, gpointer data) {
    g_print("Save reemplazo clicked\n");
}

void on_file_load_reemplazo(GtkFileChooserButton *button, gpointer data) {
    g_print("File load reemplazo clicked\n");
}

// Función para inicializar la ventana (similar a init_knapsack)
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
    g_signal_connect(plazoSpin, "value-changed", G_CALLBACK(on_plazo_changed), NULL);
    gtk_widget_set_sensitive(loadToGridReemplazo, FALSE);
    gtk_widget_set_sensitive(saveReemplazo, FALSE);
    
    init_equipo_table(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vidaSpin)));
    
    gtk_widget_show_all(windowReemplazo);
    
    gtk_main();

    return EXIT_SUCCESS;
}