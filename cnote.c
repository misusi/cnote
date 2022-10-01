/*
TODO:
    [-] Allow for appending file option (adding onto same set of notes)
    [-] Split terminal horiz with ncurses: top=prompt, bottom=read out file
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
#define MAX_LINE_INPUT_LENGTH 300

static FILE *fp;
char *trim_whitespace(char *str);
void clearScreen();
void show_prompt(int tablevel);
void quit(FILE * p_file_pointer);
void sigIntHandler(int dummy);

int main(int argc, char **argv)
{
    // Check args
    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        return 1;
    }
    // Check file
    if (!access(argv[1], F_OK)) {
        // already exists
        int overwrite_ok = 0;
        printf(
            "%s already exists. Should it be overwritten? (y/N): ", argv[1]);
        char r[20];
        fgets(r, sizeof r, stdin);
        if (strlen(trim_whitespace(r)) > 0) {
            if (tolower(r[0]) == 'y') {
                overwrite_ok = 1;
            }
        }
        if (!overwrite_ok) {
            printf("\n");
            return 1;
        }
    }
    fp = fopen(argv[1], "w+");
    pid_t pid = fork(); // fork to handle sigint (ctrl+c) during blocking fgets
    int status;
    signal(SIGINT, sigIntHandler);
    if (pid < 0) printf("Error: Could not fock process\n");

    // parent -- handles all of main program
    if (pid == 0) {
        // reserved chars and predefined strings
        char char_set_indent = '>';
        char char_set_outdent = '<';
        char char_set_heading = '^';
        char char_bullet = '-';
        char char_heading = '=';

        // fprint datetime
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char s[64];
        size_t ret = strftime(s, sizeof(s), "%c", tm);
        assert(ret);
        fprintf(fp, "%s\n\n", s);

        // input loop
        char input[MAX_LINE_INPUT_LENGTH];
        short curr_indent_level = 0;
        short saved_indent_level = curr_indent_level;
        while (1) {

            // general
            show_prompt(curr_indent_level);
            void *fgets_result = fgets(input, sizeof(input), stdin);
            if (fgets_result == NULL) break; // ctrl + d = EOF
            char *trimmed_line = trim_whitespace(input);
            if (!strcmp(trimmed_line, "exit")) break;
            if (!strcmp(trimmed_line, "quit")) break;

            // get line instruction (indent, outdent, reset, header, normal)
            int curr_index = 0;
            int trimmed_line_start_index = 0;
            int line_is_header = 0;
            if (trimmed_line[curr_index] == char_set_indent) {
                curr_indent_level++;
                trimmed_line_start_index = 1;
            } else if (trimmed_line[curr_index] == char_set_outdent) {
                if (--curr_indent_level < 0)  {
                    curr_indent_level = 0;
                }
                if (trimmed_line[curr_index + 1] &&
                        trimmed_line[curr_index + 1] == '>') {
                    curr_indent_level = 0;
                    trimmed_line_start_index = 2;
                } else {
                    trimmed_line_start_index = 1;
                }
            } else if (trimmed_line[curr_index] == char_set_heading) {
                trimmed_line_start_index = 1;
                line_is_header = 1;
            }

            // format line
            char print_line[MAX_LINE_INPUT_LENGTH] = "";
            int print_line_index = 0;
            int print_line_len =
                strlen(trimmed_line) - trimmed_line_start_index + 1;
            if (line_is_header) {
                print_line[print_line_index++] = '\n';
                for (int i = 0; i < strlen(trimmed_line) - 1; i++) {
                    print_line[print_line_index++] = char_heading;
                }
                print_line[print_line_index++] = '\n';
                for (int i = trimmed_line_start_index;
                        i < strlen(trimmed_line);
                        i++) {
                    print_line[print_line_index++] = trimmed_line[i];
                }
                print_line[print_line_index++] = '\n';
                for (int i = 0; i < strlen(trimmed_line) - 1; i++) {
                    print_line[print_line_index++] = char_heading;
                }
                print_line[print_line_index++] = '\n';
                print_line[print_line_index++] = '\0';
                line_is_header = 0;
                curr_indent_level = 0;
            } else {
                for (int i = 0; i < curr_indent_level; i++) {
                    print_line[print_line_index++] = '\t';
                }
                for (int i = trimmed_line_start_index;
                        i < strlen(trimmed_line);
                        i++) {
                    print_line[print_line_index++] = trimmed_line[i];
                }
                strcat(print_line, "\0");
            }
            // write to file
            fprintf(fp, "%s\n", print_line);
        } // END while
        quit(fp);
    } else {
        waitpid(pid, &status, 0); // a child's only purpose is to wait... '~'
    } // END if pid == 0 condition
    exit(0);
} // END main

char *trim_whitespace(char *str)
{
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) // All spaces?
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
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
    for (int i = 0; i < tablevel; i++){
        strcat(prompt,"\t");
    }
    strcat(prompt, "cnote: ");
    printf("\r%s",prompt);
}
void quit(FILE *p_file_pointer)
{
    fclose(p_file_pointer);
    clearScreen();
    printf("\r");
    exit(0);
}
void sigIntHandler(int dummy)
{
    quit(fp);
}
