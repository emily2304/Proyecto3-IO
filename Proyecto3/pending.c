#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <math.h>
#include <ctype.h>


GtkCssProvider *cssProvider;
GtkWidget *pendingWindow;
GtkBuilder  *builder;
GtkWidget *plabel1;
GtkWidget *plabel2;
GtkWidget *pbutton1;  

//Función para que se utilice el archivo .css como proveedor de estilos.
void set_css (GtkCssProvider *cssProvider, GtkWidget *widget){
	GtkStyleContext *styleContext = gtk_widget_get_style_context(widget);
	gtk_style_context_add_provider(styleContext,GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

//Cuando se hace click en el botón de 'Close Program', se hace el window no visible para que no borre la instancia de la ventana
void on_pbutton1_clicked(GtkButton *exitButton, gpointer data){
    gtk_main_quit();
	
}

int main (int argc, char *argv[]){
    gtk_init(&argc, &argv);
	
	builder =  gtk_builder_new_from_file ("Pending.glade");
	
	pendingWindow = GTK_WIDGET(gtk_builder_get_object(builder, "pendingWindow"));
	
	g_signal_connect(pendingWindow, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_builder_connect_signals(builder, NULL);

    plabel1 = GTK_WIDGET(gtk_builder_get_object(builder, "plabel1"));
	plabel2 = GTK_WIDGET(gtk_builder_get_object(builder, "plabel2"));
	pbutton1 = GTK_WIDGET(gtk_builder_get_object(builder, "pbutton1"));

    cssProvider = gtk_css_provider_new();
	gtk_css_provider_load_from_path(cssProvider, "theme.css", NULL);

    set_css(cssProvider, pendingWindow);
	set_css(cssProvider, pbutton1);

    g_signal_connect(pbutton1, "clicked", G_CALLBACK(on_pbutton1_clicked), NULL);

    gtk_widget_show(pendingWindow);
	
	gtk_main();

}
