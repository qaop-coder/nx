#pragma once

#include "build.h"

#if KORE_OS_WINDOWS
#    include <conio.h>
#elif KORE_OS_LINUX
#    include <dirent.h>
#    include <errno.h>
#    include <signal.h>
#    include <sys/inotify.h>
#    include <sys/stat.h>
#    include <time.h>
#    include <termios.h>
#    include <sys/ioctl.h>
#endif

//
// Terminal User Interface (TUI) Framework
//

typedef enum {
    TUIMode_List,
    TUIMode_SplitPane,
    TUIMode_Search
} TUIMode;

typedef struct {
    bool colour_support;
    bool initialised;
    int  selected_message_index;
    int  scroll_offset;
    int  spinner_state;
    u64  last_spinner_update;
    TUIMode mode;
    int  terminal_width;
    int  terminal_height;
    char search_query[256];
    int  search_results_count;
    int  history_index;
    int  history_count;
#if KORE_OS_LINUX
    struct termios original_termios;
#endif
} TUIState;

//
// Build message types
//

typedef enum {
    MessageType_Error,
    MessageType_Warning,
    MessageType_Note,
    MessageType_Unknown
} MessageType;

typedef struct {
    MessageType type;
    String      file_path;
    int         line_number;
    int         column_number;
    String      message;
} BuildMessage;

#define MAX_BUILD_HISTORY 10

typedef struct {
    KArray(BuildMessage) messages;
    u64 timestamp;
    u64 build_duration_ms;
    i32 exit_code;
    usize memory_used;
    Arena* arena;
} BuildHistoryEntry;

static BuildHistoryEntry build_history[MAX_BUILD_HISTORY];
static int history_write_index = 0;

static TUIState tui_state = {0};

// Terminal control sequences
#define TUI_CLEAR_SCREEN "\033[2J"
#define TUI_CURSOR_HOME  "\033[H"
#define TUI_CURSOR_HIDE  "\033[?25l"
#define TUI_CURSOR_SHOW  "\033[?25h"

static bool tui_init(void)
{
    if (tui_state.initialised) {
        return true;
    }

#if KORE_OS_WINDOWS
    // Enable ANSI escape sequences on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (SetConsoleMode(hOut, dwMode)) {
            tui_state.colour_support = true;
        }
    }
#else
    // Check if we're in a terminal that supports colours
    const char* term = getenv("TERM");
    tui_state.colour_support = term && (strstr(term, "color") || strstr(term, "xterm"));
    
    // Set up terminal for raw input
    if (tcgetattr(STDIN_FILENO, &tui_state.original_termios) == 0) {
        struct termios new_termios = tui_state.original_termios;
        new_termios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    }
#endif

    tui_state.initialised = true;
    return true;
}

static void tui_cleanup(void)
{
    if (!tui_state.initialised) {
        return;
    }

#if KORE_OS_LINUX
    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &tui_state.original_termios);
#endif

    // Show cursor and reset colours
    $.prn(TUI_CURSOR_SHOW KORE_ANSI_RESET);
    
    tui_state.initialised = false;
}

static void tui_clear_screen(void)
{
    $.pr(TUI_CLEAR_SCREEN TUI_CURSOR_HOME);
}

static void tui_print_coloured(const char* colour, const char* text)
{
    if (tui_state.colour_support) {
        $.pr("%s%s%s", colour, text, KORE_ANSI_RESET);
    } else {
        $.pr("%s", text);
    }
}

static void tui_get_terminal_size(void)
{
#if KORE_OS_WINDOWS
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        tui_state.terminal_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        tui_state.terminal_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        tui_state.terminal_width = 80;
        tui_state.terminal_height = 25;
    }
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        tui_state.terminal_width = w.ws_col;
        tui_state.terminal_height = w.ws_row;
    } else {
        tui_state.terminal_width = 80;
        tui_state.terminal_height = 25;
    }
#endif
}

static void tui_display_file_context(const char* file_path, int line_number, Arena* arena)
{
    if (!file_path || line_number <= 0) {
        return;
    }
    
    FILE* file = fopen(file_path, "r");
    if (!file) {
        return;
    }
    
    const int context_lines = 2; // Show 2 lines before and after
    const int start_line = line_number - context_lines;
    const int end_line = line_number + context_lines;
    
    char line_buffer[1024];
    int current_line = 1;
    
    // Skip to start line
    while (current_line < start_line && fgets(line_buffer, sizeof(line_buffer), file)) {
        current_line++;
    }
    
    // Display context lines
    while (current_line <= end_line && fgets(line_buffer, sizeof(line_buffer), file)) {
        // Remove trailing newline
        size_t len = strlen(line_buffer);
        if (len > 0 && line_buffer[len - 1] == '\n') {
            line_buffer[len - 1] = '\0';
        }
        
        $.pr("    ");
        if (current_line == line_number) {
            // Highlight the error line with bold
            tui_print_coloured(KORE_ANSI_BOLD KORE_ANSI_WHITE, "");
            $.pr("%4d▶ %s", current_line, line_buffer);
            tui_print_coloured(KORE_ANSI_RESET, "");
        } else {
            // Normal context line
            $.pr("%4d  %s", current_line, line_buffer);
        }
        $.pr("\n");
        current_line++;
    }
    
    fclose(file);
}

static void tui_display_spinner(const char* message)
{
    const char* spinner_chars[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    const int spinner_count = sizeof(spinner_chars) / sizeof(spinner_chars[0]);
    
    u64 current_time = $.time_ms($.time_now());
    
    // Update spinner every 100ms
    if (current_time - tui_state.last_spinner_update >= 100) {
        tui_state.spinner_state = (tui_state.spinner_state + 1) % spinner_count;
        tui_state.last_spinner_update = current_time;
    }
    
    tui_print_coloured(KORE_ANSI_CYAN, spinner_chars[tui_state.spinner_state]);
    $.pr(" %s", message);
}

static void tui_display_building_status(void)
{
    tui_display_spinner("Building...");
    $.pr("\n");
}

static void tui_display_timestamp(void)
{
    u64 current_time = $.time_ms($.time_now());
    // Convert to seconds for display
    u64 seconds = current_time / 1000;
    u64 minutes = seconds / 60;
    u64 hours = minutes / 60;
    
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "[%02llu:%02llu:%02llu]", 
             hours % 24, minutes % 60, seconds % 60);
    
    tui_print_coloured(KORE_ANSI_WHITE, timestamp);
    $.pr(" ");
}

typedef enum {
    InputResult_Continue,
    InputResult_Quit,
    InputResult_NavigateUp,
    InputResult_NavigateDown,
    InputResult_ToggleMode,
    InputResult_StartSearch,
    InputResult_CancelSearch,
    InputResult_HistoryPrev,
    InputResult_HistoryNext
} InputResult;

static InputResult tui_check_input(void)
{
#if KORE_OS_WINDOWS
    if (_kbhit()) {
        char ch = _getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) { // 27 = ESC
            return InputResult_Quit;
        }
        if (ch == 't' || ch == 'T') { // Toggle split-pane mode
            return InputResult_ToggleMode;
        }
        if (ch == '/' || ch == 's' || ch == 'S') { // Start search
            return InputResult_StartSearch;
        }
        if (ch == 'h' || ch == 'H') { // History previous
            return InputResult_HistoryPrev;
        }
        if (ch == 'j' || ch == 'J') { // History next
            return InputResult_HistoryNext;
        }
        // Handle arrow keys (they come as escape sequences)
        if (ch == 0 || (unsigned char)ch == 224) { // Extended key prefix on Windows
            ch = _getch();
            if (ch == 72) { // Up arrow
                return InputResult_NavigateUp;
            } else if (ch == 80) { // Down arrow
                return InputResult_NavigateDown;
            }
        }
    }
#else
    fd_set readfds;
    struct timeval timeout = {0, 0}; // Non-blocking
    
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) > 0) {
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                if (ch == 'q' || ch == 'Q' || ch == 27) { // 27 = ESC
                    if (ch == 27) {
                        // Check for arrow key escape sequence
                        char seq[2];
                        if (read(STDIN_FILENO, &seq[0], 1) > 0 && seq[0] == '[') {
                            if (read(STDIN_FILENO, &seq[1], 1) > 0) {
                                if (seq[1] == 'A') { // Up arrow
                                    return InputResult_NavigateUp;
                                } else if (seq[1] == 'B') { // Down arrow
                                    return InputResult_NavigateDown;
                                }
                            }
                        }
                    }
                    return InputResult_Quit;
                }
                if (ch == 't' || ch == 'T') { // Toggle split-pane mode
                    return InputResult_ToggleMode;
                }
                if (ch == '/' || ch == 's' || ch == 'S') { // Start search
                    return InputResult_StartSearch;
                }
                if (ch == 'h' || ch == 'H') { // History previous
                    return InputResult_HistoryPrev;
                }
                if (ch == 'j' || ch == 'J') { // History next
                    return InputResult_HistoryNext;
                }
            }
        }
    }
#endif
    return InputResult_Continue;
}

//
// Build message parsing functions
//

static bool string_contains_zstring(String str, const char* needle)
{
    size_t needle_len = strlen(needle);
    if (needle_len > str.length) {
        return false;
    }

    for (size_t i = 0; i <= str.length - needle_len; i++) {
        if (strncmp(str.data + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

static MessageType parse_message_type(String line)
{
    // Look for clang/GCC message patterns
    if (string_contains_zstring(line, ": error:")) {
        return MessageType_Error;
    } else if (string_contains_zstring(line, ": warning:")) {
        return MessageType_Warning;
    } else if (string_contains_zstring(line, ": note:")) {
        return MessageType_Note;
    }
    return MessageType_Unknown;
}

static BuildMessage parse_compiler_message(String line, Arena* arena)
{
    BuildMessage msg  = {0};
    msg.type          = parse_message_type(line);
    msg.line_number   = -1;
    msg.column_number = -1;

    if (msg.type == MessageType_Unknown) {
        // Just store the whole line as message
        StringBuilder sb = string_builder_init(arena);
        string_builder_append_string(&sb, line);
        string_builder_null_terminate(&sb);
        msg.message = sb.str;
        return msg;
    }

    // Parse pattern: file:line:column: type: message
    const char* data   = line.data;
    const char* colon1 = strchr(data, ':');

    if (colon1) {
        // Extract file path
        StringBuilder file_sb = string_builder_init(arena);
        string_builder_append_string(&file_sb, (String){data, colon1 - data});
        string_builder_null_terminate(&file_sb);
        msg.file_path      = file_sb.str;

        // Parse line number
        const char* colon2 = strchr(colon1 + 1, ':');
        if (colon2) {
            char   line_str[32];
            size_t line_len = colon2 - (colon1 + 1);
            if (line_len < sizeof(line_str)) {
                strncpy(line_str, colon1 + 1, line_len);
                line_str[line_len] = '\0';
                msg.line_number    = atoi(line_str);

                // Parse column number
                const char* colon3 = strchr(colon2 + 1, ':');
                if (colon3) {
                    char   col_str[32];
                    size_t col_len = colon3 - (colon2 + 1);
                    if (col_len < sizeof(col_str)) {
                        strncpy(col_str, colon2 + 1, col_len);
                        col_str[col_len]      = '\0';
                        msg.column_number     = atoi(col_str);

                        // Find message start (after "error:" or "warning:")
                        const char* msg_start = strstr(colon3, ": ");
                        if (msg_start) {
                            msg_start += 2; // Skip ": "
                            const char* type_end = strstr(msg_start, ": ");
                            if (type_end) {
                                msg_start = type_end +
                                            2; // Skip "error: " or "warning: "

                                StringBuilder msg_sb =
                                    string_builder_init(arena);
                                size_t msg_len =
                                    (line.data + line.length) - msg_start;
                                string_builder_append_string(
                                    &msg_sb, (String){msg_start, msg_len});
                                string_builder_null_terminate(&msg_sb);
                                msg.message = msg_sb.str;
                            }
                        }
                    }
                }
            }
        }
    }

    return msg;
}

static KArray(BuildMessage)
    parse_build_output(KArray(String) output_lines, Arena* arena)
{
    KArray(BuildMessage) messages = NULL;

    for (usize i = 0; i < array_length(output_lines); ++i) {
        BuildMessage msg = parse_compiler_message(output_lines[i], arena);
        if (msg.type != MessageType_Unknown) {
            array_add(messages, msg);
        }
    }

    return messages;
}

//
// TUI Status Display Functions
//

static void tui_display_success(void)
{
    tui_print_coloured(KORE_ANSI_GREEN, "✅ Build successful! No errors or warnings.\n");
}

static void tui_display_warnings_navigable(KArray(BuildMessage) messages)
{
    int warning_count = 0;
    KArray(BuildMessage) warnings = NULL;
    
    // Extract warnings
    for (usize i = 0; i < array_length(messages); ++i) {
        if (messages[i].type == MessageType_Warning) {
            array_add(warnings, messages[i]);
            warning_count++;
        }
    }
    
    if (warning_count == 0) {
        tui_display_success();
        array_free(warnings);
        return;
    }
    
    // Display warning header with navigation info
    char header[256];
    if (warning_count > 1) {
        snprintf(header, sizeof(header), "⚠️  Build completed with %d warnings (Use ↑↓ to navigate):\n", warning_count);
    } else {
        snprintf(header, sizeof(header), "⚠️  Build completed with %d warning:\n", warning_count);
    }
    tui_print_coloured(KORE_ANSI_YELLOW, header);
    
    // Ensure selected index is valid
    if (tui_state.selected_message_index >= warning_count) {
        tui_state.selected_message_index = warning_count - 1;
    }
    if (tui_state.selected_message_index < 0) {
        tui_state.selected_message_index = 0;
    }
    
    // Calculate display bounds for scrolling
    tui_get_terminal_size();
    int max_display_lines = tui_state.terminal_height - 10; // Reserve space for header/footer
    int display_count = warning_count > max_display_lines ? max_display_lines : warning_count;
    
    // Adjust scroll offset to keep selected item visible
    if (tui_state.selected_message_index < tui_state.scroll_offset) {
        tui_state.scroll_offset = tui_state.selected_message_index;
    } else if (tui_state.selected_message_index >= tui_state.scroll_offset + display_count) {
        tui_state.scroll_offset = tui_state.selected_message_index - display_count + 1;
    }
    
    // Display warning details with selection highlight
    for (int i = tui_state.scroll_offset; i < tui_state.scroll_offset + display_count && i < warning_count; ++i) {
        bool is_selected = (i == tui_state.selected_message_index);
        
        if (is_selected) {
            $.pr("▶ ");
            tui_print_coloured(KORE_ANSI_BOLD, "");
        } else {
            $.pr("  ");
        }
        
        tui_print_coloured(KORE_ANSI_CYAN, warnings[i].file_path.data);
        if (warnings[i].line_number != -1) {
            $.pr(":%d", warnings[i].line_number);
            if (warnings[i].column_number != -1) {
                $.pr(":%d", warnings[i].column_number);
            }
        }
        $.pr(": ");
        tui_print_coloured(KORE_ANSI_YELLOW, warnings[i].message.data);
        
        if (is_selected) {
            tui_print_coloured(KORE_ANSI_RESET, "");
        }
        $.pr("\n");
        
        // Show context for selected message
        if (is_selected && warnings[i].file_path.data && warnings[i].line_number > 0) {
            $.pr("\n");
            
            // Display file context
            tui_display_file_context(warnings[i].file_path.data, warnings[i].line_number, NULL);
            $.pr("\n");
        }
    }
    
    // Show scrolling indicator if needed
    if (warning_count > display_count) {
        $.pr("    ");
        char scroll_indicator[256];
        snprintf(scroll_indicator, sizeof(scroll_indicator), "... (%d/%d warnings shown)", display_count, warning_count);
        tui_print_coloured(KORE_ANSI_WHITE, scroll_indicator);
        $.pr("\n");
    }
    
    array_free(warnings);
}

static void tui_display_errors_navigable(KArray(BuildMessage) messages)
{
    int error_count = 0;
    KArray(BuildMessage) errors = NULL;
    
    // Extract errors
    for (usize i = 0; i < array_length(messages); ++i) {
        if (messages[i].type == MessageType_Error) {
            array_add(errors, messages[i]);
            error_count++;
        }
    }
    
    if (error_count == 0) {
        // No errors, check for warnings
        tui_display_warnings_navigable(messages);
        array_free(errors);
        return;
    }
    
    // Display error header with navigation info
    char header[256];
    if (error_count > 1) {
        snprintf(header, sizeof(header), "❌ Build failed with %d errors (Use ↑↓ to navigate):\n", error_count);
    } else {
        snprintf(header, sizeof(header), "❌ Build failed with %d error:\n", error_count);
    }
    tui_print_coloured(KORE_ANSI_RED, header);
    
    // Ensure selected index is valid
    if (tui_state.selected_message_index >= error_count) {
        tui_state.selected_message_index = error_count - 1;
    }
    if (tui_state.selected_message_index < 0) {
        tui_state.selected_message_index = 0;
    }
    
    // Calculate display bounds for scrolling
    tui_get_terminal_size();
    int max_display_lines = tui_state.terminal_height - 10; // Reserve space for header/footer
    int display_count = error_count > max_display_lines ? max_display_lines : error_count;
    
    // Adjust scroll offset to keep selected item visible
    if (tui_state.selected_message_index < tui_state.scroll_offset) {
        tui_state.scroll_offset = tui_state.selected_message_index;
    } else if (tui_state.selected_message_index >= tui_state.scroll_offset + display_count) {
        tui_state.scroll_offset = tui_state.selected_message_index - display_count + 1;
    }
    
    // Display error details with selection highlight
    for (int i = tui_state.scroll_offset; i < tui_state.scroll_offset + display_count && i < error_count; ++i) {
        bool is_selected = (i == tui_state.selected_message_index);
        
        if (is_selected) {
            $.pr("▶ ");
            tui_print_coloured(KORE_ANSI_BOLD, "");
        } else {
            $.pr("  ");
        }
        
        tui_print_coloured(KORE_ANSI_CYAN, errors[i].file_path.data);
        if (errors[i].line_number != -1) {
            $.pr(":%d", errors[i].line_number);
            if (errors[i].column_number != -1) {
                $.pr(":%d", errors[i].column_number);
            }
        }
        $.pr(": ");
        tui_print_coloured(KORE_ANSI_RED, errors[i].message.data);
        
        if (is_selected) {
            tui_print_coloured(KORE_ANSI_RESET, "");
        }
        $.pr("\n");
        
        // Show context for selected message
        if (is_selected && errors[i].file_path.data && errors[i].line_number > 0) {
            $.pr("\n");
            
            // Display file context
            tui_display_file_context(errors[i].file_path.data, errors[i].line_number, NULL);
            $.pr("\n");
        }
    }
    
    // Show scrolling indicator if needed
    if (error_count > display_count) {
        $.pr("    ");
        char scroll_indicator[256];
        snprintf(scroll_indicator, sizeof(scroll_indicator), "... (%d/%d errors shown)", display_count, error_count);
        tui_print_coloured(KORE_ANSI_WHITE, scroll_indicator);
        $.pr("\n");
    }
    
    array_free(errors);
}

// Extract unique file paths from messages
static KArray(String) tui_extract_file_list(KArray(BuildMessage) messages, Arena* arena)
{
    KArray(String) files = NULL;
    
    for (usize i = 0; i < array_length(messages); ++i) {
        if (messages[i].file_path.data && messages[i].file_path.length > 0) {
            // Check if we already have this file
            bool already_added = false;
            for (usize j = 0; j < array_length(files); ++j) {
                if (files[j].length == messages[i].file_path.length &&
                    strncmp(files[j].data, messages[i].file_path.data, files[j].length) == 0) {
                    already_added = true;
                    break;
                }
            }
            if (!already_added) {
                array_add(files, messages[i].file_path);
            }
        }
    }
    
    return files;
}

// Filter messages based on search query
static KArray(BuildMessage) tui_filter_messages(KArray(BuildMessage) messages, const char* query, Arena* arena)
{
    KArray(BuildMessage) filtered = NULL;
    
    if (!query || strlen(query) == 0) {
        // No filter, return all messages
        for (usize i = 0; i < array_length(messages); ++i) {
            array_add(filtered, messages[i]);
        }
        return filtered;
    }
    
    // Case-insensitive search through message text and file paths
    for (usize i = 0; i < array_length(messages); ++i) {
        BuildMessage* msg = &messages[i];
        bool matches = false;
        
        // Search in message text
        if (msg->message.data && strstr(msg->message.data, query) != NULL) {
            matches = true;
        }
        
        // Search in file path
        if (!matches && msg->file_path.data && strstr(msg->file_path.data, query) != NULL) {
            matches = true;
        }
        
        if (matches) {
            array_add(filtered, *msg);
        }
    }
    
    return filtered;
}

static void tui_display_search_mode(KArray(BuildMessage) all_messages, Arena* arena)
{
    KArray(BuildMessage) filtered = tui_filter_messages(all_messages, tui_state.search_query, arena);
    tui_state.search_results_count = array_length(filtered);
    
    $.pr("Search: %s (%d results) [Press ESC to cancel]\n", 
         tui_state.search_query, tui_state.search_results_count);
    $.pr("────────────────────────────────────────────────────────\n");
    
    if (tui_state.search_results_count == 0) {
        tui_print_coloured(KORE_ANSI_YELLOW, "No matches found.");
        $.pr("\n");
    } else {
        // Display filtered results
        for (usize i = 0; i < array_length(filtered); ++i) {
            BuildMessage* msg = &filtered[i];
            
            bool is_selected = (i == tui_state.selected_message_index);
            
            if (is_selected) {
                $.pr("▶ ");
                tui_print_coloured(KORE_ANSI_BOLD, "");
            } else {
                $.pr("  ");
            }
            
            // Display message type with color
            if (msg->type == MessageType_Error) {
                tui_print_coloured(KORE_ANSI_RED, "ERROR");
            } else if (msg->type == MessageType_Warning) {
                tui_print_coloured(KORE_ANSI_YELLOW, "WARN ");
            } else {
                $.pr("INFO ");
            }
            
            $.pr(" %s", msg->file_path.data ? msg->file_path.data : "");
            if (msg->line_number != -1) {
                $.pr(":%d", msg->line_number);
            }
            $.pr(": %s", msg->message.data ? msg->message.data : "");
            
            if (is_selected) {
                tui_print_coloured(KORE_ANSI_RESET, "");
            }
            $.pr("\n");
            
            // Show context for selected result
            if (is_selected && msg->file_path.data && msg->line_number > 0) {
                $.pr("\n");
                tui_display_file_context(msg->file_path.data, msg->line_number, NULL);
                $.pr("\n");
            }
        }
    }
    
    array_free(filtered);
}

// Build history management
static void tui_add_to_history(KArray(BuildMessage) messages, i32 exit_code, u64 duration_ms, Arena* arena)
{
    // Free old arena if it exists
    if (build_history[history_write_index].arena) {
        arena_free(build_history[history_write_index].arena);
    }
    
    // Create new arena for this history entry
    Arena* hist_arena = KORE_ALLOC(sizeof(Arena));
    *hist_arena = arena_init();
    
    // Copy messages to history arena
    KArray(BuildMessage) hist_messages = NULL;
    for (usize i = 0; i < array_length(messages); ++i) {
        BuildMessage msg = messages[i];
        
        // Copy strings to history arena
        if (msg.file_path.data) {
            StringBuilder sb = string_builder_init(hist_arena);
            string_builder_append_string(&sb, msg.file_path);
            string_builder_null_terminate(&sb);
            msg.file_path = sb.str;
        }
        
        if (msg.message.data) {
            StringBuilder sb = string_builder_init(hist_arena);
            string_builder_append_string(&sb, msg.message);
            string_builder_null_terminate(&sb);
            msg.message = sb.str;
        }
        
        array_add(hist_messages, msg);
    }
    
    build_history[history_write_index].messages = hist_messages;
    build_history[history_write_index].timestamp = $.time_ms($.time_now());
    build_history[history_write_index].build_duration_ms = duration_ms;
    build_history[history_write_index].exit_code = exit_code;
    build_history[history_write_index].memory_used = arena->cursor; // Rough memory usage estimate
    build_history[history_write_index].arena = hist_arena;
    
    history_write_index = (history_write_index + 1) % MAX_BUILD_HISTORY;
    if (tui_state.history_count < MAX_BUILD_HISTORY) {
        tui_state.history_count++;
    }
}

static KArray(BuildMessage) tui_get_history_messages(int relative_index)
{
    if (tui_state.history_count == 0) {
        return NULL;
    }
    
    int actual_index = (history_write_index - 1 - relative_index + MAX_BUILD_HISTORY) % MAX_BUILD_HISTORY;
    if (actual_index < 0 || actual_index >= tui_state.history_count) {
        return NULL;
    }
    
    return build_history[actual_index].messages;
}

static void tui_display_performance_metrics(void)
{
    if (tui_state.history_count == 0) return;
    
    // Get current build metrics
    int current_idx = (history_write_index - 1 + MAX_BUILD_HISTORY) % MAX_BUILD_HISTORY;
    BuildHistoryEntry* current = &build_history[current_idx];
    
    $.pr("📊 Build took %llu ms, Memory: %zu bytes", 
         current->build_duration_ms, current->memory_used);
    
    // Show average if we have multiple builds
    if (tui_state.history_count > 1) {
        u64 total_duration = 0;
        usize total_memory = 0;
        
        int count = tui_state.history_count > 5 ? 5 : tui_state.history_count; // Last 5 builds
        for (int i = 0; i < count; ++i) {
            int idx = (history_write_index - 1 - i + MAX_BUILD_HISTORY) % MAX_BUILD_HISTORY;
            total_duration += build_history[idx].build_duration_ms;
            total_memory += build_history[idx].memory_used;
        }
        
        u64 avg_duration = total_duration / count;
        usize avg_memory = total_memory / count;
        
        $.pr(" (Avg: %llu ms, %zu bytes)", avg_duration, avg_memory);
    }
    
    $.pr("\n");
}

static void tui_display_split_pane(KArray(BuildMessage) messages, Arena* arena)
{
    tui_get_terminal_size();
    int pane_width = tui_state.terminal_width / 2 - 2; // Leave space for separator
    
    // Extract unique files
    KArray(String) files = tui_extract_file_list(messages, arena);
    
    // Display header
    $.pr("Files");
    for (int i = 5; i < pane_width; ++i) $.pr(" ");
    $.pr("│ Error Details\n");
    
    // Display separator line
    for (int i = 0; i < pane_width; ++i) $.pr("─");
    $.pr("┼");
    for (int i = 0; i < pane_width; ++i) $.pr("─");
    $.pr("\n");
    
    // Display file list (left pane) and selected error details (right pane)
    int max_display_lines = tui_state.terminal_height - 8; // Reserve space for header/footer
    
    for (int line = 0; line < max_display_lines; ++line) {
        // Left pane: file list
        if (line < array_length(files)) {
            String file = files[line];
            // Extract just the filename from the path
            const char* filename = strrchr(file.data, '/');
            if (!filename) filename = strrchr(file.data, '\\');
            if (!filename) filename = file.data; else filename++;
            
            char display_name[256];
            snprintf(display_name, sizeof(display_name), "%.30s", filename);
            
            // Count errors/warnings for this file
            int file_issues = 0;
            for (usize i = 0; i < array_length(messages); ++i) {
                if (messages[i].file_path.length == file.length &&
                    strncmp(messages[i].file_path.data, file.data, file.length) == 0) {
                    file_issues++;
                }
            }
            
            $.pr("%-30s (%d)", display_name, file_issues);
        } else {
            for (int i = 0; i < pane_width; ++i) $.pr(" ");
        }
        
        $.pr("│ ");
        
        // Right pane: error details for selected message
        if (tui_state.selected_message_index < array_length(messages) && line < 5) {
            BuildMessage* msg = &messages[tui_state.selected_message_index];
            
            if (line == 0 && msg->file_path.data) {
                $.pr("File: %s", msg->file_path.data);
            } else if (line == 1 && msg->line_number != -1) {
                $.pr("Line: %d", msg->line_number);
                if (msg->column_number != -1) {
                    $.pr(", Column: %d", msg->column_number);
                }
            } else if (line == 2 && msg->message.data) {
                char truncated_msg[256];
                snprintf(truncated_msg, sizeof(truncated_msg), "%.60s", msg->message.data);
                if (msg->type == MessageType_Error) {
                    tui_print_coloured(KORE_ANSI_RED, "Error: ");
                } else if (msg->type == MessageType_Warning) {
                    tui_print_coloured(KORE_ANSI_YELLOW, "Warning: ");
                }
                $.pr("%s", truncated_msg);
            }
        }
        
        $.pr("\n");
    }
    
    array_free(files);
}

static void tui_display_build_status(KArray(BuildMessage) messages)
{
    if (!messages || array_length(messages) == 0) {
        tui_display_success();
        return;
    }
    
    if (tui_state.mode == TUIMode_SplitPane) {
        tui_display_split_pane(messages, NULL);
        return;
    }
    
    if (tui_state.mode == TUIMode_Search) {
        tui_display_search_mode(messages, NULL);
        return;
    }
    
    // Check if we have any errors
    bool has_errors = false;
    for (usize i = 0; i < array_length(messages); ++i) {
        if (messages[i].type == MessageType_Error) {
            has_errors = true;
            break;
        }
    }
    
    if (has_errors) {
        tui_display_errors_navigable(messages);
    } else {
        tui_display_warnings_navigable(messages);
    }
}

//
// File watching system
//

typedef struct {
    Arena* arena;
    String watch_path;
    bool   recursive;
    bool   running;
    u64    last_change_time;
    bool   build_pending;
    CompileInfo* compile_info;

#if KORE_OS_WINDOWS
    HANDLE     directory_handle;
    HANDLE     completion_port;
    OVERLAPPED overlapped;
    BYTE       change_buffer[4096];
#else
    int inotify_fd;
    KArray(int) watch_descriptors;
#endif
} WatchInfo;

// Function pointer type for CompileInfo setup callback
typedef CompileInfo (*CompileInfoFunction)(Arena* arena);

// Watch function declaration
i32 build_watch(const char* path, CompileInfoFunction setup_func);

//
// File watching implementation
//

static volatile bool watch_should_stop = false;

static bool is_relevant_file_extension(const char* filename, CompileInfo* info)
{
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        return false;
    }

    // If no watch extensions specified, use defaults
    if (!info->watch_extensions || array_length(info->watch_extensions) == 0) {
        return strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 ||
               strcmp(ext, ".cpp") == 0 || strcmp(ext, ".hpp") == 0;
    }
    
    // Check against configured extensions
    for (usize i = 0; i < array_length(info->watch_extensions); ++i) {
        if (strcmp(ext, info->watch_extensions[i].data) == 0) {
            return true;
        }
    }
    
    return false;
}

static bool is_ignored_file(const char* filename, CompileInfo* info)
{
    if (!info->ignore_patterns || array_length(info->ignore_patterns) == 0) {
        return false;
    }
    
    // Simple pattern matching for ignore patterns
    for (usize i = 0; i < array_length(info->ignore_patterns); ++i) {
        const char* pattern = info->ignore_patterns[i].data;
        
        // Simple wildcard matching - just check if filename contains pattern
        if (strstr(filename, pattern) != NULL) {
            return true;
        }
    }
    
    return false;
}

#if KORE_OS_WINDOWS

static WatchInfo* watch_init_windows(Arena* arena, const char* path, CompileInfo* info)
{
    WatchInfo* watch        = arena_alloc(arena, sizeof(WatchInfo));
    watch->arena            = arena;
    watch->watch_path       = string_view(path);
    watch->recursive        = true;
    watch->running          = false;
    watch->last_change_time = 0;
    watch->build_pending    = false;
    watch->compile_info     = info;

    // Convert path to wide string for Windows API
    int    path_len  = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    WCHAR* wide_path = arena_alloc(arena, path_len * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, path_len);

    // Open directory for watching
    watch->directory_handle =
        CreateFileW(wide_path,
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                    NULL);

    if (watch->directory_handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open directory for watching: %s\n", path);
        return NULL;
    }

    // Create completion port for async I/O
    watch->completion_port = CreateIoCompletionPort(
        watch->directory_handle, NULL, (ULONG_PTR)watch, 0);

    if (!watch->completion_port) {
        fprintf(stderr, "Failed to create completion port\n");
        CloseHandle(watch->directory_handle);
        return NULL;
    }

    ZeroMemory(&watch->overlapped, sizeof(OVERLAPPED));

    return watch;
}

static bool watch_start_monitoring_windows(WatchInfo* watch)
{
    DWORD bytes_returned;
    BOOL  success = ReadDirectoryChangesW(
        watch->directory_handle,
        watch->change_buffer,
        sizeof(watch->change_buffer),
        watch->recursive ? TRUE : FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
        &bytes_returned,
        &watch->overlapped,
        NULL);

    return success != 0;
}

static bool watch_process_changes_windows(WatchInfo* watch, bool* files_changed)
{
    DWORD        bytes_transferred;
    ULONG_PTR    completion_key;
    LPOVERLAPPED overlapped;

    // Check for changes with timeout
    BOOL result = GetQueuedCompletionStatus(watch->completion_port,
                                            &bytes_transferred,
                                            &completion_key,
                                            &overlapped,
                                            100 // 100ms timeout
    );

    if (!result) {
        DWORD error = GetLastError();
        if (error == WAIT_TIMEOUT) {
            return true; // Continue watching
        }
        fprintf(stderr, "GetQueuedCompletionStatus failed: %lu\n", error);
        return false;
    }

    // If we get here, we received a notification
    printf("CHANGE DETECTED: %lu bytes\n", bytes_transferred);

    if (bytes_transferred == 0) {
        return true; // Continue watching
    }

    // Process file changes
    FILE_NOTIFY_INFORMATION* info =
        (FILE_NOTIFY_INFORMATION*)watch->change_buffer;

    while (info) {
        // Convert filename from wide string
        int filename_len =
            WideCharToMultiByte(CP_UTF8,
                                0,
                                info->FileName,
                                info->FileNameLength / sizeof(WCHAR),
                                NULL,
                                0,
                                NULL,
                                NULL);
        char* filename = arena_alloc(watch->arena, filename_len + 1);
        WideCharToMultiByte(CP_UTF8,
                            0,
                            info->FileName,
                            info->FileNameLength / sizeof(WCHAR),
                            filename,
                            filename_len,
                            NULL,
                            NULL);
        filename[filename_len] = '\0';

        // Check if this is a relevant file and not ignored
        if (!is_ignored_file(filename, watch->compile_info) && 
            is_relevant_file_extension(filename, watch->compile_info)) {
            *files_changed = true;
        }

        // Move to next notification
        if (info->NextEntryOffset == 0) {
            break;
        }
        info = (FILE_NOTIFY_INFORMATION*)((char*)info + info->NextEntryOffset);
    }

    // Restart monitoring
    return watch_start_monitoring_windows(watch);
}

static void watch_cleanup_windows(WatchInfo* watch)
{
    if (watch->completion_port) {
        CloseHandle(watch->completion_port);
        watch->completion_port = NULL;
    }

    if (watch->directory_handle &&
        watch->directory_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(watch->directory_handle);
        watch->directory_handle = INVALID_HANDLE_VALUE;
    }
}

#else // KORE_OS_LINUX || KORE_OS_MACOS

static WatchInfo* watch_init_linux(Arena* arena, const char* path, CompileInfo* info)
{
    WatchInfo* watch         = arena_alloc(arena, sizeof(WatchInfo));
    watch->arena             = arena;
    watch->watch_path        = string_view(path);
    watch->recursive         = true;
    watch->running           = false;
    watch->last_change_time  = 0;
    watch->build_pending     = false;
    watch->watch_descriptors = NULL;
    watch->compile_info      = info;

    // Initialize inotify
    watch->inotify_fd        = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (watch->inotify_fd == -1) {
        fprintf(stderr, "Failed to initialize inotify: %s\n", strerror(errno));
        return NULL;
    }

    return watch;
}

static bool watch_add_directory_linux(WatchInfo* watch, const char* dir_path)
{
    int wd = inotify_add_watch(watch->inotify_fd,
                               dir_path,
                               IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO |
                                   IN_MOVED_FROM);

    if (wd == -1) {
        fprintf(stderr,
                "Failed to add watch for %s: %s\n",
                dir_path,
                strerror(errno));
        return false;
    }

    array_add(watch->watch_descriptors, wd);

    // If recursive, add subdirectories
    if (watch->recursive) {
        KArray(String) subdirs = files_list(watch->arena, dir_path, false);
        for (usize i = 0; i < array_length(subdirs); ++i) {
            struct stat st;
            if (stat(subdirs[i].data, &st) == 0 && S_ISDIR(st.st_mode)) {
                watch_add_directory_linux(watch, subdirs[i].data);
            }
        }
        array_free(subdirs);
    }

    return true;
}

static bool watch_process_changes_linux(WatchInfo* watch, bool* files_changed)
{
    char    buffer[4096];
    ssize_t length = read(watch->inotify_fd, buffer, sizeof(buffer));

    if (length == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true; // No events, continue watching
        }
        fprintf(stderr, "Failed to read inotify events: %s\n", strerror(errno));
        return false;
    }

    if (length == 0) {
        return true; // No events, continue watching
    }

    // Process events
    char* ptr = buffer;
    while (ptr < buffer + length) {
        struct inotify_event* event = (struct inotify_event*)ptr;

        if (event->len > 0) {
            const char* filename = event->name;

            // Check if this is a relevant file and not ignored
            if (!is_ignored_file(filename, watch->compile_info) && 
                is_relevant_file_extension(filename, watch->compile_info)) {
                *files_changed = true;
            }

            // If a new directory was created, add it to watch list
            if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
                StringBuilder full_path = string_builder_init(watch->arena);
                string_builder_append_string(&full_path, watch->watch_path);
                string_builder_append_zstring(&full_path, "/");
                string_builder_append_zstring(&full_path, filename);
                string_builder_null_terminate(&full_path);

                watch_add_directory_linux(watch, full_path.str.data);
            }
        }

        ptr += sizeof(struct inotify_event) + event->len;
    }

    return true;
}

static void watch_cleanup_linux(WatchInfo* watch)
{
    if (watch->inotify_fd != -1) {
        // Remove all watches
        for (usize i = 0; i < array_length(watch->watch_descriptors); ++i) {
            inotify_rm_watch(watch->inotify_fd, watch->watch_descriptors[i]);
        }

        close(watch->inotify_fd);
        watch->inotify_fd = -1;
    }

    if (watch->watch_descriptors) {
        array_free(watch->watch_descriptors);
        watch->watch_descriptors = NULL;
    }
}

#endif

//
// Cross-platform signal handling
//

#if KORE_OS_WINDOWS
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
    switch (ctrl_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        watch_should_stop = true;
        return TRUE;
    default:
        return FALSE;
    }
}

static void setup_signal_handlers()
{
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
}

static void cleanup_signal_handlers()
{
    SetConsoleCtrlHandler(console_ctrl_handler, FALSE);
}
#else
static void sigint_handler(int sig)
{
    (void)sig;
    watch_should_stop = true;
}

static void setup_signal_handlers()
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

static void cleanup_signal_handlers()
{
    // Restore default SIGINT handler
    signal(SIGINT, SIG_DFL);
}
#endif

//
// Main watch function implementation
//

i32 build_watch(const char* path, CompileInfoFunction setup_func)
{
    $.init();

    Arena      watch_arena = arena_init();
    WatchInfo* watch       = NULL;

    // Initialise TUI
    if (!tui_init()) {
        $.eprn("Failed to initialise TUI");
        return 1;
    }

    // Setup signal handling for graceful shutdown
    setup_signal_handlers();

    // Setup CompileInfo using callback
    CompileInfo info = setup_func(&watch_arena);

    // Initialise platform-specific watching
#if KORE_OS_WINDOWS
    watch = watch_init_windows(&watch_arena, path, &info);
    if (!watch) {
        fprintf(stderr, "Failed to initialize file watching on Windows\n");
        goto cleanup;
    }
    if (!watch_start_monitoring_windows(watch)) {
        fprintf(stderr, "Failed to start file monitoring on Windows\n");
        goto cleanup;
    }
    printf("File watching started for: %s\n", path);
#else
    watch = watch_init_linux(&watch_arena, path, &info);
    if (!watch || !watch_add_directory_linux(watch, path)) {
        fprintf(stderr, "Failed to start file watching on Linux\n");
        goto cleanup;
    }
#endif

    watch->running = true;
    tui_clear_screen();
    $.prn("🔍 Watching directory: %s", path);
    $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
    $.prn("");

    // Initial build with capture
    tui_display_timestamp();
    tui_display_building_status();
    
    u64 build_start = $.time_ms($.time_now());
    KArray(String) initial_output = NULL;
    i32 initial_result = compile_watch(&info, &initial_output, &watch_arena);
    u64 build_duration = $.time_ms($.time_now()) - build_start;
    
    KArray(BuildMessage) initial_messages = parse_build_output(initial_output, &watch_arena);
    
    // Add initial build to history
    tui_add_to_history(initial_messages, initial_result, build_duration, &watch_arena);
    
    tui_clear_screen();
    $.prn("🔍 Watching directory: %s", path);
    $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
    $.prn("");
    
    // Show the command that was executed
    String cmd_display = compile_info_to_command(&info);
    if (cmd_display.data && cmd_display.length > 0) {
        $.prn("Command: %.*s", (int)cmd_display.length, cmd_display.data);
    } else {
        $.prn("Command: [COMMAND GENERATION FAILED]");
    }
    $.prn("");
    
    // Display initial build completion with timestamp
    tui_display_timestamp();
    if (initial_result == 0) {
        tui_print_coloured(KORE_ANSI_GREEN, "✅ Initial build completed successfully");
    } else {
        tui_print_coloured(KORE_ANSI_RED, "❌ Initial build completed with errors");
    }
    $.prn("\n\n");
    
    tui_display_build_status(initial_messages);
    $.prn("");

    // Store current state for navigation
    KArray(BuildMessage) current_messages = initial_messages;
    String current_cmd_display = cmd_display;
    
    // Main watch loop
    while (watch->running && !watch_should_stop) {
        bool files_changed = false;

        // Process file changes
#if KORE_OS_WINDOWS
        if (!watch_process_changes_windows(watch, &files_changed)) {
            fprintf(stderr, "Error processing file changes on Windows\n");
            break;
        }
#else
        if (!watch_process_changes_linux(watch, &files_changed)) {
            fprintf(stderr, "Error processing file changes on Linux\n");
            break;
        }
#endif

        // If files changed, update debounce timer
        if (files_changed) {
            watch->last_change_time = $.time_ms($.time_now());
            watch->build_pending    = true;
            
            // Display file change notification with timestamp
            tui_display_timestamp();
            tui_print_coloured(KORE_ANSI_YELLOW, "📝 File changes detected, waiting for stabilisation...");
            $.pr("\n");
        }

        // Check if we should trigger build (debounced)
        if (watch->build_pending) {
            u64 current_time = $.time_ms($.time_now());
            if (current_time - watch->last_change_time >=
                500) { // 500ms debounce
                watch->build_pending = false;
                
                // Display building status with timestamp
                tui_display_timestamp();
                tui_display_building_status();

                // Capture build output using proper compile_watch
                u64 build_start = $.time_ms($.time_now());
                KArray(String) output_lines = NULL;
                i32 result = compile_watch(&info, &output_lines, &watch_arena);
                u64 build_duration = $.time_ms($.time_now()) - build_start;
                
                current_messages = parse_build_output(output_lines, &watch_arena);
                tui_state.selected_message_index = 0; // Reset selection
                tui_state.scroll_offset = 0; // Reset scroll
                tui_state.history_index = 0; // Reset to latest
                
                // Add to history
                tui_add_to_history(current_messages, result, build_duration, &watch_arena);
                
                // Update display with results
                tui_clear_screen();
                $.prn("🔍 Watching directory: %s", path);
                $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
                $.prn("");
                $.prn("Command: %s", current_cmd_display.data);
                $.prn("");
                
                // Display build completion with timestamp
                tui_display_timestamp();
                if (result == 0) {
                    tui_print_coloured(KORE_ANSI_GREEN, "✅ Build completed successfully");
                } else {
                    tui_print_coloured(KORE_ANSI_RED, "❌ Build completed with errors");
                }
                $.pr("\n");
                
                // Show performance metrics
                tui_display_performance_metrics();
                $.pr("\n");
                
                tui_display_build_status(current_messages);
                $.prn("");
            }
        }

        // Check for keyboard input
        InputResult input = tui_check_input();
        if (input == InputResult_Quit) {
            watch->running = false;
            break;
        } else if (input == InputResult_NavigateUp) {
            if (tui_state.selected_message_index > 0) {
                tui_state.selected_message_index--;
                // Refresh display
                tui_clear_screen();
                $.prn("🔍 Watching directory: %s", path);
                $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
                $.prn("");
                $.prn("Command: %s", current_cmd_display.data);
                $.prn("");
                tui_display_build_status(current_messages);
                $.prn("");
            }
        } else if (input == InputResult_NavigateDown) {
            // Count total navigable messages
            int total_messages = 0;
            for (usize i = 0; i < array_length(current_messages); ++i) {
                if (current_messages[i].type == MessageType_Error || current_messages[i].type == MessageType_Warning) {
                    total_messages++;
                }
            }
            if (tui_state.selected_message_index < total_messages - 1) {
                tui_state.selected_message_index++;
                // Refresh display
                tui_clear_screen();
                $.prn("🔍 Watching directory: %s", path);
                $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
                $.prn("");
                $.prn("Command: %s", current_cmd_display.data);
                $.prn("");
                tui_display_build_status(current_messages);
                $.prn("");
            }
        } else if (input == InputResult_ToggleMode) {
            // Toggle between list and split-pane mode
            tui_state.mode = (tui_state.mode == TUIMode_List) ? TUIMode_SplitPane : TUIMode_List;
            
            // Refresh display
            tui_clear_screen();
            $.prn("🔍 Watching directory: %s", path);
            $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
            $.prn("");
            $.prn("Command: %s", current_cmd_display.data);
            $.prn("");
            
            // Re-display build completion status
            tui_display_timestamp();
            if (array_length(current_messages) > 0) {
                bool has_errors = false;
                for (usize i = 0; i < array_length(current_messages); ++i) {
                    if (current_messages[i].type == MessageType_Error) {
                        has_errors = true;
                        break;
                    }
                }
                if (has_errors) {
                    tui_print_coloured(KORE_ANSI_RED, "❌ Build completed with errors");
                } else {
                    tui_print_coloured(KORE_ANSI_GREEN, "✅ Build completed successfully");
                }
            } else {
                tui_print_coloured(KORE_ANSI_GREEN, "✅ Build completed successfully");
            }
            const char* mode_name = "List";
            if (tui_state.mode == TUIMode_SplitPane) mode_name = "Split-pane";
            else if (tui_state.mode == TUIMode_Search) mode_name = "Search";
            $.pr(" (Mode: %s)\n\n", mode_name);
            
            tui_display_build_status(current_messages);
            $.prn("");
        } else if (input == InputResult_StartSearch) {
            // Enter search mode
            tui_state.mode = TUIMode_Search;
            tui_state.search_query[0] = '\0'; // Clear search
            tui_state.selected_message_index = 0;
            
            // For now, just display search mode - in a full implementation you'd handle text input
            // Refresh display
            tui_clear_screen();
            $.prn("🔍 Watching directory: %s", path);
            $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
            $.prn("");
            $.prn("Command: %s", current_cmd_display.data);
            $.prn("");
            
            tui_display_build_status(current_messages);
            $.prn("");
        } else if (input == InputResult_HistoryPrev) {
            // Navigate to older build in history
            if (tui_state.history_index < tui_state.history_count - 1) {
                tui_state.history_index++;
                KArray(BuildMessage) hist_messages = tui_get_history_messages(tui_state.history_index);
                if (hist_messages) {
                    current_messages = hist_messages;
                    tui_state.selected_message_index = 0;
                    
                    // Refresh display
                    tui_clear_screen();
                    $.prn("🔍 Watching directory: %s", path);
                    $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
                    $.prn("");
                    $.prn("Command: %s", current_cmd_display.data);
                    $.prn("");
                    
                    tui_display_timestamp();
                    if (tui_state.history_index == 0) {
                        tui_print_coloured(KORE_ANSI_CYAN, "📅 Current build");
                    } else {
                        char hist_info[64];
                        snprintf(hist_info, sizeof(hist_info), "📅 History #%d (of %d)", 
                                tui_state.history_index + 1, tui_state.history_count);
                        tui_print_coloured(KORE_ANSI_CYAN, hist_info);
                    }
                    $.pr("\n\n");
                    
                    tui_display_build_status(current_messages);
                    $.prn("");
                }
            }
        } else if (input == InputResult_HistoryNext) {
            // Navigate to newer build in history
            if (tui_state.history_index > 0) {
                tui_state.history_index--;
                KArray(BuildMessage) hist_messages = tui_get_history_messages(tui_state.history_index);
                if (hist_messages) {
                    current_messages = hist_messages;
                    tui_state.selected_message_index = 0;
                    
                    // Refresh display
                    tui_clear_screen();
                    $.prn("🔍 Watching directory: %s", path);
                    $.prn("Press Ctrl+C/'q' to quit, 't' to toggle view, '/' to search, 'h'/'j' for history.");
                    $.prn("");
                    $.prn("Command: %s", current_cmd_display.data);
                    $.prn("");
                    
                    tui_display_timestamp();
                    if (tui_state.history_index == 0) {
                        tui_print_coloured(KORE_ANSI_CYAN, "📅 Current build");
                    } else {
                        char hist_info[64];
                        snprintf(hist_info, sizeof(hist_info), "📅 History #%d (of %d)", 
                                tui_state.history_index + 1, tui_state.history_count);
                        tui_print_coloured(KORE_ANSI_CYAN, hist_info);
                    }
                    $.pr("\n\n");
                    
                    tui_display_build_status(current_messages);
                    $.prn("");
                }
            }
        }

        // Small sleep to prevent excessive CPU usage
#if KORE_OS_WINDOWS
        Sleep(10);
#else
        usleep(10000); // 10ms
#endif
    }

cleanup:
    // Cleanup TUI
    tui_cleanup();
    
    // Cleanup resources
    if (watch) {
#if KORE_OS_WINDOWS
        watch_cleanup_windows(watch);
#else
        watch_cleanup_linux(watch);
#endif
    }

    cleanup_signal_handlers();
    arena_free(&watch_arena);

    $.prn("File watching stopped.");
    return 0;
}
