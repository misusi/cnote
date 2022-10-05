/*
TODO: Fix: on long lines, will sometimes print the previous line in curses
*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <assert.h>
#include <ncurses.h>
#define MAX_LINE_INPUT_LENGTH 300

typedef struct LINE
{
    char * string;
    struct LINE *prev;
    struct LINE *next;
    int length;
} LINE;
typedef struct LINE_LIST
{
    LINE *first;
    LINE *last;
    int length;
    int capacity;
} LINE_LIST;

static FILE *fp;
static WINDOW* subwindow;
static LINE_LIST line_list;
void trim_whitespace(char *str);
void substring(char *str, int start_index, int end_index);
void clearScreen();
void show_prompt(int tablevel);
void quit(FILE *p_file_pointer, WINDOW *win, LINE_LIST* line_list);
void sigIntHandler(int dummy);
void reset_term();
void line_list_init(LINE_LIST *line_list);
void line_list_add_line(LINE_LIST *line_list, LINE *line);
void free_list(LINE_LIST* list);

int main(int argc, char **argv)
{
    // Check args
    if (argc < 2)
    {
        printf("Usage: %s filename\n", argv[0]);
        return 1;
    }

    signal(SIGINT, sigIntHandler);

    char char_set_indent = '>';
    char char_set_outdent = '<';
    char char_bullet = '-';

    // ncurses
    initscr();
    cbreak();
    subwindow = newwin(LINES - 1, COLS, 1, 0);

    // list of line inputs
    line_list_init(&line_list);

    // Check if file already exists
    if (!access(argv[1], F_OK))
    {
        mvprintw(0, 0, "%s \"%s\" %s ", "File", argv[1],
                 "already exists. Overwrite, append, or neither (exit)? (o/a/N): ");
        char r[20];
        getstr(r);
        trim_whitespace(r);
        if (r > 0)
        {
            if (tolower(r[0]) == 'n')
            {
                reset_term();
                exit(1);
            }
            else if (tolower((r[0] == 'o')))
            {
                fp = fopen(argv[1], "w+");
            }
            else if (tolower((r[0] == 'a')))
            {
                fp = fopen(argv[1], "a+");
                fprintf(fp, "%s", "\n");
            }
            else
            {
                reset_term();
                exit(1);
            }
        }
    }
    else
    {
        fp = fopen(argv[1], "w+");
    }


    // fprint datetime
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char s[64];
    size_t ret = strftime(s, sizeof(s), "%c", tm);
    assert(ret);
    char divider[strlen(s) + 1];
    for (int i = 0; i < strlen(s); i++)
    {
        divider[i] = '-';
    }
    divider[strlen(s)] = '\0';
    fprintf(fp, "\n%s\n%s\n%s\n\n", divider, s, divider);

    // input loop
    short curr_indent_level = 0;
    short saved_indent_level = curr_indent_level;
    while (1)
    {

        // ncurses print skeleton
        clear();
        mvprintw(0, 0, "\rcnote: ");
        box(subwindow, 0, 0);
        refresh();
        wrefresh(subwindow);

        // get input
        LINE *new_line = malloc(sizeof(LINE));
        char input[100] = "";
        getstr(input);
        trim_whitespace(input);

        // check line for keywords (exit, quit, ...)
        if (!strcmp(input, "quit"))
        {
            quit(fp, subwindow, &line_list);
        }
        if (!strcmp(input, "exit"))
        {
            quit(fp, subwindow, &line_list);
        }

        // check input instruction (indent, outdent, reset, header, normal)
        int curr_index = 0;
        int input_start_index = 0;
        if (input[curr_index] == char_set_indent)
        {
            curr_indent_level++;
            input_start_index = 1;
        }
        else if (input[curr_index] == char_set_outdent)
        {
            input_start_index = 1;
            if (--curr_indent_level  < 0)
            {
                curr_indent_level = 0;
            }
            if (input[curr_index + 1] && input[curr_index + 1] == '>')
            {
                curr_indent_level = 0;
                input_start_index = 2;
            }
        }
        // Add tabs, header stuff
        substring(input, input_start_index, strlen(input));
        char output[MAX_LINE_INPUT_LENGTH] = "";
        for (int i = 0; i < curr_indent_level; i++)
        {
            output[i] = '\t';
        }
        strcat(output, input);

        // save line to list
        new_line->string = malloc(sizeof(strlen(output)) + 1);
        new_line->length = strlen(output);
        strcpy(new_line->string, output);
        line_list_add_line(&line_list, new_line);

        // write line to file
        fprintf(fp, "%s\n", new_line->string);

        // visual
        clear();
        wclear(subwindow);
        box(subwindow, 0, 0);
        LINE *current_line = line_list.first;

        // printing display
        int offset_y = 0, offset_x = 1;
        int n = 0;
        if (line_list.length > LINES - 3)
        {
            current_line = line_list.last;
            while (n < LINES - 2)
            {
                mvwprintw(subwindow, LINES - 3 - n, offset_x, current_line->string);
                current_line = current_line->prev;
                n++;
            }
        }
        else
        {
            int i = 1;
            while (current_line != NULL)
            {
                mvwprintw(subwindow, i++, 1, current_line->string);
                current_line = current_line->next;
            }
        }
        refresh();
        wrefresh(subwindow);
    }
    quit(fp, subwindow, &line_list);
    exit(0);
} // END main

void substring(char *str, int start_index, int end_index)
{
    if (start_index < 0) start_index = 0;
    if (end_index > strlen(str) - 1) end_index = strlen(str) - 1;

    int n = 0;
    for (int i = start_index; i < end_index + 1; i++)
    {
        str[n] = str[n + start_index];
        n++;
    }
    str[end_index - start_index + 1] = '\0';
}
void trim_whitespace(char *str)
{
    int str_len = strlen(str);
    if (str_len == 0) return;
    int first_non_space_index = 0;
    int last_non_space_index = str_len - 1;
    while (first_non_space_index < str_len)
    {
        if (!isspace(str[first_non_space_index])) break;
        first_non_space_index++;
    }
    if (first_non_space_index < str_len - 1)
    {
        while (last_non_space_index > 0)
        {
            if (!isspace(str[last_non_space_index])) break;
            last_non_space_index--;
        }
    }
    substring(str, first_non_space_index, last_non_space_index);
}
void clearScreen()
{
    const char *CLEAR_SCREEN_ANSI = "\e[1;1H\e[2J";
    write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 12);
}
void show_prompt(int tablevel)
{
    clearScreen();
    char prompt[100] = "";
    for (int i = 0; i < tablevel; i++)
    {
        strcat(prompt, "\t");
    }
    strcat(prompt, "cnote: ");
    printf("\r%s", prompt);
}
void quit(FILE * p_file_pointer, WINDOW * win, LINE_LIST * line_list)
{
    free_list(line_list);
    delwin(win);
    endwin();
    fclose(p_file_pointer);
    clearScreen();
    printf("\r");
    reset_term();
    exit(0);
}
void sigIntHandler(int dummy)
{
    quit(fp, subwindow, &line_list);
}
void reset_term()
{
    move(0, 0);
    clear();
    erase();
    refresh();
    fflush(stdout);
}

void line_list_init(LINE_LIST * line_list)
{
    line_list->capacity = 0;
    line_list->length = 0;
    line_list->first = NULL;
    line_list->last = NULL;
}

void line_list_add_line(LINE_LIST * line_list, LINE * line)
{
    if (line_list->length == 0)
    {
        line->prev = NULL;
        line->next = NULL;
        line_list->first = line;
        line_list->last = line;
    }
    else
    {
        line->prev = line_list->last;
        line->next = NULL;
        line->prev->next = line;
        line_list->last = line;
    }
    line_list->length++;
}

void free_list(LINE_LIST * list)
{
    if (list->length == 0) return;
    else
    {
        if (list->length == 1)
        {
            free(list->first->string);
        }
        else
        {
            LINE *current_line = list->first;
            while (current_line != list->last)
            {
                free(current_line->string);
                current_line = current_line->next;
                free(current_line->prev);
            }
            free(list->last->string);
            free(list->last);
        }
    }
}
