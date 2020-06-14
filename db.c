//https://cstack.github.io/db_tutorial/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Set up input buffer struct, we track:
A pointer to a character buffer
The current length of the buffer
The length of the current input */
typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

/* enum that holds our statement types */
typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

/* Defined consts for our max username and email size */
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

//Plus one here is for the null terminator
typedef struct{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

/* struct that holds a statement, which possesses a statement type property*/
typedef struct {
    StatementType type;
    Row row_to_insert;
} Statement;

/*enum for holding the result of our analysis of our meta command (any command starting with '.') */
typedef enum{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
} ExecuteResult;

/* enum for holding the result of our analysis of any sql commands (any command not starting with '.')*/
typedef enum {
    PREPARE_SUCCESS,
    PREPARE_STRING_TOO_LONG,
    PREPARE_NEGATIVE_ID,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

/* Cheeky macro to get the size of a struct attibute, i believe the sizeof(((Struct*)0)->Attribute) part is doing the following:
 * 1. Get the pointer to the struct
 * 2. Cast it to the address 0
 * 3. Get the position of the Attribute pointer (from 0) assuming that the struct starts at address 0 (as cast in step 2)
 * 4. Use this calculated offset to pass to sizeof, which gives us the size in bytes of our attribute */
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute) 
//Here we are just getting the size of each attribute and using it to create string offsets to parse our row input data
const uint32_t ID_SIZE         = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE   = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE      = size_of_attribute(Row, email);
const uint32_t ID_OFFSET       = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET    = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE        = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

/* Both of the following function take two pointers, a source and a destination.
 * They copy our id, username, email from the source struct to the destination 
 * memcpy takes (void *dest, void * src, size_t n) */
void serialize_row(Row* source, void* destination) {
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}
//The &(destination->id) here is simply getting the memory address of each struct attribute, so that we can the contents of source into it 
void deserialize_row(void* source, Row* destination) {
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

/* Sets page size to 4Kb, as this is the same size as a page used in the VM of most computer architectures. 
 * This means the OS will move pages in and out of memory as whole  units instead of breaking them up. */
const uint32_t PAGE_SIZE = 4096;
/* Not sure why we would use const here and #define for the max pages, but there are a couple differences to note:
 * 1. define is a preprocessor directive while const is a keyword, meaning that defines are usable in other files
 * 2. const is scoped whilst define is not and is only available within the function it is declared in
 * 3. Macros can be defined and redefined but a const cannot */
#define TABLE_MAX_PAGES 100
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct {
    uint32_t num_rows;
    void* pages[TABLE_MAX_PAGES];
} Table;

void print_row(Row* row) {
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

/* This is how we figure out where to read/write in memory for a given row */
void* row_slot(Table* table, uint32_t row_num) {
    //Find the number of the page
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    //Get the page
    void* page = table->pages[page_num];
    //If the page is empty, allocate memory for it
    if(page == NULL) {
        //Allocate memory only when we try to access a page
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }
    /* This looks a tiny bit odd, but what we're actually doing is:
     * 1. Take the row number within the page
     * 2. Modulo it against the amount of rows in a page
     * 3. This way, the remainder is the offset from the 'last' page 
     * 4. e.g if row_num was 22 and our rows per page was 10, we would end up with 2 (offset of 2 from the third page)
     * */
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    //Get the byte offset (within the page) for this row
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return page + byte_offset;
}

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

PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement){
    statement->type = STATEMENT_INSERT;

    char* keyword = strtok(input_buffer->buffer, " ");
    char* id_string = strtok(NULL, " ");
    char* username  = strtok(NULL, " ");
    char* email = strtok(NULL, " ");

    if (id_string == NULL || username == NULL || email == NULL) {
        return PREPARE_SYNTAX_ERROR;
    }

    int id = atoi(id_string);
    if(id < 0) {
        return PREPARE_NEGATIVE_ID;
    }
    if (strlen(username) > COLUMN_USERNAME_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }

    if (strlen(email) > COLUMN_EMAIL_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);

    return PREPARE_SUCCESS;
}

/*Prepare the result of our sql query, takes in a pointer to our input buffer and a pointer to a statement
struct. We set the value of this statement struct based on our input buffer and then return a PrepareResult
enum which defines if it was succesful or not*/
PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement){
    if(strncmp(input_buffer->buffer, "insert", 6) == 0){
        return prepare_insert(input_buffer, statement);
    }
    if(strncmp(input_buffer->buffer, "select", 6) == 0){
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

//Take an insert statement and insert the row into given table
ExecuteResult execute_insert(Statement* statement, Table* table) {
    if (table->num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);

    serialize_row(row_to_insert, row_slot(table, table->num_rows));
    table->num_rows += 1;

    return EXECUTE_SUCCESS;
}

/* Take a statement and a table, init a row and then for each row in the table, deserialize the row into our row var and print it */
ExecuteResult execute_select(Statement* statement, Table* table) {
    Row row;
    for (uint32_t i = 0; i < table->num_rows; i++) {
        deserialize_row(row_slot(table, i), &row);
        print_row(&row);
    }
    return EXECUTE_SUCCESS;
}

/* Very similar to our do_eta_command function, will become a large switch that performs actions based 
on the type of our requested statement */
ExecuteResult execute_statement(Statement* statement, Table* table){
    switch(statement->type) {
        case(STATEMENT_INSERT):
            return execute_insert(statement, table);
        case(STATEMENT_SELECT):
            return execute_select(statement, table);
    }
}

Table* new_table() {
    Table* table = malloc(sizeof(Table));
    table->num_rows = 0;
    //Init all the pages of our table to null
    for(uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        table->pages[i] = NULL;
    }
    return table;
}

void free_table(Table* table) {
    for (int i = 0; table->pages[i]; i++) {
        free(table->pages[i]);
    }
    free(table);
}

int main (int argc, char* argv[]){
    Table* table = new_table();
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

        Statement statement;
        /* Pass our input buffer and our statement into prepare_statement, statement will be set
        Here as we pass in pointer to our struct, input buffer is also a pointer but we dont
        need to preceed with & as it is already a pointer (as appose to statement which is
        actually a struct, rather than a pointer */
        switch(prepare_statement(input_buffer, &statement)){
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_NEGATIVE_ID):
                printf("ID must be positive.\n");
                continue;
            case (PREPARE_STRING_TOO_LONG):
                printf("ID must be positive.\n");
                continue;
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statements.\n");
                continue;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s' . \n", input_buffer->buffer);
                continue;
        }

        switch(execute_statement(&statement, table)) {
            case (EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case (EXECUTE_TABLE_FULL):
                printf("Error: Table full.\n");
                break;
        }
    }
}
