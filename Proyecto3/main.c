/*
MINI PROYECTO IO -PR00
Estudiantes:
Emily -
Viviana -
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

GtkWidget   *window;
GtkWidget   *fixed1;
GtkWidget   *button1;
GtkWidget   *button2;
GtkWidget   *button3;
GtkWidget   *button4;
GtkWidget   *label1;
GtkWidget   *label2;
GtkWidget   *exitButton;
GtkBuilder  *builder;
GtkWidget 	*image;
GtkCssProvider *cssProvider;
GtkWidget *pendingWindow;

/* 
Para ejecutar:
Abrir la terminal en el folder principal de Mini Proyecto.
Usar el comando: gcc main.c $(pkg-config --cflags --libs gtk+-3.0) -o main -export-dynamic
(Esto para que pueda correr con libgtk-3.0)
Ejecutar el main con el comando en terminal: ./main
También se puede hacer click en el archivo ejecutable 'Main' en la carpeta principal.
*/

//Función para que se utilice el archivo .css como proveedor de estilos.
void set_css (GtkCssProvider *cssProvider, GtkWidget *widget){
	GtkStyleContext *styleContext = gtk_widget_get_style_context(widget);
	gtk_style_context_add_provider(styleContext,GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

/*
Funciones para Pending Window
*/

//Muestra la pantalla de pending y carga sus widgets.
void* pending(){
	char * command = "gcc pending.c $(pkg-config --cflags --libs gtk+-3.0) -o pending -export-dynamic";
	system(command);
	system("./pending");
}

//Muestra la pantalla de pending y carga sus widgets.
void* floyd(){
	char * command = "gcc floyd.c $(pkg-config --cflags --libs gtk+-3.0) -lm -export-dynamic -o floyd";
	system(command);
	system("./floyd");
}

void* knapsack(){
	char * command = "gcc knapsack.c $(pkg-config --cflags --libs gtk+-3.0) -lm -export-dynamic -o knapsack";
	system(command);
	system("./knapsack");
}

void* ReemplazoEquipos(){
	char * command = "gcc ReemplazoEquipos.c $(pkg-config --cflags --libs gtk+-3.0) -lm -export-dynamic -o ReemplazoEquipos";
	system(command);
	system("./ReemplazoEquipos");
}

/*
Funciones para Main Window
*/

//Funciones de accion para los 4 botones de la pantalla principal. Todos llaman a pending ya que ninguno está asignado a un programa.

void on_button1_clicked (GtkWidget *button1, gpointer data){
	pthread_t thread;
	pthread_create(&thread, NULL, floyd, NULL);

}
void on_button2_clicked (GtkButton *button2, gpointer data){
	pthread_t thread;
	pthread_create(&thread, NULL, knapsack, NULL);
}
void on_button3_clicked (GtkButton *button3, gpointer data){
	pthread_t thread;
	pthread_create(&thread, NULL, ReemplazoEquipos, NULL);
}
void on_button4_clicked (GtkButton *button4, gpointer data){
	pthread_t thread;
	pthread_create(&thread, NULL, pending, NULL);
}

//Función de acción para el botón de 'Exit' que cierra todo el programa.
void on_exitButton_clicked (GtkButton *exitButton, gpointer data){
	gtk_main_quit();
}


//Main
int main (int argc, char *argv[]){
	gtk_init(&argc, &argv);
	
	builder =  gtk_builder_new_from_file ("MiniProyecto.glade");
	
	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	
	gtk_builder_connect_signals(builder, NULL);
	
	fixed1 = GTK_WIDGET(gtk_builder_get_object(builder, "fixed1"));
	button1 = GTK_WIDGET(gtk_builder_get_object(builder, "button1"));
	button2 = GTK_WIDGET(gtk_builder_get_object(builder, "button2"));
	button3 = GTK_WIDGET(gtk_builder_get_object(builder, "button3"));
	button4 = GTK_WIDGET(gtk_builder_get_object(builder, "button4"));
	label1 = GTK_WIDGET(gtk_builder_get_object(builder, "label1"));
	exitButton = GTK_WIDGET(gtk_builder_get_object(builder, "exitButton"));
	image = GTK_WIDGET(gtk_builder_get_object(builder, "image"));

	gtk_widget_set_tooltip_text(button1, "The Floyd-Warshall algorithm was created in 1962 by Robert Floyd, and it's an example of Dynammic Programming. It is a method to find the shortest paths between all pairs of nodes in a network.\nThe algorithm starts with a table called G(0) that shows the direct distances between each node. If two nodes are not directly connected, the table marks that distance as infinity. Then it checks if going through an extra node can make the path between two nodes shorter.\n The process is repeated until all possible intermediate nodes have been tested (meaning, there will be one G(k) table for each k node). In the end, the P table shows the shortest possible distance between every pair of nodes.\nWe could visualize these problems with distances between cities: What happens if I want to go directly from city A to city C? Would it be shorter if I go directly from A to C or if I go from A to B and from B to C?\nThe time complexity for the Floyd-Warshall algorithm is O(n³)");
	gtk_widget_set_tooltip_text(button2, "The Knapsack Problem is a combinatorial optimization problem that has been studied for more than a century, with early works dating as far back as 1897. Its purpose is to find a subset of items, each item with a specific cost (or weight) and value, to fit into a knapsack with a limited weight capacity. This way we can maximize the total value of the selected items without exceeding the knapsack's capacity.\nThere are 3 types of knapsack problems: \n1. 0/1 Knapsack: You can only take one of each item.\n2. Bounded: You can take a limited amount of items (1 or more). \n3. Unbounded: There is no limit to the amount of items you can take. The quantity is infinite.\nThe knapsack problem can be solved by exhaustive methods or using Dynamic Programming (HINT: Dynamic programming is the best option!).\nExample: I have bag with a capacity of 10kg, and I can steal a painting (5kg, $20), a sword (7kg, $35), a book (1kg, $5), a gold plate (2kg, $30) and a dog statue (4kg, $15). Which items should I take?");
	gtk_widget_set_tooltip_text(button3, "To Be Assigned");
	gtk_widget_set_tooltip_text(button4, "To Be Assigned");


	cssProvider = gtk_css_provider_new();
	gtk_css_provider_load_from_path(cssProvider, "theme.css", NULL);

	set_css(cssProvider, window);
	set_css(cssProvider, button1);
	set_css(cssProvider, button2);
	set_css(cssProvider, button3);
	set_css(cssProvider, button4);
	set_css(cssProvider, exitButton);

	g_signal_connect(exitButton, "clicked", G_CALLBACK(on_exitButton_clicked), NULL);
	g_signal_connect(button1, "clicked", G_CALLBACK(on_button1_clicked), NULL);
	g_signal_connect(button2, "clicked", G_CALLBACK(on_button2_clicked), NULL);
	g_signal_connect(button3, "clicked", G_CALLBACK(on_button3_clicked), NULL);
	g_signal_connect(button4, "clicked", G_CALLBACK(on_button4_clicked), NULL);
	
	gtk_widget_show(window);
	
	gtk_main();

	return EXIT_SUCCESS;
	}


