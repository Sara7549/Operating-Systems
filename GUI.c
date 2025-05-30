#include <gtk/gtk.h>
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
#include "callbacks.h"
#include "scheduler.h"

// Global application
GtkApplication *app;
SchedulerGUI *gui;

// Setup function for label factories
void setup_label_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0); // Left align text
    gtk_list_item_set_child(list_item, label);
}

// Create the process table section
// Create simple process display grid
static GtkWidget* create_process_table() {
    GtkWidget *frame = gtk_frame_new("Process Table");
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Create a grid for process display
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    // Add header row (removed "Priority")
    const char *headers[] = {"PID", "Filename", "Arrival Time", "State","PC", "CPU Time", "Waiting Time","Memory Bounds"};
    for (int i = 0; i < 8; i++) {  // Changed from 7 to 6 columns
        GtkWidget *label = gtk_label_new(headers[i]);
        gtk_widget_add_css_class(label, "header");
        gtk_grid_attach(GTK_GRID(grid), label, i, 0, 1, 1);
    }
    
    // Create empty rows for processes
    for (int row = 1; row <= MAX_PROCESSES; row++) {
        for (int col = 0; col < 8; col++) {  // Changed from 7 to 6 columns
            GtkWidget *label = gtk_label_new("");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_widget_set_hexpand(label, TRUE);
            gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
        }
    }
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), grid);
    gtk_frame_set_child(GTK_FRAME(frame), scrolled);
    
    gui->process_grid = grid;
    return frame;
}

// Create memory view grid
static GtkWidget* create_memory_view() {
    GtkWidget *frame = gtk_frame_new("Memory View");
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Create grid for memory display
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    // Add header row
    const char *headers[] = {"Index", "PID", "Name", "Value"};
    for (int i = 0; i < 4; i++) {
        GtkWidget *label = gtk_label_new(headers[i]);
        gtk_widget_add_css_class(label, "header");
        gtk_grid_attach(GTK_GRID(grid), label, i, 0, 1, 1);
    }
    
    // Create empty rows for memory cells (will be filled in update_memory_view)
    for (int row = 1; row <= 60; row++) {
        for (int col = 0; col < 4; col++) {
            GtkWidget *label = gtk_label_new("");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_widget_set_hexpand(label, TRUE);
            gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
        }
    }
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), grid);
    gtk_frame_set_child(GTK_FRAME(frame), scrolled);
    
    // Store the grid for later updates
    gui->memory_grid = grid;
    
    return frame;
}

// Function to get filename from path
char* get_basename(const char* path) {
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        last_slash = strrchr(path, '\\');
    }
    
    if (last_slash) {
        return (char*)(last_slash + 1);  // Return filename part
    }
    return (char*)path;  // No slash found, return the whole path
}

// Update process list display
void update_process_list(SchedulerGUI *gui) {
    GtkWidget *grid = gui->process_grid;
    if (!grid) return;
    
    // Clear existing rows first
    for (int row = 1; row <= MAX_PROCESSES; row++) {
        for (int col = 0; col < 8; col++) {  // Changed from 7 to 6 columns
            GtkWidget *label = gtk_grid_get_child_at(GTK_GRID(grid), col, row);
            if (GTK_IS_LABEL(label)) {
                gtk_label_set_text(GTK_LABEL(label), "");
            }
        }
    }
    
    // Add process entries
    for (int i = 0; i < numProcesses; i++) {
        int row = i + 1;
        char buffer[64];
        
        // PID
        snprintf(buffer, sizeof(buffer), "%d", processTable[i].id);
        GtkWidget *pid_label = gtk_grid_get_child_at(GTK_GRID(grid), 0, row);
        gtk_label_set_text(GTK_LABEL(pid_label), buffer);
        
        // Filename - show basename only
        char *basename = get_basename(processTable[i].filename);
        GtkWidget *filename_label = gtk_grid_get_child_at(GTK_GRID(grid), 1, row);
        gtk_label_set_text(GTK_LABEL(filename_label), basename);
        
        // Arrival Time
        snprintf(buffer, sizeof(buffer), "%d", processTable[i].arrivalTime);
        GtkWidget *arrival_label = gtk_grid_get_child_at(GTK_GRID(grid), 2, row);
        gtk_label_set_text(GTK_LABEL(arrival_label), buffer);
        
        // State
        char *state = "NEW";
        if (processTable[i].pcb) {
            state = processTable[i].pcb->processState;
        } else if (processTable[i].isComplete) {
            state = "TERMINATED";
        }
        GtkWidget *state_label = gtk_grid_get_child_at(GTK_GRID(grid), 3, row);
        gtk_label_set_text(GTK_LABEL(state_label), state);
        
        // CPU Time
        snprintf(buffer, sizeof(buffer), "%d", processTable[i].executedTime);
        GtkWidget *cputime_label = gtk_grid_get_child_at(GTK_GRID(grid), 4, row);
        gtk_label_set_text(GTK_LABEL(cputime_label), buffer);
        
        // Waiting Time
        snprintf(buffer, sizeof(buffer), "%d", processTable[i].waitingTime);
        GtkWidget *waittime_label = gtk_grid_get_child_at(GTK_GRID(grid), 5, row);
        gtk_label_set_text(GTK_LABEL(waittime_label), buffer);

        if (processTable[i].pcb)
        snprintf(buffer, sizeof(buffer), "%d", processTable[i].pcb->programCounter);
    else
        strcpy(buffer, "-");
    gtk_label_set_text(GTK_LABEL(gtk_grid_get_child_at(GTK_GRID(grid), 6, row)), buffer);

    if (processTable[i].pcb)
    snprintf(buffer, sizeof(buffer), "%d", processTable[i].pcb->upperMemoryBound-processTable[i].pcb->lowerMemoryBound);
    else
        strcpy(buffer, "-");
    gtk_label_set_text(GTK_LABEL(gtk_grid_get_child_at(GTK_GRID(grid), 7, row)), buffer);
    }
}

// Update memory view display
void update_memory_view(SchedulerGUI *gui) {
    GtkWidget *grid = gui->memory_grid;
    if (!grid) return;
    
    // Update memory cells
    for (int i = 0; i < memorySize; i++) {
        int row = i + 1;
        char buffer[64];
        
        // Index
        snprintf(buffer, sizeof(buffer), "%d", i);
        GtkWidget *index_label = gtk_grid_get_child_at(GTK_GRID(grid), 0, row);
        gtk_label_set_text(GTK_LABEL(index_label), buffer);
        
        // PID
        snprintf(buffer, sizeof(buffer), "%d", memory[i].processID);
        GtkWidget *pid_label = gtk_grid_get_child_at(GTK_GRID(grid), 1, row);
        gtk_label_set_text(GTK_LABEL(pid_label), buffer);
        
        // Name
        GtkWidget *name_label = gtk_grid_get_child_at(GTK_GRID(grid), 2, row);
        gtk_label_set_text(GTK_LABEL(name_label), memory[i].name ? memory[i].name : "NULL");
        
        // Value
        GtkWidget *value_label = gtk_grid_get_child_at(GTK_GRID(grid), 3, row);
        gtk_label_set_text(GTK_LABEL(value_label), memory[i].value ? memory[i].value : "NULL");
    }
}

// Create the execution log section
static GtkWidget* create_log_view() {
    GtkWidget *frame = gtk_frame_new("Execution Log");
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Create text view for the log
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    
    // Create text buffer
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    
    // Set up tag for highlighted text
    gtk_text_buffer_create_tag(buffer, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);
    gtk_frame_set_child(GTK_FRAME(frame), scrolled_window);
    
    // Save references
    gui->log_view = text_view;
    gui->log_buffer = buffer;
    
    return frame;
}

// Create the control panel
static GtkWidget* create_control_panel() {
    GtkWidget *frame = gtk_frame_new("Controls");
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_margin_start(grid, 10);
    gtk_widget_set_margin_end(grid, 10);
    gtk_widget_set_margin_top(grid, 10);
    gtk_widget_set_margin_bottom(grid, 10);
    
    // Algorithm selection
    GtkWidget *algo_label = gtk_label_new("Scheduler Algorithm:");
    gtk_widget_set_halign(algo_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), algo_label, 0, 0, 1, 1);
    
    // Create dropdown for algorithm selection
    const char *algorithm_names[] = {"FCFS", "Round Robin", "MLFQ", NULL};
    GtkStringList *string_list = gtk_string_list_new(algorithm_names);
    
    GtkWidget *algo_combo = gtk_drop_down_new(G_LIST_MODEL(string_list), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(algo_combo), 0); // Default to FCFS
    gtk_grid_attach(GTK_GRID(grid), algo_combo, 1, 0, 1, 1);
    g_signal_connect(algo_combo, "notify::selected", G_CALLBACK(on_algorithm_changed), gui);
    
    // Time quantum for RR and MLFQ
    GtkWidget *quantum_label = gtk_label_new("Time Quantum:");
    gtk_widget_set_halign(quantum_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), quantum_label, 0, 1, 1, 1);
    
    GtkWidget *quantum_spin = gtk_spin_button_new_with_range(1, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(quantum_spin), 2); // Default quantum
    gtk_widget_set_sensitive(quantum_spin, FALSE); // Disabled initially (FCFS)
    gtk_grid_attach(GTK_GRID(grid), quantum_spin, 1, 1, 1, 1);
    g_signal_connect(quantum_spin, "value-changed", G_CALLBACK(on_quantum_changed), gui);
    
    // Button box for process control
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    // Add Process button
    GtkWidget *add_button = gtk_button_new_with_label("Add Process");
    gtk_box_append(GTK_BOX(button_box), add_button);
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_process_clicked), gui);
    
    // Start button
    GtkWidget *start_button = gtk_button_new_with_label("Start");
    gtk_box_append(GTK_BOX(button_box), start_button);
    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_button_clicked), gui);
    
    // Stop/Pause button
    GtkWidget *stop_button = gtk_button_new_with_label("Stop");
    gtk_box_append(GTK_BOX(button_box), stop_button);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_button_clicked), gui);
    
    // Step button
    GtkWidget *step_button = gtk_button_new_with_label("Step");
    gtk_box_append(GTK_BOX(button_box), step_button);
    g_signal_connect(step_button, "clicked", G_CALLBACK(on_step_button_clicked), gui);
    
    // Reset button
    GtkWidget *reset_button = gtk_button_new_with_label("Reset");
    gtk_box_append(GTK_BOX(button_box), reset_button);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(on_reset_button_clicked), gui);
    
    gtk_grid_attach(GTK_GRID(grid), button_box, 0, 2, 2, 1);
    
    // Add Output Area
    GtkWidget *output_label = gtk_label_new("Output:");
    gtk_widget_set_halign(output_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), output_label, 0, 3, 2, 1);
    
    // Create a scrolled window for the output area
    GtkWidget *output_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(output_scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(output_scroll, -1, 80); // Set minimum height
    
    // Create output text view
    GtkWidget *output_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output_text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(output_text_view), GTK_WRAP_WORD_CHAR);
    
    // Set background color to make it distinguishable
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, 
        "textview { background-color: #f0f0f0; font-family: monospace; }", -1);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(output_text_view),
        GTK_STYLE_PROVIDER(provider),
       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    
    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(output_scroll), output_text_view);
    
    // Add scrolled window to grid
    gtk_grid_attach(GTK_GRID(grid), output_scroll, 0, 4, 2, 1);
    gtk_widget_set_vexpand(output_scroll, TRUE);
    
    // Status bar
    GtkWidget *status_bar = gtk_label_new("Ready");
    gtk_widget_set_halign(status_bar, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), status_bar, 0, 5, 2, 1);
    
    gtk_frame_set_child(GTK_FRAME(frame), grid);
    
    // Save references
    gui->algorithm_combo = algo_combo;
    gui->quantum_spin = quantum_spin;
    gui->step_button = step_button;
    gui->start_button = start_button;
    gui->stop_button = stop_button;
    gui->reset_button = reset_button;
    gui->add_process_button = add_button;
    gui->status_bar = status_bar;
    gui->output_text_view = output_text_view;
    gui->output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_text_view));
    
    return frame;
}

// Add this new function to display output messages
void add_output_message(SchedulerGUI *gui, const char *message) {
    if (!gui || !gui->output_buffer || !message) {
        return;
    }
    
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(gui->output_buffer, &iter);
    
    // Add timestamp to message
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", t);
    
    // Create full message with timestamp 
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "%s%s\n", timestamp, message);
    
    // Insert the message at the end of the buffer
    gtk_text_buffer_insert(gui->output_buffer, &iter, full_message, -1);
    
    // Auto-scroll to the bottom
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(gui->output_text_view), &iter, 0.0, FALSE, 0.0, 0.0);
}

// Add after create_control_panel()

// Create the resource management panel
static GtkWidget* create_resource_panel() {
    GtkWidget *frame = gtk_frame_new("Resource Management");
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(main_box, 10);
    gtk_widget_set_margin_end(main_box, 10);
    gtk_widget_set_margin_top(main_box, 5);
    gtk_widget_set_margin_bottom(main_box, 5);
    
    // Create mutex status section
    GtkWidget *mutex_frame = gtk_frame_new("Mutex Status");
    GtkWidget *mutex_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(mutex_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(mutex_grid), 10);
    
    // Headers for mutex grid
    GtkWidget *mutex_header = gtk_label_new("Resource");
    gtk_widget_add_css_class(mutex_header, "header");
    gtk_grid_attach(GTK_GRID(mutex_grid), mutex_header, 0, 0, 1, 1);
    
    GtkWidget *status_header = gtk_label_new("Status");
    gtk_widget_add_css_class(status_header, "header");
    gtk_grid_attach(GTK_GRID(mutex_grid), status_header, 1, 0, 1, 1);
    
    GtkWidget *owner_header = gtk_label_new("Owner");
    gtk_widget_add_css_class(owner_header, "header");
    gtk_grid_attach(GTK_GRID(mutex_grid), owner_header, 2, 0, 1, 1);
    
    // Resource rows (file, userInput, userOutput)
    const char *resources[] = {"file", "userInput", "userOutput"};
    for (int i = 0; i < 3; i++) {
        GtkWidget *resource_label = gtk_label_new(resources[i]);
        gtk_widget_set_halign(resource_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(mutex_grid), resource_label, 0, i+1, 1, 1);
        
        GtkWidget *status_label = gtk_label_new("FREE");
        gtk_widget_set_halign(status_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(mutex_grid), status_label, 1, i+1, 1, 1);
        
        GtkWidget *owner_label = gtk_label_new("None");
        gtk_widget_set_halign(owner_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(mutex_grid), owner_label, 2, i+1, 1, 1);
    }
    
    gtk_frame_set_child(GTK_FRAME(mutex_frame), mutex_grid);
    gtk_box_append(GTK_BOX(main_box), mutex_frame);
    
    // Create blocked queue section
    GtkWidget *blocked_frame = gtk_frame_new("Blocked Queues");
    GtkWidget *blocked_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(blocked_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(blocked_grid), 10);
    
    // Headers for blocked grid
    GtkWidget *resource_header = gtk_label_new("Resource");
    gtk_widget_add_css_class(resource_header, "header");
    gtk_grid_attach(GTK_GRID(blocked_grid), resource_header, 0, 0, 1, 1);
    
    GtkWidget *blocked_list_header = gtk_label_new("Waiting Processes (PID)");
    gtk_widget_add_css_class(blocked_list_header, "header");
    gtk_grid_attach(GTK_GRID(blocked_grid), blocked_list_header, 1, 0, 1, 1);
    
    // Resource rows for blocked queues
    for (int i = 0; i < 3; i++) {
        GtkWidget *resource_label = gtk_label_new(resources[i]);
        gtk_widget_set_halign(resource_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(blocked_grid), resource_label, 0, i+1, 1, 1);
        
        GtkWidget *waiting_label = gtk_label_new("None");
        gtk_widget_set_halign(waiting_label, GTK_ALIGN_START);
        gtk_widget_set_hexpand(waiting_label, TRUE);
        gtk_grid_attach(GTK_GRID(blocked_grid), waiting_label, 1, i+1, 1, 1);
    }
    
    gtk_frame_set_child(GTK_FRAME(blocked_frame), blocked_grid);
    gtk_box_append(GTK_BOX(main_box), blocked_frame);
    
    gtk_frame_set_child(GTK_FRAME(frame), main_box);
    
    return frame;
}

// Add a function to update the resource panel
void update_resource_panel(SchedulerGUI *gui) {
    if (!gui || !gui->resource_panel) return;
    
    // Get the mutex grid and blocked grid
    GtkWidget *mutex_frame = gtk_widget_get_first_child(gtk_frame_get_child(GTK_FRAME(gui->resource_panel)));
    GtkWidget *mutex_grid = gtk_frame_get_child(GTK_FRAME(mutex_frame));
    
    GtkWidget *blocked_frame = gtk_widget_get_next_sibling(mutex_frame);
    GtkWidget *blocked_grid = gtk_frame_get_child(GTK_FRAME(blocked_frame));
    
    // Update mutex status
    const char *resources[] = {"file", "userInput", "userOutput"};
    Mutex *mutexes[] = {&fileMutex, &inputMutex, &outputMutex};
    
    for (int i = 0; i < 3; i++) {
        GtkWidget *status_label = gtk_grid_get_child_at(GTK_GRID(mutex_grid), 1, i+1);
        GtkWidget *owner_label = gtk_grid_get_child_at(GTK_GRID(mutex_grid), 2, i+1);
        
        if (status_label && owner_label) {
            if (mutexes[i]->locked) {
                gtk_label_set_text(GTK_LABEL(status_label), "LOCKED");
                if (mutexes[i]->owner) {
                    char owner_text[20];
                    sprintf(owner_text, "PID %d", mutexes[i]->owner->processID);
                    gtk_label_set_text(GTK_LABEL(owner_label), owner_text);
                } else {
                    gtk_label_set_text(GTK_LABEL(owner_label), "Unknown");
                }
            } else {
                gtk_label_set_text(GTK_LABEL(status_label), "FREE");
                gtk_label_set_text(GTK_LABEL(owner_label), "None");
            }
        }
        
        // Update blocked queue
        GtkWidget *waiting_label = gtk_grid_get_child_at(GTK_GRID(blocked_grid), 1, i+1);
        if (waiting_label) {
            if (mutexes[i]->blockedQueue) {
                // Create a string representation of the blocked queue
                char blocked_text[200] = "";
                PCB *current = mutexes[i]->blockedQueue;
                while (current) {
                    char process_info[30];
                    sprintf(process_info, "PID %d (Pri %d) ", 
                            current->processID, current->currentPriority);
                    strcat(blocked_text, process_info);
                    current = current->next;
                }
                gtk_label_set_text(GTK_LABEL(waiting_label), blocked_text);
            } else {
                gtk_label_set_text(GTK_LABEL(waiting_label), "None");
            }
        }
    }
}
// Add after create_resource_panel() or another appropriate location

// Create the overview section
static GtkWidget* create_overview_panel() {
    GtkWidget *frame = gtk_frame_new("System Overview");
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(main_box, 10);
    gtk_widget_set_margin_end(main_box, 10);
    gtk_widget_set_margin_top(main_box, 5);
    gtk_widget_set_margin_bottom(main_box, 5);
    
    // Create grid for overview information
    GtkWidget *overview_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(overview_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(overview_grid), 10);
    gtk_widget_set_hexpand(overview_grid, TRUE);
    
    // Add overview information labels
    int row = 0;
    
    // Clock cycle
    GtkWidget *clock_label = gtk_label_new("Clock Cycle:");
    gtk_widget_set_halign(clock_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(overview_grid), clock_label, 0, row, 1, 1);
    
    GtkWidget *clock_value = gtk_label_new("-1");
    gtk_widget_set_halign(clock_value, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(overview_grid), clock_value, 1, row, 1, 1);
    row++;
    
    // Total processes
    GtkWidget *processes_label = gtk_label_new("Total Processes:");
    gtk_widget_set_halign(processes_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(overview_grid), processes_label, 0, row, 1, 1);
    
    GtkWidget *processes_value = gtk_label_new("0");
    gtk_widget_set_halign(processes_value, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(overview_grid), processes_value, 1, row, 1, 1);
    row++;
    
    // Active algorithm
    GtkWidget *algorithm_label = gtk_label_new("Algorithm:");
    gtk_widget_set_halign(algorithm_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(overview_grid), algorithm_label, 0, row, 1, 1);
    
    GtkWidget *algorithm_value = gtk_label_new("MLFQ");
    gtk_widget_set_halign(algorithm_value, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(overview_grid), algorithm_value, 1, row, 1, 1);
    
    gtk_box_append(GTK_BOX(main_box), overview_grid);
    gtk_frame_set_child(GTK_FRAME(frame), main_box);
    
    return frame;
}

// Create the queue section
static GtkWidget* create_queue_panel() {
    GtkWidget *frame = gtk_frame_new("Process Queues");
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(main_box, 10);
    gtk_widget_set_margin_end(main_box, 10);
    gtk_widget_set_margin_top(main_box, 5);
    gtk_widget_set_margin_bottom(main_box, 5);

    // Running Process Section
    GtkWidget *running_frame = gtk_frame_new("Running Process");
    GtkWidget *running_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(running_box, 5);
    gtk_widget_set_margin_end(running_box, 5);
    gtk_widget_set_margin_top(running_box, 5);
    gtk_widget_set_margin_bottom(running_box, 5);

    GtkWidget *run_info = gtk_label_new("No process running");
    gtk_widget_set_halign(run_info, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(running_box), run_info);

    gtk_frame_set_child(GTK_FRAME(running_frame), running_box);
    gtk_box_append(GTK_BOX(main_box), running_frame);

    // MLFQ Queues Section (initially hidden if not MLFQ)
    GtkWidget *mlfq_frame = gtk_frame_new("MLFQ Queues");
    GtkWidget *mlfq_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(mlfq_box, 5);
    gtk_widget_set_margin_end(mlfq_box, 5);
    gtk_widget_set_margin_top(mlfq_box, 5);
    gtk_widget_set_margin_bottom(mlfq_box, 5);

    // Add 4 levels for MLFQ
    for (int level = 0; level < 4; level++) {
        char level_label[32];
        snprintf(level_label, sizeof(level_label), "Level %d Queue:", level + 1);
        GtkWidget *level_frame = gtk_frame_new(level_label);
        GtkWidget *level_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_margin_start(level_box, 5);
        gtk_widget_set_margin_end(level_box, 5);
        gtk_widget_set_margin_top(level_box, 2);
        gtk_widget_set_margin_bottom(level_box, 2);
        GtkWidget *level_info = gtk_label_new("Queue empty");
        gtk_widget_set_halign(level_info, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(level_box), level_info);
        gtk_frame_set_child(GTK_FRAME(level_frame), level_box);
        gtk_box_append(GTK_BOX(mlfq_box), level_frame);
    }
    gtk_frame_set_child(GTK_FRAME(mlfq_frame), mlfq_box);
    gtk_box_append(GTK_BOX(main_box), mlfq_frame);

    // Ready Queue Section
    GtkWidget *ready_frame = gtk_frame_new("Ready Queue");
    GtkWidget *ready_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(ready_box, 5);
    gtk_widget_set_margin_end(ready_box, 5);
    gtk_widget_set_margin_top(ready_box, 5);
    gtk_widget_set_margin_bottom(ready_box, 5);
    GtkWidget *ready_info = gtk_label_new("Queue empty");
    gtk_widget_set_halign(ready_info, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(ready_box), ready_info);
    gtk_frame_set_child(GTK_FRAME(ready_frame), ready_box);
    gtk_box_append(GTK_BOX(main_box), ready_frame);

    // Blocked Queue Section (General, not resource specific)
    GtkWidget *blocked_frame = gtk_frame_new("Blocked Queue");
    GtkWidget *blocked_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(blocked_box, 5);
    gtk_widget_set_margin_end(blocked_box, 5);
    gtk_widget_set_margin_top(blocked_box, 5);
    gtk_widget_set_margin_bottom(blocked_box, 5);
    GtkWidget *blocked_info = gtk_label_new("Queue empty");
    gtk_widget_set_halign(blocked_info, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(blocked_box), blocked_info);
    gtk_frame_set_child(GTK_FRAME(blocked_frame), blocked_box);
    gtk_box_append(GTK_BOX(main_box), blocked_frame);

    gtk_frame_set_child(GTK_FRAME(frame), main_box);

    // Store references for dynamic show/hide
    gui->mlfq_frame = mlfq_frame;
    gui->ready_frame = ready_frame;

    return frame;
}

// Add a function to update the overview panel
void update_overview_panel(SchedulerGUI *gui) {
    if (!gui || !gui->overview_panel) return;
    
    // Get the overview grid
    GtkWidget *main_box = gtk_frame_get_child(GTK_FRAME(gui->overview_panel));
    GtkWidget *overview_grid = gtk_widget_get_first_child(main_box);
    
    // Update clock cycle
    GtkWidget *clock_value = gtk_grid_get_child_at(GTK_GRID(overview_grid), 1, 0);
    if (clock_value) {
        char text[20];
        sprintf(text, "%d", currentTime);
        gtk_label_set_text(GTK_LABEL(clock_value), text);
    }
    
    // Update total processes
    GtkWidget *processes_value = gtk_grid_get_child_at(GTK_GRID(overview_grid), 1, 1);
    if (processes_value) {
        char text[20];
        sprintf(text, "%d", numProcesses);
        gtk_label_set_text(GTK_LABEL(processes_value), text);
    }
    
    // Update algorithm
    GtkWidget *algorithm_value = gtk_grid_get_child_at(GTK_GRID(overview_grid), 1, 2);
    if (algorithm_value) {
        const char* algo_text;
        switch (algorithm) {
            case FCFS: algo_text = "FCFS"; break;
            case ROUND_ROBIN: algo_text = "Round Robin"; break;
            case MLFQ: algo_text = "MLFQ"; break;
            default: algo_text = "Unknown";
        }
        gtk_label_set_text(GTK_LABEL(algorithm_value), algo_text);
    }
}

// Add a function to update the queue panel
// void update_queue_panel(SchedulerGUI *gui) {
//     if (!gui || !gui->queue_panel) return;
    
//     // Get the main box
//     GtkWidget *main_box = gtk_frame_get_child(GTK_FRAME(gui->queue_panel));
    
//     // Update running process
//     GtkWidget *running_frame = gtk_widget_get_first_child(main_box);
//     GtkWidget *running_box = gtk_frame_get_child(GTK_FRAME(running_frame));
//     GtkWidget *run_info = gtk_widget_get_first_child(running_box);
    
//     if (run_info) {
//         if (currentRunningProcess) {
//             char text[200];
//             int pc = currentRunningProcess->programCounter;
//             const char* instr = "Unknown";
            
//             // Get the current instruction if valid PC
//             if (pc >= currentRunningProcess->lowerMemoryBound && 
//                 pc <= currentRunningProcess->upperMemoryBound - 9) {
//                 instr = memory[pc].value ? memory[pc].value : "Unknown";
//             }
            
//             sprintf(text, "PID: %d | Instruction: %s", 
//                     currentRunningProcess->processID, instr);
//             gtk_label_set_text(GTK_LABEL(run_info), text);
//         } else {
//             gtk_label_set_text(GTK_LABEL(run_info), "No process running");
//         }
//     }
    
//     // Update ready queue
//     GtkWidget *ready_frame = gtk_widget_get_next_sibling(running_frame);
//     GtkWidget *ready_box = gtk_frame_get_child(GTK_FRAME(ready_frame));
//     GtkWidget *ready_info = gtk_widget_get_first_child(ready_box);
    
//     if (ready_info) {
//         if (readyQueue.front) {
//             char text[500] = "";
//             QueueNode currentNode = readyQueue.front;  // Use QueueNode instead of PCB*
//             int count = 0;
            
//             while (currentNode && count < 5) {  // Show up to 5 processes to avoid overflow
//                 PCB *current = currentNode->pcb;  // Get the PCB from the QueueNode
//                 if (current) {
//                     char process_info[100];
//                     sprintf(process_info, "PID: %d | Priority: %d\n", 
//                             current->processID, current->currentPriority);
//                     strcat(text, process_info);
//                 }
//                 currentNode = currentNode->next;
//                 count++;
//             }
            
//             if (currentNode) {  // If there are more processes
//                 strcat(text, "... more processes in queue");
//             }
            
//             gtk_label_set_text(GTK_LABEL(ready_info), text);
//         } else {
//             gtk_label_set_text(GTK_LABEL(ready_info), "Queue empty");
//         }
//     }
    
//     // Update blocked queue (this is a general view, not specific to resources)
//     GtkWidget *blocked_frame = gtk_widget_get_next_sibling(ready_frame);
//     GtkWidget *blocked_box = gtk_frame_get_child(GTK_FRAME(blocked_frame));
//     GtkWidget *blocked_info = gtk_widget_get_first_child(blocked_box);
    
//     if (blocked_info) {
//         // Count blocked processes across all resources
//         int blocked_count = 0;
//         PCB* blocked_processes[50]; // Assuming max 50 processes could be blocked
        
//         // Check file mutex
//         PCB *current = fileMutex.blockedQueue;
//         while (current) {
//             blocked_processes[blocked_count++] = current;
//             current = current->next;
//         }
        
//         // Check input mutex
//         current = inputMutex.blockedQueue;
//         while (current) {
//             blocked_processes[blocked_count++] = current;
//             current = current->next;
//         }
        
//         // Check output mutex
//         current = outputMutex.blockedQueue;
//         while (current) {
//             blocked_processes[blocked_count++] = current;
//             current = current->next;
//         }
        
//         if (blocked_count > 0) {
//             char text[500] = "";
//             int count = 0;
            
//             for (int i = 0; i < blocked_count && i < 5; i++) {  // Show up to 5 processes
//                 char process_info[100];
//                 sprintf(process_info, "PID: %d | Priority: %d\n", 
//                         blocked_processes[i]->processID, blocked_processes[i]->currentPriority);
//                 strcat(text, process_info);
//             }
            
//             if (blocked_count > 5) {  // If there are more processes
//                 strcat(text, "... more processes blocked");
//             }
            
//             gtk_label_set_text(GTK_LABEL(blocked_info), text);
//         } else {
//             gtk_label_set_text(GTK_LABEL(blocked_info), "Queue empty");
//         }
//     }
// }
void update_queue_panel(SchedulerGUI *gui) {
    if (!gui || !gui->queue_panel) return;
    
    // Get the main box
    GtkWidget *main_box = gtk_frame_get_child(GTK_FRAME(gui->queue_panel));
    
    // Update running process
    GtkWidget *running_frame = gtk_widget_get_first_child(main_box);
    GtkWidget *running_box = gtk_frame_get_child(GTK_FRAME(running_frame));
    GtkWidget *run_info = gtk_widget_get_first_child(running_box);
    
    if (run_info) {
        if (currentRunningProcess) {
            char text[200];
            int pc = currentRunningProcess->programCounter;
            const char* instr = "Unknown";
            
            // Get the current instruction if valid PC
            if (pc >= currentRunningProcess->lowerMemoryBound && 
                pc < currentRunningProcess->upperMemoryBound - 9) {
                // if(pc==0)
                    instr = memory[pc].value ? memory[pc].value : "Unknown";
                // else
                //     instr = memory[pc-1].value ? memory[pc-1].value : "Unknown";
            }
            
            sprintf(text, "PID: %d | Instruction: %s", 
                    currentRunningProcess->processID, instr);
            gtk_label_set_text(GTK_LABEL(run_info), text);
        } else {
            gtk_label_set_text(GTK_LABEL(run_info), "No process running");
        }
    }
    
    // Update MLFQ queues
    GtkWidget *mlfq_frame = gtk_widget_get_next_sibling(running_frame);
    GtkWidget *mlfq_box = gtk_frame_get_child(GTK_FRAME(mlfq_frame));
    GtkWidget *ready_frame = gtk_widget_get_next_sibling(mlfq_frame);
    GtkWidget *ready_box = gtk_frame_get_child(GTK_FRAME(ready_frame));
    GtkWidget *ready_info = gtk_widget_get_first_child(ready_box);

    if (algorithm == MLFQ) {
        gtk_widget_set_visible(mlfq_frame, TRUE);
        gtk_widget_set_visible(ready_frame, FALSE);
    
    
        for (int level = 0; level < 4; level++) {
            GtkWidget *level_frame = gtk_widget_get_first_child(mlfq_box);
            for (int i = 0; i < level && level_frame; i++) {
                level_frame = gtk_widget_get_next_sibling(level_frame);
            }
            if (!level_frame) continue;
            GtkWidget *level_box = gtk_frame_get_child(GTK_FRAME(level_frame));
            GtkWidget *level_info = gtk_widget_get_first_child(level_box);
        
            Queue *queue = &mlfqScheduler.queues[level];
            if (queue->front) {
                char text[200] = "";
                QueueNode *currentNode = queue->front;
                int count = 0;
                while (currentNode && count < 5) {
                    PCB *current = currentNode->pcb;
                    if (current) {
                        char process_info[100];
                        sprintf(process_info, "PID: %d\n",
                                current->processID);
                        strcat(text, process_info);
                        count++;
                    }
                    currentNode = currentNode->next;
                }
                if (currentNode) {
                    strcat(text, "... more processes in queue");
                }
                gtk_label_set_text(GTK_LABEL(level_info), text);
            } else {
                gtk_label_set_text(GTK_LABEL(level_info), "Queue empty");
            }
        }
}
    
    else {
        gtk_widget_set_visible(mlfq_frame, FALSE);
        gtk_widget_set_visible(ready_frame, TRUE);
    }
    
    if (ready_info) {
        if (readyQueue.front) {
            char text[500] = "";
            QueueNode *currentNode = readyQueue.front;  // Use QueueNode* instead of PCB*
            int count = 0;
            
            while (currentNode && count < 5) {  // Show up to 5 processes to avoid overflow
                PCB *current = currentNode->pcb;  // Get the PCB from the QueueNode
                if (current) {
                    char process_info[100];
                    sprintf(process_info, "PID: %d\n", 
                            current->processID);
                    strcat(text, process_info);
                }
                currentNode = currentNode->next;
                count++;
            }
            
            if (currentNode) {  // If there are more processes
                strcat(text, "... more processes in queue");
            }
            
            gtk_label_set_text(GTK_LABEL(ready_info), text);
        } else {
            gtk_label_set_text(GTK_LABEL(ready_info), "Queue empty");
        }
    }
    
    
    // Update blocked queue (this is a general view, not specific to resources)
    GtkWidget *blocked_frame = gtk_widget_get_next_sibling(ready_frame);
    GtkWidget *blocked_box = gtk_frame_get_child(GTK_FRAME(blocked_frame));
    GtkWidget *blocked_info = gtk_widget_get_first_child(blocked_box);
    
    if (blocked_info) {
        // Count blocked processes across all resources
        int blocked_count = 0;
        PCB* blocked_processes[50]; // Assuming max 50 processes could be blocked
        
        // Check file mutex
        PCB *current = fileMutex.blockedQueue;
        while (current) {
            blocked_processes[blocked_count++] = current;
            current = current->next;
        }
        
        // Check input mutex
        current = inputMutex.blockedQueue;
        while (current) {
            blocked_processes[blocked_count++] = current;
            current = current->next;
        }
        
        // Check output mutex
        current = outputMutex.blockedQueue;
        while (current) {
            blocked_processes[blocked_count++] = current;
            current = current->next;
        }
        
        if (blocked_count > 0) {
            char text[500] = "";
            int count = 0;
            
            for (int i = 0; i < blocked_count && i < 5; i++) {  // Show up to 5 processes
                char process_info[100];
                sprintf(process_info, "PID: %d | Priority: %d\n", 
                        blocked_processes[i]->processID, blocked_processes[i]->currentPriority);
                strcat(text, process_info);
            }
            
            if (blocked_count > 5) {  // If there are more processes
                strcat(text, "... more processes blocked");
            }
            
            gtk_label_set_text(GTK_LABEL(blocked_info), text);
        } else {
            gtk_label_set_text(GTK_LABEL(blocked_info), "Queue empty");
        }
    }
}

// Add a message to the log
// Update log_message function for proper formatting
void log_message(SchedulerGUI *gui, const char *message) {
    GtkTextBuffer *buffer = gui->log_buffer;
    GtkTextMark *mark;
    GtkTextIter iter;
    
    if (!gui || !gui->log_buffer || !GTK_IS_TEXT_BUFFER(gui->log_buffer)) {
        // Can't log if buffer is invalid
        fprintf(stderr, "Warning: Invalid log buffer. Message: %s\n", message);
        return;
    } 
    // Get end iterator
    gtk_text_buffer_get_end_iter(buffer, &iter);
    
    // Add timestamp to message
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", t);
    
    // Add newline
    gtk_text_buffer_insert(buffer, &iter, "\n", 1);
    
    // Insert the timestamp and message
    gtk_text_buffer_insert(buffer, &iter, timestamp, -1);
    gtk_text_buffer_insert(buffer, &iter, message, -1);
    gtk_text_buffer_insert(buffer, &iter, "\n", 1);

    // Scroll to the end
    mark = gtk_text_buffer_create_mark(buffer, NULL, &iter, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(gui->log_view), mark, 0.0, TRUE, 0.0, 1.0);
    gtk_text_buffer_delete_mark(buffer, mark);
}

// Update the entire GUI
// Update the update_gui function to include updating the new panels

void update_gui(SchedulerGUI *gui) {
    update_process_list(gui);
    update_memory_view(gui);
    update_resource_panel(gui);
    update_overview_panel(gui);
    update_queue_panel(gui);
    
    // Update status bar with current time
    char status_text[64];
    sprintf(status_text, "Time: %d", currentTime);
    gtk_label_set_text(GTK_LABEL(gui->status_bar), status_text);
}


void bind_process_id_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    
    char text[16];
    snprintf(text, sizeof(text), "%d", GPOINTER_TO_INT(g_object_get_data(obj, "id")));
    gtk_label_set_text(GTK_LABEL(label), text);
}



void bind_process_filename_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    const char *filename = g_object_get_data(obj, "filename");
    gtk_label_set_text(GTK_LABEL(label), filename ? filename : "");
}

void bind_process_arrival_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    char text[16];
    int arrival = GPOINTER_TO_INT(g_object_get_data(obj, "arrivalTime"));
    sprintf(text, "%d", arrival);
    gtk_label_set_text(GTK_LABEL(label), text);
}

// void bind_process_priority_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
//     GObject *obj = gtk_list_item_get_item(list_item);
//     GtkWidget *label = gtk_list_item_get_child(list_item);
//     char text[16];
//     int priority = GPOINTER_TO_INT(g_object_get_data(obj, "priority"));
//     sprintf(text, "%d", priority);
//     gtk_label_set_text(GTK_LABEL(label), text);
// }

void bind_process_state_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    const char *state = g_object_get_data(obj, "state");
    gtk_label_set_text(GTK_LABEL(label), state ? state : "");
}

void bind_process_executed_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    char text[16];
    int executed = GPOINTER_TO_INT(g_object_get_data(obj, "executedTime"));
    sprintf(text, "%d", executed);
    gtk_label_set_text(GTK_LABEL(label), text);
}

void bind_process_waiting_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    char text[16];
    int waiting = GPOINTER_TO_INT(g_object_get_data(obj, "waitingTime"));
    sprintf(text, "%d", waiting);
    gtk_label_set_text(GTK_LABEL(label), text);
}

// Bind callbacks for memory view
// Replace the memory binding functions

void bind_memory_index_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    char text[16];
    int index = GPOINTER_TO_INT(g_object_get_data(obj, "index"));
    sprintf(text, "%d", index);
    gtk_label_set_text(GTK_LABEL(label), text);
}

void bind_memory_pid_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    char text[16];
    int pid = GPOINTER_TO_INT(g_object_get_data(obj, "pid"));
    
    if (pid >= 0) {
        sprintf(text, "%d", pid + 1); // Display PID starting from 1
    } else {
        strcpy(text, "-");
    }
    
    gtk_label_set_text(GTK_LABEL(label), text);
}

void bind_memory_name_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    const char *name = g_object_get_data(obj, "name");
    gtk_label_set_text(GTK_LABEL(label), name ? name : "");
}

void bind_memory_value_cb(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GObject *obj = gtk_list_item_get_item(list_item);
    GtkWidget *label = gtk_list_item_get_child(list_item);
    const char *value = g_object_get_data(obj, "value");
    gtk_label_set_text(GTK_LABEL(label), value ? value : "");
}
// Create main application window
// Modify the create_window function to include the new panels

// Show input dialog for the assign instruction

// Add this function to GUI.c

// Show input dialog for the assign instruction
void show_input_dialog(const char *var_name, bool numeric_only) {
    // Create a dialog
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Variable Input");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gui->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 150);
    
    // Create layout
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    
    // Create prompt
    char prompt[100];
    if (numeric_only) {
        snprintf(prompt, sizeof(prompt), "Enter an integer value for %s:", var_name);
    } else {
        snprintf(prompt, sizeof(prompt), "Enter a value for %s:", var_name);
    }
    
    GtkWidget *label = gtk_label_new(prompt);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), label);
    
    // Create entry
    GtkWidget *entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), entry);
    
    // Create button box
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(button_box, 10);
    
    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    
    gtk_box_append(GTK_BOX(button_box), cancel_button);
    gtk_box_append(GTK_BOX(button_box), ok_button);
    gtk_box_append(GTK_BOX(box), button_box);

    InputDialogData *dialog_data = g_new(InputDialogData, 1);
    dialog_data->gui = gui;
    dialog_data->entry = entry;
    dialog_data->dialog = dialog;
    
    // Connect signals with correct user_data
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_input_dialog_cancel), dialog_data);
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_input_dialog_ok), dialog_data);
    
    // Free the dialog data when dialog is destroyed
    g_object_set_data_full(G_OBJECT(dialog), "dialog-data", dialog_data, g_free);
    
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    
    // Connect signals
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_input_dialog_cancel), dialog);
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_input_dialog_ok), entry);
    
    // Store dialog in GUI struct for reference
    gui->input_dialog = dialog;
    
    // Show the dialog
    gtk_widget_set_visible(dialog, TRUE);
}

static void create_window(GtkApplication *app) {
    // Create main window
    gui->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(gui->window), "OS Scheduler Simulator");
    gtk_window_set_default_size(GTK_WINDOW(gui->window), 1200, 800);
    g_signal_connect(gui->window, "delete-event", G_CALLBACK(on_window_delete_event), NULL);
    
    // Create a scrolled window for the entire content
    GtkWidget *main_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Create main grid layout
    GtkWidget *main_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(main_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(main_grid), 10);
    gtk_widget_set_margin_start(main_grid, 10);
    gtk_widget_set_margin_end(main_grid, 10);
    gtk_widget_set_margin_top(main_grid, 10);
    gtk_widget_set_margin_bottom(main_grid, 10);
    
    // Left side: Overview, queues, process table and control panel
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    // Overview panel at the top
    GtkWidget *overview_panel = create_overview_panel();
    gui->overview_panel = overview_panel;
    gtk_box_append(GTK_BOX(left_box), overview_panel);
    
    // Queue panel below overview
    GtkWidget *queue_panel = create_queue_panel();
    gui->queue_panel = queue_panel;
    gtk_box_append(GTK_BOX(left_box), queue_panel);
    
    // Process table
    GtkWidget *process_table = create_process_table();
    gtk_widget_set_vexpand(process_table, TRUE);
    gtk_box_append(GTK_BOX(left_box), process_table);
    
    // Control panel
    GtkWidget *control_panel = create_control_panel();
    gtk_box_append(GTK_BOX(left_box), control_panel);
    
    // Resource panel
    GtkWidget *resource_panel = create_resource_panel();
    gui->resource_panel = resource_panel;
    gtk_box_append(GTK_BOX(left_box), resource_panel);
    
    // Add left box to main grid
    gtk_grid_attach(GTK_GRID(main_grid), left_box, 0, 0, 1, 2);
    
    // Right side: Memory view and log
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    // Memory view at the top
    GtkWidget *memory_view = create_memory_view();
    gtk_widget_set_vexpand(memory_view, TRUE);
    gtk_box_append(GTK_BOX(right_box), memory_view);
    
    // Log view at the bottom
    GtkWidget *log_view = create_log_view();
    gtk_widget_set_vexpand(log_view, TRUE);
    gtk_box_append(GTK_BOX(right_box), log_view);
    
    // Add right box to main grid
    gtk_grid_attach(GTK_GRID(main_grid), right_box, 1, 0, 1, 2);
    
    // Status bar at the bottom
    GtkWidget *status_bar = gtk_label_new("Ready");
    gtk_widget_set_halign(status_bar, GTK_ALIGN_START);
    gtk_widget_add_css_class(status_bar, "status-bar");
    gtk_grid_attach(GTK_GRID(main_grid), status_bar, 0, 2, 2, 1);
    gui->status_bar = status_bar;
    
    // Add the main grid to the scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(main_scroll), main_grid);
    
    // Set the scrolled window as the child of the main window
    gtk_window_set_child(GTK_WINDOW(gui->window), main_scroll);
}
// Application activate callback
static void app_activate(GtkApplication *app, gpointer user_data) {
    // Allocate the GUI structure
    gui = g_new0(SchedulerGUI, 1);
    gui->is_running = FALSE;
    gui->is_paused = FALSE;
    gui->timer_id = 0;
    
    // Create window
    create_window(app);
    
    // Connect window delete signal
    g_signal_connect(G_OBJECT(gui->window), "close-request", 
                    G_CALLBACK(on_window_delete_event), gui);
    
    // Show the window
    gtk_widget_set_visible(gui->window, TRUE);
    
    // Initial log message
    log_message(gui, "OS Scheduler Simulation initialized");
    
    // Set RR_QUANTUM default value based on GUI
    RR_QUANTUM = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui->quantum_spin));
}
// Add this function


// void initialize_simulation() {
//     // Reset simulation state
//     currentTime = 0;
//     currentRunningProcess = NULL;
//     simulationPaused = false;
//     simulationRunning = true;
    
//     // Initialize memory
//     for (int i = 0; i < memorySize; i++) {
//         memory[i].processID = -1;
//         memory[i].name = NULL;
//         memory[i].value = NULL;
//     }
    
//     // Initialize queues
//     initialize_queue(&readyQueue);
//     initMutex(&fileMutex, "file");
//     initMutex(&inputMutex, "userInput");
//     initMutex(&outputMutex, "userOutput");
    
//     // Initialize MLFQ queues if using MLFQ
//     if (algorithm == MLFQ) {
//         for (int i = 0; i < NUM_MLFQ_LEVELS; i++) {
//             initialize_queue(&mlfqScheduler.queues[i]);
//             // Set default time quantum values
//             switch (i) {
//                 case 0: mlfqScheduler.timeQuantums[i] = 1; break; 
//                 case 1: mlfqScheduler.timeQuantums[i] = 2; break;
//                 case 2: mlfqScheduler.timeQuantums[i] = 4; break;
//                 case 3: mlfqScheduler.timeQuantums[i] = 8; break;
//             }
//         }
//     }
    
//     // Initialize and reset variables
//     for (int i = 0; i < MAX_VARIABLES; i++) {
//         variables[i].name[0] = '\0';
//         variables[i].value[0] = '\0';
//     }
//     var_count = 0;
    
   
// }

// Add this function to drive the simulation from the GUI side
void gui_run_full_simulation(SchedulerGUI *gui) {
    if (!gui) return;
    
    // Set GUI state to running
    gui->is_running = TRUE;
    gui->is_paused = FALSE;
    
    // Update UI state
    gtk_widget_set_sensitive(gui->step_button, FALSE);
    gtk_widget_set_sensitive(gui->algorithm_combo, FALSE);
    gtk_widget_set_sensitive(gui->quantum_spin, FALSE);
    gtk_widget_set_sensitive(gui->add_process_button, FALSE);
    
    log_message(gui, "Starting full simulation");
    
    // Call the backend simulation function
    run_full_simulation();
    
    // Update GUI with final state
    update_gui(gui);
    
    // Reset GUI state if simulation completed
    if (all_processes_complete()) {
        gui->is_running = FALSE;
        gui->is_paused = FALSE;
        
        // Re-enable controls
        gtk_widget_set_sensitive(gui->step_button, FALSE);
        gtk_widget_set_sensitive(gui->algorithm_combo, TRUE);
        gtk_widget_set_sensitive(gui->quantum_spin, 
            gtk_drop_down_get_selected(GTK_DROP_DOWN(gui->algorithm_combo)) != 0);
        gtk_widget_set_sensitive(gui->add_process_button, TRUE);
        
        log_message(gui, "Simulation complete");
    }
}
// Main function
int main(int argc, char *argv[]) {
    // Create the application
    app = gtk_application_new("com.example.osscheduler", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
    
    // Run the application
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}

// Add these stubs at the bottom of the file

// Empty stubs for backwards compatibility



// Structure to pass data to the callback
// typedef struct {
//     GtkEntry *entry;
//     int *result;
//     int *ok;
//     GtkWidget *dialog;
// } IntInputData;

// // Callback for dialog response
// static void on_int_input_response(GtkDialog *dialog, int response_id, gpointer user_data) {
//     IntInputData *data = (IntInputData *)user_data;
//     if (response_id == GTK_RESPONSE_OK) {
//         const char *text = gtk_editable_get_text(GTK_EDITABLE(data->entry));      
//         if (text && *text) {
//             *(data->result) = atoi(text);
//             *(data->ok) = 1;
//         }
//     }
//     gtk_window_destroy(GTK_WINDOW(data->dialog));
//     g_free(data);
// }

// Shows the dialog and returns immediately; result and ok are set asynchronously
// void get_user_int_input_async(SchedulerGUI *gui, const char *prompt, int *result, int *ok) {
//     GtkWidget *dialog = gtk_dialog_new();
//     gtk_window_set_title(GTK_WINDOW(dialog), prompt);
//     gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gui->window));
//     gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

//     GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
//     GtkWidget *entry = gtk_entry_new();
//     gtk_box_append(GTK_BOX(content_area), entry);

//     GtkWidget *enter_button = gtk_button_new_with_label("Enter");
//     gtk_box_append(GTK_BOX(content_area), enter_button);

//     gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);

//     // Prepare data for callback
//     IntInputData *data = g_malloc(sizeof(IntInputData));
//     data->entry = GTK_ENTRY(entry);
//     data->result = result;
//     data->ok = ok;
//     data->dialog = dialog;
//     *ok = 0;

//     // Connect Enter button to OK response
//     g_signal_connect(enter_button, "clicked", G_CALLBACK(gtk_dialog_response), dialog);
//     // Connect dialog response to handler
//     g_signal_connect(dialog, "response", G_CALLBACK(on_int_input_response), data);

//     gtk_widget_show(dialog);
// }

// Returns TRUE if user entered a value, FALSE if cancelled