#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "callbacks.h"


// Make sure this full structure is defined before any usage
// Update the SchedulerGUI struct to include the new panels

typedef struct {
    GtkWidget *window;
    GtkWidget *process_grid;
    GtkWidget *memory_grid;
    GtkWidget *log_view;
    GtkTextBuffer *log_buffer;
    GtkWidget *resource_panel;
    GtkWidget *overview_panel;  // Add this
    GtkWidget *queue_panel;     // Add this
    GtkWidget *algorithm_combo;
    GtkWidget *quantum_spin;
    GtkWidget *start_button;
    GtkWidget *stop_button;
    GtkWidget *step_button;
    GtkWidget *reset_button;
    GtkWidget *add_process_button;
    GtkWidget *status_bar;
    gboolean is_running;
    gboolean is_paused;
    guint timer_id;
    GtkWidget *input_dialog;  // Dialog for variable input
    GtkWidget *output_text_view;
    GtkTextBuffer *output_buffer;
    GtkWidget *mlfq_frame;   
    GtkWidget *ready_frame;
} SchedulerGUI;

// Add the function prototypes
void update_overview_panel(SchedulerGUI *gui);
void update_queue_panel(SchedulerGUI *gui);

typedef struct {
    SchedulerGUI *gui;
    GtkWidget *dialog;
    GtkWidget *arrival_spin;
    GtkWidget *priority_spin;
    char **filename;
} ProcessDialogData;

typedef struct {
    SchedulerGUI *gui;
    GtkWidget *dialog;
    GtkWidget *entry;
} InputDialogData;

// Function to log messages to the GUI
void log_message(SchedulerGUI *gui, const char *message);
void gui_run_full_simulation(SchedulerGUI *gui);
void update_gui(SchedulerGUI *gui);
void update_process_list(SchedulerGUI *gui);
void update_overview_panel(SchedulerGUI *gui);
void update_queue_panel(SchedulerGUI *gui);
// Add this to GUI.h after other function declarations
void show_input_dialog(const char *var_name, bool numeric_only);
void add_output_message(SchedulerGUI *gui, const char *message);




#endif /* GUI_H */