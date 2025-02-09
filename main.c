#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <netdb.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>

struct wordListNode;
struct gameListNode;
int initialization();
void capitalizeString();
void acceptInput(char *move);
bool isDone();
void tearDown();
bool compareCounts(int letterChoices[26], int letterCandidates[26]);
void addToList();
struct wordListNode* getRandomWord(int numberToSearch);
void findWords(struct wordListNode * masterWord);
void addToGameList(char *word);
char* checkInputOnGameList(char *string);
void cleanupWordListNodes();
void cleanupGameListNodes();
void wordsToUnderscores(char *word);

struct wordListNode{
    char word[30];
    struct wordListNode *next;
};

struct gameListNode{
    char word[30];
    bool wordHasBeenFound;
    struct gameListNode *next;
};

struct wordListNode *dictionaryRoot;
struct gameListNode *gameListRoot;
struct wordListNode *currentWord;
struct gameListNode *currentWordInGameList;
struct wordListNode *masterWordNode;

int wordCount = 0;

#define PORT "7777"
#define BUFFER_SIZE (4096 * 3)

char *mainAddress;

void makeServer(char *address);
void *requestHandler(void *client_fd);
void generateGameProgressHTML(char *buffer, int buffer_size);

int main(int argc, char *argv[]) {
    //Type in path for file, if the http request is /words then game will start else it will try to retrieve the file
//    if (argc < 2) {
//        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
//        perror("Type in path for file when running the program, if the http request is \"/words\" then game will start, else it will try to retrieve the file");
//        exit(1);
//    }
    mainAddress = "/bryanrosales";
    mainAddress = argv[1];
    makeServer(mainAddress);

    return 0;
}
//Makes server and threads to handle multiple requests at the same time
void makeServer(char *address){
    int server_fd, client_fd;
    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;
    socklen_t client_len = sizeof(client_addr);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        perror("ERROR in getaddrinfo");
        exit(1);
    }

    server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    if (bind(server_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("ERROR on binding");
        close(server_fd);
        exit(1);
    }

    listen(server_fd, 5);
    printf("Server started at http://localhost:%s/ serving files from %s\n", PORT, address);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_len);
        if (client_fd < 0) {
            perror("ERROR on accept");
        } else {
            pthread_t id;
            int *client_fd_ptr = malloc(sizeof(int));
            *client_fd_ptr = client_fd;
            if (pthread_create(&id, NULL, requestHandler, client_fd_ptr) != 0) {
                perror("ERROR creating thread");
                close(client_fd);
                free(client_fd_ptr);
            }
        }
    }
}
//Function handles incoming requests and returns a response accordingly
//If the http request has /words as the path, then it will look for a guess from the user.
//If there is no guess then the game will start, otherwise the guess will be checked and displayed if it was correct
//If the http request is something other than /words, it will consider it a file path and will try to retrieve that file
//from the path provided.
void *requestHandler(void *client_fd_ptr) {
    int client_fd = *((int *) client_fd_ptr);
    char buffer[BUFFER_SIZE];
    char guess[100] = {0};
    memset(buffer, 0, BUFFER_SIZE);

    ssize_t bytes_received = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_received <= 0) {
        fprintf(stderr, "Error reading from client.\n");
        close(client_fd);
        return NULL;
    }

    char *temp;
    char *method = strtok_r(buffer, " ", &temp);
    char *path = strtok_r(NULL, " ", &temp);

//    printf("Method is: %s\n", method);
//    printf("Path is: %s\n", path);

    char full_path[BUFFER_SIZE];
    snprintf(full_path, BUFFER_SIZE, "%s%s", mainAddress, path);
    struct stat file_stat;

    if (method == NULL || path == NULL) {
        fprintf(stderr, "Invalid HTTP request.\n");
        close(client_fd);
        return NULL;
    }
    if (strncmp(path, "/words", 6) == 0 && strcmp(method, "GET") == 0) {
        char *query = strchr(path, '?');
        char move[50] = {0};

//        printf("Path: %s\n", path);
//        if (query) {
//            printf("Query: %s\n", query);
//        } else {
//            printf("No query string found.\n");
//        }
        //If it finds word from the user, it'll extract it from the url
        if (query) {
            query++;
            //printf("Processed query: '%s'\n", query);

            if (sscanf(query, "move=%49s", move) == 1) {
                //printf("Move extracted: %s\n", move);

                capitalizeString(move);

                memset(guess, '\0', sizeof(guess));
                sprintf(guess, "%s", checkInputOnGameList(move));

            } else {
                printf("Failed to extract move from query: %s\n", query);
            }
          //If there is no guess, then a brand-new game will start
        } else if (!query) {
            tearDown();
            wordCount = initialization();
            //printf("word count = %d\n", wordCount);
            masterWordNode = getRandomWord(wordCount);
            //printf("master word: %s\n", masterWordNode->word);
            findWords(masterWordNode);
        }
        //If all the words are found, a congratulations message will show up and users can start a new game
        if(isDone()){
            char allWordsFound[BUFFER_SIZE * 2] = {0};
            strcat(allWordsFound,
                   "HTTP/1.1 200 OK\r\ncontent-type: text/html; charset=UTF-8 \r\n\r\n"
                   "<html><body style=\"text-align: center; font-size: 2em; background-color: #BDFCC9\">Congratulations! You solved it! <a href=\"words\">Another?</a></body></html>");
            send(client_fd, allWordsFound, strlen(allWordsFound), 0);
            return NULL;
        }
        // Generate and send the game state HTML
        char html_response[BUFFER_SIZE * 2] = {0};
        snprintf(html_response, sizeof(html_response),
                 "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                 "<html><body style=\"background-color: powderblue;\">"
                 "<h1 style=\"text-align: center; font-size: 2em;\">Words Without Friends: The Game - Reloaded</h1>"
                 "<p style=\"font-size: 1.1em;\">Current Master Word: <strong><u>%s</u></strong></p>"
                 "<p style=\"font-size: 1.1em;\">Current List Of Words In This Round:</p>",
                 masterWordNode->word);
        char game_state_html[BUFFER_SIZE] = {0};

        generateGameProgressHTML(game_state_html, sizeof(game_state_html));

        strcat(html_response, game_state_html);
        strcat(html_response, guess);
        strcat(html_response, "</body></html>");
        //printf("the html response is: %s", html_response);
        send(client_fd, html_response, strlen(html_response), 0);
        //If the request does not contain /words then it will try to retrieve a file with the provided file path
    } else if (stat(full_path, &file_stat) < 0 || S_ISDIR(file_stat.st_mode)) {
        char *not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
        send(client_fd, not_found_response, strlen(not_found_response), 0);
    } else {
        int file_fd = open(full_path, O_RDONLY);
        if (file_fd < 0) {
            perror("ERROR opening file");
        } else {
            char *ok_response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
            send(client_fd, ok_response, strlen(ok_response), 0);
            ssize_t bytes_read;
            while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                send(client_fd, buffer, bytes_read, 0);
            }
            close(file_fd);
        }
    }
    close(client_fd);
    return NULL;
}
//Displays either the underscores if the word hasn't been found or it displays the full word if it has
//Undo comment to see which words are left to guess
void generateGameProgressHTML(char *buffer, int buffer_size) {
    struct gameListNode *current = gameListRoot;
    char temp[50];

    while (current) {
        if (current->wordHasBeenFound) {
            snprintf(temp, sizeof(temp), "%s", current->word);
        } else {
            strcpy(temp, current->word);
            //printf("Word to be guessed is: %s\n", temp);
            wordsToUnderscores(temp);
        }
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer),
                 "<p><strong>%s</strong></p>", temp);
        current = current->next;
    }
    snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer),
             "<form action=\"/words\" method=\"GET\">"
             "<input type=\"text\" name=\"move\" autofocus>"
             "<input type=\"submit\" value=\"Guess\">"
             "</form></body></html>");
}
//Opens file, capitalizes every word in the dictionary and adds it to the dictionary linked list
int initialization(){
    srand(time(NULL));
    wordCount = 0;
    FILE *file = fopen("2of12.txt", "r");
    char string [1000];

    if(file == NULL){
        printf("Fail to open\n");
    } else {
        //printf("File is open\n");
        while(fgets(string,1000,file) != NULL){
            capitalizeString(string);
            addToList(string);
            wordCount++;
            //printf("%s\n", string);
        }
    }
    fclose(file);
    return wordCount;
}
//Capitalizes any char array given
void capitalizeString(char* string){
    for (int i = 0; i < strlen(string); i++)
    {
        char currentCharacter = string[i];
        if (currentCharacter == '\r' || currentCharacter == '\n'){
            string[i] = '\0';
        } else {
            string[i] = toupper(string[i]);
        }
    }
}
//Method check to see if every word in the game list has been found
bool isDone(){
    struct gameListNode *current = gameListRoot;
    while(current != NULL){
        if(!current->wordHasBeenFound){
            return false;
        }
        current = current->next;
    }
    return true;
}
//Frees both lists with allocated memory
void tearDown(){
    cleanupWordListNodes();
    cleanupGameListNodes();
}
//Takes in a char array and increments the location of each letter in the letter array
//For example A would increment letters[0] by one since A is the first letter in the alphabet
void getLetterDistribution(const char *word, int letters[26]){
    for (int i = 0; i < 26; i++){
        letters[i] = 0;
    }
    for (int i = 0; word[i] != '\0'; i++){
        char currentCharacter = word[i];
        if (currentCharacter >= 'A' && currentCharacter <= 'Z'){
            int index = currentCharacter - 'A';
            letters[index]++;
        }
    }
}
//Compares two arrays to check if the candidate word can be made from the main word
bool compareCounts(int letterChoices[26], int letterCandidates[26]){
    for (int i = 0; i < 26; i++){
        if (letterCandidates[i] > letterChoices[i]){
            return false;
        }
    }
    return true;
}
//Gets a random number and looks for the corresponding word in the dictionary
//If the chosen word is less than 6 characters long then it will keep looking for another word
struct wordListNode* getRandomWord(int wordCount){
    srand(time(0));
    struct wordListNode * current = dictionaryRoot;
    int randomNumber = rand () % (wordCount) + 0;

    for(int i = 0; i < randomNumber && current != NULL; i++){
        current = current->next;
        //printf("current word is %s\n", current->word);
    }

    while(current != NULL && strlen(current->word) <= 6){
        current = current->next;
        //printf("current word is %s\n", current->word);
    }

    if(current == NULL){
        printf("No word found\n");
    }

    return current;
}
//Method takes in a master word and checks it against the dictionary to see what other words can be made from that master word
//Every word that can be made from the master word is added to the game list linked list
void findWords(struct wordListNode * masterWord){
    int masterDistribution[26];
    getLetterDistribution(masterWord->word,masterDistribution);
    struct wordListNode * current = dictionaryRoot;

    while(current != NULL){
        int candidateLetterDistribution[26];
        getLetterDistribution(current->word,candidateLetterDistribution);

        if(compareCounts(masterDistribution,candidateLetterDistribution)){
            addToGameList(current->word);
            //printf("Word found %s\n", current->word);
        }
        current = current->next;
    }
}
//Checks to see if the guess from the user is in the game list and displays a message
char* checkInputOnGameList(char *string){
    struct gameListNode * current = gameListRoot;

    while(current != NULL){
        if(strcmp(current->word, string) == 0){
            //printf("Matching word found: '%s' and guess: '%s'\n", current->word, string);
            if(!current->wordHasBeenFound){
                current->wordHasBeenFound = 1;
                //printf("Word was found! %s\n", current->word);
                return "<p style=\"color:green;\">Word was found!</p>";
            } else {
                //printf("You have already found that word\n");
                return "<p style=\"color:blue;\">You have already found that word, try again!</p>";
            }
        }
        current = current->next;
    }
    if(current == NULL){
        //printf("Word not found, try again: %s\n", string);
        return "<p style=\"color:red;\">Word not was found, try again!</p>";
    }
};
//Method takes in a word and adds it to the dictionary linked list
void addToList (char *word){
    struct wordListNode *newNode = (struct wordListNode*)malloc(sizeof(struct wordListNode));
    strncpy(newNode->word,word,sizeof(newNode->word));
    newNode->next = NULL;

    if(dictionaryRoot == NULL){
        dictionaryRoot = newNode;
    } else {
        currentWord = dictionaryRoot;
        while(currentWord->next != NULL) {
            currentWord = currentWord->next;
        }
        currentWord->next = newNode;
    }
};
//Method takes in a word and adds it to the game linked list
void addToGameList(char *word){
    struct gameListNode *newNode = (struct gameListNode*)malloc(sizeof(struct gameListNode));
    strncpy(newNode->word,word,sizeof(newNode->word));
    newNode->wordHasBeenFound = 0;
    newNode->next = NULL;

    if(gameListRoot == NULL){
        gameListRoot = newNode;
    } else {
        currentWordInGameList = gameListRoot;
        while(currentWordInGameList->next != NULL) {
            currentWordInGameList = currentWordInGameList->next;
        }
        currentWordInGameList->next = newNode;
    }
};
//Free up the word linked list
void cleanupWordListNodes(){
    struct wordListNode * current = dictionaryRoot;
    struct wordListNode * temp;
    while(current != NULL){
        temp = current;
        current = current->next;
        free(temp);
    }
    dictionaryRoot = NULL;
}
//Free up the game linked list
void cleanupGameListNodes(){
    struct gameListNode * current = gameListRoot;
    struct gameListNode * temp;
    while(current != NULL){
        temp = current;
        current = current->next;
        free(temp);
    }
    gameListRoot = NULL;
}
//Changes words to underscores for every letter in a word
void wordsToUnderscores(char *word) {
    char temp[100];
    int j = 0;

    for (int i = 0; word[i] != '\0'; i++) {
        if (isalpha(word[i])) {
            temp[j++] = '_';
            temp[j++] = ' ';
        } else {
            temp[j++] = word[i];
        }
    }

    if (j > 0 && temp[j - 1] == ' ') {
        temp[j - 1] = '\0';
    } else {
        temp[j] = '\0';
    }

    strcpy(word, temp);
}