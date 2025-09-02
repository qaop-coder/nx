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

typedef struct {
    bool colour_support;
    bool initialised;
    int  selected_message_index;
    int  scroll_offset;
#if KORE_OS_LINUX
    struct termios original_termios;
#endif
} TUIState;

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

static int tui_get_terminal_height(void)
{
#if KORE_OS_WINDOWS
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
    return 25; // Default fallback
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_row;
    }
    return 25; // Default fallback
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

typedef enum {
    InputResult_Continue,
    InputResult_Quit,
    InputResult_NavigateUp,
    InputResult_NavigateDown
} InputResult;

static InputResult tui_check_input(void)
{
#if KORE_OS_WINDOWS
    if (_kbhit()) {
        char ch = _getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) { // 27 = ESC
            return InputResult_Quit;
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
            }
        }
    }
#endif
    return InputResult_Continue;
}

//
// Build message parsing
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
    int terminal_height = tui_get_terminal_height();
    int max_display_lines = terminal_height - 10; // Reserve space for header/footer
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
    int terminal_height = tui_get_terminal_height();
    int max_display_lines = terminal_height - 10; // Reserve space for header/footer
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

static void tui_display_build_status(KArray(BuildMessage) messages)
{
    if (!messages || array_length(messages) == 0) {
        tui_display_success();
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

static bool is_relevant_file_extension(const char* filename)
{
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        return false;
    }

    return strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 ||
           strcmp(ext, ".cpp") == 0 || strcmp(ext, ".hpp") == 0;
}

#if KORE_OS_WINDOWS

static WatchInfo* watch_init_windows(Arena* arena, const char* path)
{
    WatchInfo* watch        = arena_alloc(arena, sizeof(WatchInfo));
    watch->arena            = arena;
    watch->watch_path       = string_view(path);
    watch->recursive        = true;
    watch->running          = false;
    watch->last_change_time = 0;
    watch->build_pending    = false;

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

        // Check if this is a relevant file
        if (is_relevant_file_extension(filename)) {
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

static WatchInfo* watch_init_linux(Arena* arena, const char* path)
{
    WatchInfo* watch         = arena_alloc(arena, sizeof(WatchInfo));
    watch->arena             = arena;
    watch->watch_path        = string_view(path);
    watch->recursive         = true;
    watch->running           = false;
    watch->last_change_time  = 0;
    watch->build_pending     = false;
    watch->watch_descriptors = NULL;

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

            // Check if this is a relevant file
            if (is_relevant_file_extension(filename)) {
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

    // Initialise platform-specific watching
#if KORE_OS_WINDOWS
    watch = watch_init_windows(&watch_arena, path);
    if (!watch || !watch_start_monitoring_windows(watch)) {
        fprintf(stderr, "Failed to start file watching on Windows\n");
        goto cleanup;
    }
#else
    watch = watch_init_linux(&watch_arena, path);
    if (!watch || !watch_add_directory_linux(watch, path)) {
        fprintf(stderr, "Failed to start file watching on Linux\n");
        goto cleanup;
    }
#endif

    watch->running = true;
    tui_clear_screen();
    $.prn("🔍 Watching directory: %s", path);
    $.prn("Press Ctrl+C or 'q' to stop watching.");
    $.prn("");

    // Setup CompileInfo using callback
    CompileInfo info = setup_func(&watch_arena);

    // Initial build with capture
    $.prn("⚙️  Running initial build...");
    KArray(String) initial_output = NULL;
    i32 initial_result = compile_watch(&info, &initial_output, &watch_arena);
    
    KArray(BuildMessage) initial_messages = parse_build_output(initial_output, &watch_arena);
    
    tui_clear_screen();
    $.prn("🔍 Watching directory: %s", path);
    $.prn("Press Ctrl+C or 'q' to stop watching.");
    $.prn("");
    
    // Show the command that was executed
    String cmd_display = compile_info_to_command(&info);
    $.prn("Command: %s", cmd_display.data);
    $.prn("");
    
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
            $.prn("📝 File changes detected, waiting for stabilisation...");
        }

        // Check if we should trigger build (debounced)
        if (watch->build_pending) {
            u64 current_time = $.time_ms($.time_now());
            if (current_time - watch->last_change_time >=
                500) { // 500ms debounce
                watch->build_pending = false;
                $.prn("⚙️  Changes stabilised, triggering build...");

                // Capture build output using proper compile_watch
                KArray(String) output_lines = NULL;
                i32 result = compile_watch(&info, &output_lines, &watch_arena);
                
                current_messages = parse_build_output(output_lines, &watch_arena);
                tui_state.selected_message_index = 0; // Reset selection
                tui_state.scroll_offset = 0; // Reset scroll
                
                // Update display with results
                tui_clear_screen();
                $.prn("🔍 Watching directory: %s", path);
                $.prn("Press Ctrl+C or 'q' to stop watching.");
                $.prn("");
                $.prn("Command: %s", current_cmd_display.data);
                $.prn("");
                
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
                $.prn("Press Ctrl+C or 'q' to stop watching.");
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
                $.prn("Press Ctrl+C or 'q' to stop watching.");
                $.prn("");
                $.prn("Command: %s", current_cmd_display.data);
                $.prn("");
                tui_display_build_status(current_messages);
                $.prn("");
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
