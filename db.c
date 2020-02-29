//https://cstack.github.io/db_tutorial/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set up input buffer struct, we track:
A pointer to a character buffer
The current length of the buffer
The length of the current input */
typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

/* enum that holds our statenent types */
typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

/* struct that holds a statement, which possesses a statement type property*/
typedef struct {
    StatementType type;
} Statement;

/*enum for holding the result of our analysis of our meta command (any command starting with '.') */
typedef enum{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

/* enum for holding the result of our analysis of any sql commands (any command not starting with '.')*/
typedef enum {
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

/* Function for creating a new input buffer pointer:
1. Create a pointer to a new input buffer, with memory allocated for the size of the buffer
2. init the buffer to null, and the length of the buffer and it's input to 0
3. Return our new input buffer pointer*/
InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

/* Function for reading input into our input_buffer struct:
1. use getline() (from stdio.h) to read in line from the user, we must pass in a pointer where we
want to store the line (at the address of our buffer struct), a size (the size of our buffer) and a stream to take input from (stdin)
2. getline() returns the number of chars read, so if we have read 0 chars then we assume there is an error so we can exit 
3. Otherwise, we want our buffers input length to be the length of bytes read (-1 to avoid trailing newline) and our buffer to contain the bytes read and the last char set to 0 (unsure why)*/
void read_input(InputBuffer* input_buffer){
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);
    if(bytes_read <= 0){
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read -1] = 0;
}

/* When closing our input buffer, we want to free the memory it is using up from our malloc() call */
void close_input_buffer(InputBuffer* input_buffer){
    free(input_buffer->buffer);
    free(input_buffer);
}

/* Simple function just for printing out a prompt for the user to enter a string */
void print_prompt() { printf("db > "); }

/* function for peforming our meta commands, will essentially become a large switch taht holds
all the different options based on the command passed in (with . before it)
Will return a MetaCommandResult*/
MetaCommandResult do_meta_command(InputBuffer* input_buffer){
    if(strcmp(input_buffer->buffer, ".exit") == 0){
        exit(EXIT_SUCCESS);
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

/*Prepare the result of our sql query, takes in a pointer to our input buffer and a pointer to a statement
struct. We set the value of this statement struct based on our input buffer and then return a PrepareResult
enum which defines if it was succesful or not*/
PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement){
    if(strncmp(input_buffer->buffer, "insert", 6) == 0){
        statement->type = STATEMENT_INSERT;
        return PREPARE_SUCCESS;
    }
    if(strncmp(input_buffer->buffer, "select", 6) == 0){
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

/* Very similar to our do_eta_command function, will become a large switch that performs actions based 
on the type of our requested statement */
void execute_statement(Statement* statement){
    switch(statement->type) {
        case(STATEMENT_INSERT):
            printf("This is where we would do an insert.\n");
            break;
        case(STATEMENT_SELECT):
            printf("This is where we would do a select.\n");
            break;
    }
}

int main (int argc, char* argv[]){
    InputBuffer* input_buffer = new_input_buffer();
    while(true){
        print_prompt();
        read_input(input_buffer);
        //If this is a command
        if(input_buffer->buffer[0] == '.'){
            switch(do_meta_command(input_buffer)) {
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrecognized command, '%s'\n", input_buffer->buffer);
                    continue;
            }
        }

        //Create a new statement struct
        Statement statement;
        //Pass our input buffer and our statement into prepare_statement, statement will be set
        //Here as we pass in pointer to our struct, input buffer is also a pointer but we dont
        //need to preceed with & as it is already a pointer (as appose to statement which is
        //actually a struct, rather than a pointer
        switch(prepare_statement(input_buffer, &statement)){
            case(PREPARE_SUCCESS):
                break;
            case(PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s' . \n", input_buffer->buffer);
                continue;
        }

        execute_statement(&statement);
        printf("Executed.\n:");
    }
}
