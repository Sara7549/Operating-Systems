#include <gtk/gtk.h>
#include "callbacks.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>     
#include "GUI.h"
#include <ctype.h>



// Window close handler
gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
    // Free any resources and allow window to close
    return FALSE;
}

// Start simulation
void on_start_button_clicked(GtkButton *button, gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;

    if (!gui) {
        fprintf(stderr, "Error: GUI pointer is NULL\n");
        return;
    }
    if (!gui->is_running) {
        // Initialize simulation if it's the first start
        if (currentTime == -1) { // Check if simulation has not been initialized
            initialize_simulation(); // Call initialization function
            currentTime = 0;
            if (currentTime == 0) {  // Check if initialization was successful
                log_message(gui, "Simulation initialized");
            } else {
                log_message(gui, "Failed to initialize simulation");
                return;
            }
        }

        // Start the simulation
        run_full_simulation(); // Call simulation function
        gui->is_running = true; // Assume simulation is running once called
        log_message(gui, "Simulation started");
    }
}

// Stop/pause simulation
void on_stop_button_clicked(GtkButton *button, gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;
    
    if (gui->is_running && !gui->is_paused) {
        gui->is_paused = TRUE;
        
        // Remove the timer
        if (gui->timer_id != 0) {
            g_source_remove(gui->timer_id);
            gui->timer_id = 0;
        }
        
        // Update UI state
        gtk_widget_set_sensitive(gui->step_button, TRUE); // Enable step button when paused
        
        log_message(gui, "Simulation paused");
    } else if (gui->is_running && gui->is_paused) {
        // Resume
        gui->is_paused = FALSE;
        
        // Restart timer
        if (gui->timer_id == 0) {
            gui->timer_id = g_timeout_add(1000, on_timer_tick, gui);
        }
        
        // Update UI state
        gtk_widget_set_sensitive(gui->step_button, FALSE); // Disable step button when running
        
        log_message(gui, "Simulation resumed");
    }
}

// Execute one step of the simulation
void on_step_button_clicked(GtkButton *button, gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;

    if (!gui) {
        fprintf(stderr, "Error: GUI pointer is NULL\n");
        return;
    }
    
    if (processTable == NULL || numProcesses == 0) {
        log_message(gui, "Error: No processes loaded. Add processes first.");
        return;
    }
    
    // Execute one simulation step
    if (all_processes_complete()) {
        log_message(gui, "Simulation complete - no more steps to execute");
        return;
    }
    
    // If this is the first step, ensure clock starts at 0
    if (currentTime == -1) {
        currentTime = 0;
    }
    
    printf("Before executing step, current time: %d\n", currentTime);
    execute_step();
    printf("After executing step, current time: %d\n", currentTime);
    
    // Update all GUI elements
    // update_gui(gui);

    // Update status bar with current time
    char status_text[64];
    snprintf(status_text, sizeof(status_text), "Time: %d", currentTime);
    gtk_label_set_text(GTK_LABEL(gui->status_bar), status_text);
    
    // Log what happened
    // char message[100];
    // snprintf(message, sizeof(message), "Executed step at time %d", currentTime - 1);
    // log_message(gui, message);
}

// Reset simulation to initial state
void on_reset_button_clicked(GtkButton *button, gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;

    if (!gui) {
        fprintf(stderr, "Error: GUI pointer is NULL\n");
        return;
    }
    
    // Stop timer if running
    if (gui->timer_id != 0) {
        g_source_remove(gui->timer_id);
        gui->timer_id = 0;
    }
    
    // Reset state flags
    gui->is_running = FALSE;
    gui->is_paused = FALSE;
    
    // Reset the scheduler
    reset_scheduler();

     // Clear the execution log by clearing the text buffer
     if (gui->log_buffer) {
        gtk_text_buffer_set_text(gui->log_buffer, "", -1);
    }
    
    // Clear the output area
    if (gui->output_buffer) {
        gtk_text_buffer_set_text(gui->output_buffer, "", -1);
    }
    
    // Update UI state
    gtk_widget_set_sensitive(gui->step_button, TRUE);
    gtk_widget_set_sensitive(gui->start_button, TRUE);
    gtk_widget_set_sensitive(gui->algorithm_combo, TRUE);
    gtk_widget_set_sensitive(gui->quantum_spin, 
    gtk_drop_down_get_selected(GTK_DROP_DOWN(gui->algorithm_combo)) != 0);
    gtk_widget_set_sensitive(gui->add_process_button, TRUE);
    
    // Update the process and memory tables
    update_gui(gui);

    // Reset the status bar
    if (gui->status_bar) {
        gtk_label_set_text(GTK_LABEL(gui->status_bar), "Time: 0");
    }
    
    log_message(gui, "Simulation reset");
}

void on_input_dialog_ok(GtkButton *button, gpointer user_data) {
    extern SchedulerGUI *gui;  // Use the global gui variable
    
    GtkEntry *entry = GTK_ENTRY(user_data);
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    GtkWidget *dialog = gtk_widget_get_ancestor(GTK_WIDGET(button), GTK_TYPE_WINDOW);
    
    if (!dialog || !text) {
        fprintf(stderr, "Error: Invalid dialog or text\n");
        return;
    }
    
    // Check if input is valid (for numeric variables)
    bool valid = true;
    if (pending_input_var[0] >= 'a' && pending_input_var[0] <= 'z') {
        // Numeric validation
        valid = true;
        for (int i = 0; text[i] != '\0'; i++) {
            if (i == 0 && text[i] == '-') continue;
            if (!isdigit(text[i])) {
                valid = false;
                break;
            }
        }
    }
    
    if (valid) {
        // Apply the input value
        setVariable(pending_input_var, text);
        
        // Log the action safely
        if (gui && gui->log_buffer) {
            char message[200];
            snprintf(message, sizeof(message), "Set variable %s = %s", pending_input_var, text);
            log_message(gui, message);
        }
        
        // Update memory for the variable with safety checks
        if (pending_input_process != NULL) {
            int found = 0;
            int start = pending_input_process->lowerMemoryBound;
            int end = pending_input_process->upperMemoryBound - 6;
            
            // Bounds check to prevent segfault
            if (start >= 0 && end < memorySize && start < end) {
                for (int i = start + (end - start - 3); i < end; i++) {
                    if (i >= 0 && i < memorySize && memory[i].name != NULL) {
                        if ((strcmp(memory[i].name, "Variable") == 0 && 
                             memory[i].value != NULL && strcmp(memory[i].value, "NULL") == 0) ||
                            (strcmp(memory[i].name, pending_input_var) == 0)) {
                            
                            // Update variable name if needed
                            if (strcmp(memory[i].name, "Variable") == 0) {
                                char *new_name = strdup(pending_input_var);
                                if (new_name) {
                                    free(memory[i].name);
                                    memory[i].name = new_name;
                                }
                            }
                            
                            // Update variable value
                            if (memory[i].value != NULL) {
                                free(memory[i].value);
                            }
                            
                            memory[i].value = strdup(text);
                            found = 1;
                            break;
                        }
                    }
                }
            }
            
            if (!found && gui && gui->log_buffer) {
                log_message(gui, "Warning: Variable location not found in memory");
            }
        }
        
        // Reset input state
        waiting_for_input = false;
        pending_input_var[0] = '\0';
        pending_input_process = NULL;
        
        // Close the dialog
        gtk_window_destroy(GTK_WINDOW(dialog));
        
        // Update GUI safely
        if (gui) {
            update_gui(gui);
            
            // Continue execution
            if (gui->is_running && !gui->is_paused) {
                if (gui->timer_id == 0) {
                    gui->timer_id = g_timeout_add(1000, on_timer_tick, gui);
                }
            }
        }
    } else {
        // Show error message for invalid input
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
                                                       GTK_DIALOG_MODAL,
                                                       GTK_MESSAGE_ERROR,
                                                       GTK_BUTTONS_OK,
                                                       "Please enter a valid integer value for %s",
                                                       pending_input_var);
        gtk_window_set_title(GTK_WINDOW(error_dialog), "Input Error");
        gtk_widget_set_visible(error_dialog, TRUE);
        g_signal_connect(error_dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    }
}
void on_input_dialog_cancel(GtkButton *button, gpointer user_data) {
    InputDialogData *data = (InputDialogData *)user_data;
    SchedulerGUI *gui = data->gui;
    GtkWidget *dialog = data->dialog;
    
    // Reset input state BEFORE dialog is destroyed
    waiting_for_input = false;
    pending_input_var[0] = '\0';
    pending_input_process = NULL;
    
    // Close the dialog
    gtk_window_destroy(GTK_WINDOW(dialog));
    
    // Log safely
    if (gui && gui->log_buffer) {
        log_message(gui, "User input cancelled");
    }
}

// Add this function to safely continue simulation after input
gboolean continue_simulation_after_input(gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;
    
    if (!gui || !gui->is_running || gui->is_paused) {
        return FALSE;
    }
    
    // Call run_full_simulation again to continue the simulation
    run_full_simulation();
    
    // This is a one-time call, so return FALSE to remove the source
    return FALSE;
}

gboolean continue_step_after_input(gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;
    
    if (!gui || !gui->is_running || gui->is_paused) {
        return FALSE;
    }
    
    // Call run_full_simulation again to continue the simulation
    execute_step();
    
    // This is a one-time call, so return FALSE to remove the source
    return FALSE;
}

// Handle algorithm selection change
void on_algorithm_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    
    // Enable time quantum spinbutton only for RR and MLFQ
    gboolean need_quantum = (selected != 0); // 0 is FCFS
    gtk_widget_set_sensitive(gui->quantum_spin, need_quantum);
    
    // Update the scheduler algorithm
    set_scheduler_algorithm(selected);
    
    // Log the change
    const char *algo_names[] = {"FCFS", "Round Robin", "MLFQ"};
    char message[100];
    snprintf(message, sizeof(message), "Algorithm changed to %s", algo_names[selected]);
    log_message(gui, message);
}

// Handle quantum value change
void on_quantum_changed(GtkSpinButton *spin_button, gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;
    int quantum = gtk_spin_button_get_value_as_int(spin_button);
    
    // Update the scheduler time quantum
    set_scheduler_quantum(quantum);
    
    // Log the change
    char message[100];
    snprintf(message, sizeof(message), "Time quantum set to %d", quantum);
    log_message(gui, message);
}

// Timer callback for automatic simulation steps
gboolean on_timer_tick(gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;
    
    if (gui->is_running && !gui->is_paused) {
        // If this is the first step, ensure clock starts at 0
        if (currentTime == -1) {
            currentTime = 0;
        }
        //test
        check_for_process_arrivals(currentTime);
        // Execute one step of the simulation
        int result = execute_step();
        
        // Update the UI
        update_gui(gui);
        
        // If simulation is complete, stop timer
        if (result == 0) {
            log_message(gui, "Simulation complete");
            gui->is_running = FALSE;
            gui->timer_id = 0;
            
            // Re-enable controls
            gtk_widget_set_sensitive(gui->step_button, FALSE);
            gtk_widget_set_sensitive(gui->algorithm_combo, TRUE);
            gtk_widget_set_sensitive(gui->quantum_spin, 
            gtk_drop_down_get_selected(GTK_DROP_DOWN(gui->algorithm_combo)) != 0);
            gtk_widget_set_sensitive(gui->add_process_button, TRUE);
        }
    }
    
    return G_SOURCE_CONTINUE;
}

// Legacy process params handler
void on_process_params_ok(GtkButton *button, gpointer user_data) {
    // This function is kept for backward compatibility
    // But functionality has moved to on_process_add_clicked
    g_warning("on_process_params_ok called - this function is deprecated");
}

// Legacy file selection callback
void add_process_file_selected_cb(GtkFileDialog *dialog, GAsyncResult *res, gpointer user_data) {
    // This function is kept for backward compatibility
    // But functionality has moved to on_file_selected
    g_warning("add_process_file_selected_cb called - this function is deprecated");
}

// Function to safely load a process file
bool load_process_file(const char *filepath, int arrival_time) {
    if (!filepath) {
        fprintf(stderr, "Invalid file path\n");
        return false;
    }
    
    FILE *file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filepath);
        return false;
    }
    
    // Add process to the scheduler - removed priority parameter
    int pid = add_process(filepath, arrival_time);
    if (pid < 0) {
        fprintf(stderr, "Failed to add process from file: %s\n", filepath);
        fclose(file);
        return false;
    }
    
    fclose(file);
    return true;
}

// File button click handler for the process creation dialog
void on_file_button_clicked(GtkButton *button, gpointer user_data) {
    ProcessDialogData *data = (ProcessDialogData *)user_data;
    
    GtkFileDialog *file_dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(file_dialog, "Select Process File");
    
    // Set up file filters for .txt files
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_filter_set_name(filter, "Text Files (*.txt)");
    g_list_store_append(filters, filter);
    g_object_unref(filter);
    
    gtk_file_dialog_set_filters(file_dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
    
    // Use correct callback signature for GTK4
    gtk_file_dialog_open(file_dialog, GTK_WINDOW(data->dialog), NULL, 
                        (GAsyncReadyCallback)on_file_selected, data);
    g_object_unref(file_dialog);
}

// File selection callback with correct GObject signature
void on_file_selected(GObject *source, GAsyncResult *result, gpointer user_data) {
    ProcessDialogData *data = (ProcessDialogData *)user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_open_finish(dialog, result, NULL);
    
    if (file) {
        // Free previous filename if it exists
        if (*(data->filename)) {
            g_free(*(data->filename));
        }
        
        // Set the new filename
        *(data->filename) = g_file_get_path(file);
        g_object_unref(file);
        
        // Find the file path label and update it
        GtkWidget *box = gtk_widget_get_first_child(GTK_WIDGET(data->dialog));
        if (box) {
            GtkWidget *grid = gtk_widget_get_first_child(box);
            if (GTK_IS_GRID(grid)) {
                GtkWidget *child = gtk_grid_get_child_at(GTK_GRID(grid), 0, 1);
                if (GTK_IS_LABEL(child)) {
                    gtk_label_set_text(GTK_LABEL(child), *(data->filename));
                }
            }
        }
    }
}

// Process creation dialog
void on_add_process_clicked(GtkButton *button, gpointer user_data) {
    SchedulerGUI *gui = (SchedulerGUI *)user_data;
    
    // Create a modern GTK4 dialog
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Process Parameters");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gui->window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 200);
    
    // Create dialog content
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    
    // File selection button
    GtkWidget *file_label = gtk_label_new("Process File:");
    gtk_widget_set_halign(file_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), file_label, 0, 0, 1, 1);
    
    GtkWidget *file_button = gtk_button_new_with_label("Select File");
    gtk_grid_attach(GTK_GRID(grid), file_button, 1, 0, 1, 1);
    
    GtkWidget *file_path_label = gtk_label_new("No file selected");
    gtk_widget_set_hexpand(file_path_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), file_path_label, 0, 1, 2, 1);
    
    // Arrival time
    GtkWidget *arrival_label = gtk_label_new("Arrival Time:");
    gtk_widget_set_halign(arrival_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), arrival_label, 0, 2, 1, 1);
    
    GtkWidget *arrival_spin = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_grid_attach(GTK_GRID(grid), arrival_spin, 1, 2, 1, 1);
    
    // Removed priority UI elements
    
    // Button box
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(button_box, 10);
    
    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    gtk_box_append(GTK_BOX(button_box), cancel_button);
    
    GtkWidget *add_button = gtk_button_new_with_label("Add");
    gtk_box_append(GTK_BOX(button_box), add_button);
    
    gtk_box_append(GTK_BOX(box), grid);
    gtk_box_append(GTK_BOX(box), button_box);
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    
    // Data structure to hold the file path - persistent through dialog lifetime
    char **selected_file_path = g_new(char *, 1);
    *selected_file_path = NULL;
    
    // Store dialog data
    ProcessDialogData *dialog_data = g_new(ProcessDialogData, 1);
    dialog_data->gui = gui;
    dialog_data->dialog = dialog;
    dialog_data->arrival_spin = arrival_spin;
    //dialog_data->priority_spin = NULL; // Removed priority spin
    dialog_data->filename = selected_file_path;
    
    // Connect signals
    g_signal_connect(file_button, "clicked", G_CALLBACK(on_file_button_clicked), dialog_data);
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_process_add_clicked), dialog_data);
    
    // Free dialog_data when dialog is destroyed
    g_object_set_data_full(G_OBJECT(dialog), "dialog-data", dialog_data, 
                          (GDestroyNotify)g_free);
    
    // Show the dialog
    gtk_widget_set_visible(dialog, TRUE);
}

// Process add button handler
void on_process_add_clicked(GtkButton *button, gpointer user_data) {
    ProcessDialogData *data = (ProcessDialogData *)user_data;
    
    if (data->filename && *(data->filename)) {
        int arrival = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->arrival_spin));
        
        // Use our modified function to load the process file (without priority)
        if (load_process_file(*(data->filename), arrival)) {
            char message[256];
            snprintf(message, sizeof(message), 
                     "Added process from file %s with arrival time %d",
                     *(data->filename), arrival);
            log_message(data->gui, message);
            
            // Update the GUI after successfully adding process
            update_gui(data->gui);
        }
        
        // Free the filename
        g_free(*(data->filename));
        *(data->filename) = NULL;
    }
    
    // Close the dialog - dialog_data will be freed by the destroy notify function
    gtk_window_destroy(GTK_WINDOW(data->dialog));
}