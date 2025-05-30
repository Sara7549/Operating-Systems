#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>
#include "GUI.h"
#include "scheduler.h"



// Dialog data structure - add this definition


// Callback function prototypes
gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);
void on_start_button_clicked(GtkButton *button, gpointer user_data);
void on_stop_button_clicked(GtkButton *button, gpointer user_data);
void on_step_button_clicked(GtkButton *button, gpointer user_data);
void on_reset_button_clicked(GtkButton *button, gpointer user_data);
void on_add_process_clicked(GtkButton *button, gpointer user_data);
void on_process_add_clicked(GtkButton *button, gpointer user_data);
void on_file_button_clicked(GtkButton *button, gpointer user_data);
void on_file_selected(GObject *source_object, GAsyncResult *res, gpointer user_data);
gboolean on_timer_tick(gpointer user_data);
void on_algorithm_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data);
void on_quantum_changed(GtkSpinButton *spin_button, gpointer user_data);
// Add these to callbacks.h
void on_input_dialog_cancel(GtkButton *button, gpointer user_data);
void on_input_dialog_ok(GtkButton *button, gpointer user_data);
// Add these prototypes
gboolean continue_simulation_after_input(gpointer user_data);
void on_input_dialog_ok(GtkButton *button, gpointer user_data);
void on_input_dialog_cancel(GtkButton *button, gpointer user_data);



#endif // CALLBACKS_H