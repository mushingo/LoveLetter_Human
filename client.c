/* client.c - Michael Scotson
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include "shared.h"

// Exit Codesi
#define NO_ERROR 0
#define BAD_ARG_NUMBER 1
#define BAD_PLAYER_NAME 2
#define BAD_GAME_NAME 3
#define BAD_PORT 4
#define BAD_SERVER 5
#define BAD_GAME_INFO 6
#define BAD_MESSAGE 7
#define SERVER_LOSS 8
#define PLAYER_LOSS 9
//#define BAD_SYSTEM 20

/* Player structure used to stored information about the client
 * including information received about other players.
 */
struct Player {
    int players;
    char label;
    char *playerName;
    char *gameName;
    int port;
    char *hostname;
    int serverComs;
    FILE *toServer;
    FILE *fromServer;

    char firstCard;
    char secondCard;

    char *playerAName;
    char *playerBName;
    char *playerCName;
    char *playerDName;

    char cardsPlayedA[9];
    char cardsPlayedB[9];
    char cardsPlayedC[9];
    char cardsPlayedD[9];
    char statusA;
    char statusB;
    char statusC;
    char statusD;
    char playCard;
    char targetPlayer;
    char guessedCard;
};


/*
 * Exit player(client) with appropriate code after freeing memory
 * @param p The Player structure
 * @param status The exit status to use.
 */
void exit_player(struct Player *p, int status) {

    if (p->serverComs >= 0) { 
        close(p->serverComs);
    }
    free(p);

    switch(status) {
        case NO_ERROR:
            exit(NO_ERROR);
        case BAD_ARG_NUMBER:
            fprintf(stderr, "Usage: client name game_name port host\n");
            exit(BAD_ARG_NUMBER);
        case BAD_PLAYER_NAME:
            fprintf(stderr, "Invalid player name\n");
            exit(BAD_PLAYER_NAME);
        case BAD_GAME_NAME:
            fprintf(stderr, "Invalid game name\n");
            exit(BAD_GAME_NAME);
        case BAD_PORT:
            fprintf(stderr, "Invalid server port\n");
            exit(BAD_PORT);
        case BAD_SERVER:
            fprintf(stderr, "Server connection failed\n");
            exit(BAD_SERVER);
        case BAD_GAME_INFO:
            fprintf(stderr, "Invalid game information received from server\n");
            exit(BAD_GAME_INFO);
        case BAD_MESSAGE:
            fprintf(stderr, "Bad message from server\n");
            exit(BAD_MESSAGE);
        case SERVER_LOSS:
            fprintf(stderr, "Unexpected loss of server\n");
            exit(SERVER_LOSS);
        case PLAYER_LOSS:
            fprintf(stderr, "End of player input\n");
            exit(PLAYER_LOSS);
        default:
            exit(NO_ERROR);
    }
}

/*
 * Flush streams to server and to stdin and out.
 */
void flush_streams(struct Player *p) {
    fflush(stdout);
    fflush(stderr);
    fflush(p->toServer);
}

/* Check that the length of the parameter is correct
 * @return 0 if good, 1 if bad
 */
int check_param(char *param, int length) {

    if (length == 0) {
        return 1;
    }

    for (int i = 0; i < length; i++) {
        if (param[i] == '\n') {
            return 1;
        }
    }

    return 0;
}

/*
 * Take arguments supplied and create player and Game strucutures
 */
void parse_args(struct Player *p, int argc, char *argv[]) {

    int portLength = strlen(argv[3]), gameLength = strlen(argv[2]), hostLength;
    int playerLength = strlen(argv[1]), port;
    char localHost[] = "localhost";
    char *portNext, *player, *game, *host;

    if (check_param(argv[1], playerLength)) {
        exit_player(p, 2);
    }
    if (check_param(argv[2], gameLength)) {
        exit_player(p, 3);
    }

    port = strtol(argv[3], &portNext, 10);

    if (*portNext || port < 1 || port > 65535 || portLength == 0 || 
            portLength > 6) {
        exit_player(p, 4);
    }
    
    player = malloc(sizeof(char) * (playerLength + 1));
    game = malloc(sizeof(char) * (playerLength + 1));

    strncpy(game, argv[2], gameLength);
    strncpy(player, argv[1], playerLength);
    game[gameLength] = 0;
    player[playerLength] = 0;
    
    if (argc == 4) {
        hostLength = strlen(localHost);
    } else {
        hostLength = strlen(argv[4]);
    }

    host = malloc(sizeof(char) * (hostLength + 1));

    if (argc == 4) {
        strncpy(host, localHost, hostLength);
    } else {
        strncpy(host, argv[4], hostLength);
    }
    host[hostLength] = 0;

    p->playerName = player;
    p->gameName = game;
    p->port = port;
    p->hostname = host;
}

/*
 * return the host name supplied as an IP address within an in_addr struct
 * @return an in_addr struct with the IP address for hostname
 */
struct in_addr* name_to_ip_address(struct Player *p, char* hostname)
{
    int addressError;
    struct addrinfo* addressInfo, hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    addressError = getaddrinfo(hostname, NULL, &hints, &addressInfo);

    if(addressError) {
        return NULL;
    }
    
    return &(((struct sockaddr_in*)(addressInfo->ai_addr))->sin_addr);
}

/*
 * Connect to the IP address supplied
 * @return return the server comunication file descripter
 */
int connect_to(struct Player *p, struct in_addr* ipAddress, int port)
{
    struct sockaddr_in socketAddr;
    int serverComs;
    
    serverComs = socket(AF_INET, SOCK_STREAM, 0);
    
    if(serverComs < 0) {
        exit_player(p, BAD_SERVER);
    }

    socketAddr.sin_family = AF_INET;
    socketAddr.sin_port = htons(port);
    socketAddr.sin_addr.s_addr = ipAddress->s_addr;    

    if(connect(serverComs, (struct sockaddr*)&socketAddr, 
            sizeof(socketAddr)) < 0) {
        exit_player(p, BAD_SERVER);
    }

    return serverComs;
}

/*
 * Connect to a server, and accept a connection and open FILE streams to talk
 * to the server
 */
void connect_to_server(struct Player *p) {
    int serverComs, port = p->port;
    struct in_addr* ipAddress;
    char *hostname = p->hostname;

    p->serverComs = -1; 

    ipAddress = name_to_ip_address(p, hostname);
    
    if(!ipAddress) {
        exit_player(p, BAD_SERVER);
    }

    serverComs = connect_to(p, ipAddress, port);
    
    p->serverComs = serverComs;

    p->fromServer = fdopen(serverComs, "r");
    p->toServer = fdopen(serverComs, "w");

    fprintf(p->toServer, "%s\n", p->playerName);
    fprintf(p->toServer, "%s\n", p->gameName);
    flush_streams(p);
}

/*
 * Get a message of any length from the player 
 * @return returns a char pointer to a heap allocated message of any length
 */
char* get_message(struct Player *p) {
    int i = 0;
    char *buffer = calloc(2, sizeof(char));
    char messagePart;

    while(1) {
        messagePart = fgetc(p->fromServer);

        if (messagePart == EOF || messagePart == '\n') {
            i++;
            break;
        }

        buffer[i] = messagePart;
        i++;

        buffer = realloc(buffer, ((sizeof(char) * 2) + i));
    }

    buffer[i] = 0;

    if (strlen(buffer) == 0) {
        exit_player(p, BAD_GAME_INFO);
    }

    return buffer;
}

/*
 * Get information about the game (name, player names) from serve
 */
void get_game_information(struct Player *p) {
    char *gameDetails;

    gameDetails = get_message(p);

    if (strlen(gameDetails) != 3) {
        exit_player(p, BAD_GAME_INFO);
    }

    sscanf(gameDetails, "%d %c", &(p->players), &(p->label));

    if (check_player(p->label, p->players)) {
        exit_player(p, BAD_GAME_INFO);
    }

    p->playerAName = get_message(p);
    p->playerBName = get_message(p);
    p->playerCName = NULL;
    p->playerDName = NULL;

    switch(p->players) {
        case 4:
            p->playerCName = get_message(p);
            p->playerDName = get_message(p);
            break;
        case 3:
            p->playerCName = get_message(p);
    }
}

/* Setup a new round by giving players blank cards if playing and setting 
 * their cards to zero otherwise. Statuses are also set to blank if playing
 * and zero if not. 
 * @params p The player structure
 */
void init_round(struct Player *p) { 
    char blankCards[] = {'-', '-', '-', '-', '-', '-', '-', '-', '-'};
    char notPlayingCards[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

    strncpy(p->cardsPlayedA, blankCards, 9);
    strncpy(p->cardsPlayedB, blankCards, 9);
    strncpy(p->cardsPlayedC, blankCards, 9);
    strncpy(p->cardsPlayedD, blankCards, 9);
    p->statusA = ' ';
    p->statusB = ' ';
    p->statusC = ' ';
    p->statusD = ' ';
    p->firstCard = '-';
    p->secondCard = '-';

    switch (p->players) {
        case 2:
            strncpy(p->cardsPlayedC, notPlayingCards, 9);
            p->statusC = 0;
        case 3:
            strncpy(p->cardsPlayedD, notPlayingCards, 9);
            p->statusD = 0;
    }
}

/* Sets the supplied player status to indicate that the player is protected
 * @params p The player structure
 * @params label The player to remove protection from
 */
void protect_player(struct Player *p, char label) {

    switch (label) {
        case 'A':
            p->statusA = '*';
            break;
        case 'B':
            p->statusB = '*';
            break;
        case 'C':
            p->statusC = '*';
            break;
        case 'D':
            p->statusD = '*';
            break;
    }
    return;
}

/* Prints the status of each player playing (out or not) as well as the cards
 * they have discarded thus far.
 * @params p The player structure
 * @params The player to print the status and discarded cards for
 */
void print_player_status(struct Player *p, int player) {
    char card; 
    int j = 0;
    
    switch (player) {
        case 0:
            fprintf(stdout, "A(%s)%c:", p->playerAName, p->statusA);
            card = p->cardsPlayedA[j];
            while (card != '-') {
                fprintf(stdout, "%c", card);
                card = p->cardsPlayedA[++j];
            }
            break;
        case 1:
            fprintf(stdout, "B(%s)%c:", p->playerBName, p->statusB);
            card = p->cardsPlayedB[j];
            while (card != '-') {
                fprintf(stdout, "%c", card);
                card = p->cardsPlayedB[++j];
            }          
            break;
        case 2:
            fprintf(stdout, "C(%s)%c:", p->playerCName, p->statusC);
            card = p->cardsPlayedC[j];
            while (card != '-') {
                fprintf(stdout, "%c", card);
                card = p->cardsPlayedC[++j];
            }
            break;
        case 3:
            fprintf(stdout, "D(%s)%c:", p->playerDName, p->statusD);
            card = p->cardsPlayedD[j];
            while (card != '-') {
                fprintf(stdout, "%c", card);
                card = p->cardsPlayedD[++j];
            }            
            break;
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}


/* Prints the status message. First prints the status and discarded cards of 
 * each player then it prints what cards this program is holding
 * @params p The player structure
 * */ 
void print_status(struct Player *p) {

    for (int player = 0; player < p->players; ++player) {
        print_player_status(p, player);
    }

    fprintf(stdout, "You are holding:%c%c\n", p->firstCard, p->secondCard);
}

/* Reads params given in commands expecting a single card (newround, yourturn,
 * replace) and checks to see if it's a single valid card, then returns this 
 * card. Exits if invalid.
 * @params p The player structure
 * @params param The parameter given to check if a single card
 * @return A single valid card
 */
char read_single_card(struct Player *p, char *param) {
    int paramLength;

    paramLength = strlen(param);

    if (paramLength != 1 || check_card(param[0]) || param[0] == '-') {
        exit_player(p, BAD_MESSAGE);
    }

    return param[0];
}

/* Replaces card held with card specified by replace command
 * @params p The player structure
 * @params param the param passed in- a card.
 */
void replace(struct Player *p, char *param) {
    char card;
    
    card = read_single_card(p, param);
    p->firstCard = card;
    return;
}

/* If the supplied player is protected, the protection is removed from the 
 * the player status.
 * @params p The player structure
 * @params label The player to remove protection from
 */
void remove_protection(struct Player *p, char label) {
  
    switch (label) {
        case 'A':
            if (p->statusA == '*') {
                p->statusA = ' ';
            }
            break;
        case 'B':
            if (p->statusB == '*') {
                p->statusB = ' ';
            }
            break;
        case 'C':
            if (p->statusC == '*') {
                p->statusC = ' ';
            }
            break;
        case 'D':
            if (p->statusD == '*') {
                p->statusD = ' ';
            }
            break;
    }
    return;
}

/* Add card played (as reported by thishappend or discarded during turn) to
 * a list of cards played by all players.
 * @params p The player structure
 * @params label the player discarded cards list to add the discarded card to
 * @params card the card to add to the players discarded card list
 */
void add_played_card(struct Player *p, char label, char card) {
    int i;

    if (label == '-' || card == '-') {
        return;
    }

    switch (label) {
        case 'A':
            for (i = 0; i < 9; ++i) {
                if (p->cardsPlayedA[i] == '-') {
                    p->cardsPlayedA[i] = card;
                    return;
                }
            }
            break;
        case 'B':
            for (i = 0; i < 9; ++i) {
                if (p->cardsPlayedB[i] == '-') {
                    p->cardsPlayedB[i] = card;
                    return;
                }
            }
            break;
        case 'C':
            for (i = 0; i < 9; ++i) {
                if (p->cardsPlayedC[i] == '-') {
                    p->cardsPlayedC[i] = card;
                    return;
                }
            }
            break;
        case 'D':
            for (i = 0; i < 9; ++i) {
                if (p->cardsPlayedD[i] == '-') {
                    p->cardsPlayedD[i] = card;
                    return;
                }
            }
    }
}

/*
 * Check to see if the target supplied by the player is possible
 * @return 0 if possible, 1 if not
 */
int target_possible(struct Player *p) {
    char label = p->label;
    
    switch (label) {
        case 'A':
            if (p->statusB == ' ' || p->statusC == ' ' || p->statusD == ' ') {
                return 1;
            } 
            break;
        case 'B':
            if (p->statusA == ' ' || p->statusC == ' ' || p->statusD == ' ') {
                return 1;
            }
            break;
        case 'C':
            if (p->statusB == ' ' || p->statusA == ' ' || p->statusD == ' ') {
                return 1;
            }
            break;
        case 'D':
            if (p->statusB == ' ' || p->statusC == ' ' || p->statusA == ' ') {
                return 1;
            }
            break;
    }
    return 0;
}

/*
 * Prompts a player for a guess and if valid put it in the player strucutre
 */
void get_guess(struct Player *p, char discard, char target) {
    char input[3], guess = '-', extra;
    int validGuess = 0;
	
    while (!validGuess && discard == '1' && target != '-') {
        fprintf(stdout, "guess>");
        fflush(stdout);
        if (fgets(input, 3, stdin) == NULL) {
            exit_player(p, PLAYER_LOSS);
        }
		
        guess = input[0];
        if (input[1] == '\n' && input[2] == 0) {
            if (guess > '1' && guess < '9') {
                validGuess = 1;
            }
        } else {
            while ((extra = fgetc(stdin)) != '\n' && extra != EOF);
        }
    }
	
    p->guessedCard = guess; 
}

/*
 * Prompts a player for a move and if valid put it in the player strucutre
 */
void get_move(struct Player *p) {
    char input[3], discard, extra, target = '-';
    int validCard = 0, validTarget = 0; 

    while (!validCard) {   
        fprintf(stdout, "card>");
        fflush(stdout);
        if (fgets(input, 3, stdin) == NULL) {
            exit_player(p, PLAYER_LOSS);
        }
        discard = input[0];
		
        if (input[1] == '\n' && input[2] == 0) {
            if (discard > '0' && discard < '9' && (discard == p->firstCard ||
                    discard == p->secondCard)) {
                validCard = 1;
            }
        } else {
            while ((extra = fgetc(stdin)) != '\n' && extra != EOF);
        }
    }
    p->playCard = discard;

    while (!validTarget && (discard == '6' || discard == '5' || 
	    discard == '3' || discard == '1')) {
        if (discard != '5' && !target_possible(p)) {
            target = '-';
            validTarget = 1;
        } else {
            fprintf(stdout, "target>");
            fflush(stdout);
            if (fgets(input, 3, stdin) == NULL) {
                exit_player(p, PLAYER_LOSS);
            }
            target = input[0];
			
            if (input[1] == '\n' && input[2] == 0) {
                if (target > '@' && target < 'E' && (discard == '5' || 
                        target != p->label)) {
                    validTarget = 1;
                }
            } else {
                while ((extra = fgetc(stdin)) != '\n' && extra != EOF);
            }
        }
    }
    p->targetPlayer = target;
	
    get_guess(p, discard, target);
}


/* Compares command given with valid commands available. If command is valid
 * returns a different code for each command. If command is not valid, exits
 * due to invalid message.
 * @params p The player structure
 * @params command the command read from stdin
 * @return A command code. Each valid command has a unique code
 */
int process_command(struct Player *p, char *command) {
    char gameover[] = "gameover", newround[] = "newround", no[] = "NO",
            yourturn[] = "yourturn", thishappened[] = "thishappened",
            replace[] = "replace", scores[] = "scores", yes[] = "YES";

    if (!(strcmp(command, gameover))) {
        return 1;
    }
    if (!(strcmp(command, newround))) {
        return 2;
    }
    if (!(strcmp(command, yourturn))) {
        return 3;
    }
    if (!(strcmp(command, thishappened))) {
        return 4;
    }
    if (!(strcmp(command, replace))) {
        return 5;
    }
    if (!(strcmp(command, scores))) {
        return 6;
    }
    if (!(strcmp(command, yes))) {
        return 7;
    }
    if (!(strcmp(command, no))) {
        return 8;
    }

    exit_player(p, BAD_MESSAGE);

    return -1;
}

/*
 * Gets a message from the server.
 * @return a poitner to the message
 */
char* get_server_message(struct Player *p) {
    char *messageIn, *input, c;
    int length;

    messageIn = calloc(23, sizeof(char));

    input = fgets(messageIn, 23, p->fromServer);
    //fprintf(stderr, "From Server: %s", messageIn);

    if (input == NULL) {
        exit_player(p, SERVER_LOSS);
    }

    length = strlen(input);
    if (input[length - 1] != '\n') {
        while ((c = fgetc(p->fromServer)) != '\n' && c != EOF);
    }

    //fprintf(stderr, "From Server: %s\n", messageIn);

    return messageIn;
}

/* Process yourturn message by performing turn start actions (remove
 * protection) and then getting the appropiate move command (played card,
 * target player, guessed card) which is sent to stdout.
 * @params p The player structure
 * @params param The param passed in from command- a card
 */
void your_turn(struct Player *p, char *param) {
    char card, label, *messageIn, command[23];

    card = read_single_card(p, param);
    p->secondCard = card;

    label = p->label;
    remove_protection(p, label);

    p->playCard = '-';
    p->targetPlayer = '-';
    p->guessedCard = '-';

    print_status(p);

    get_move(p);

    fprintf(p->toServer, "%c%c%c\n", p->playCard, p->targetPlayer,
            p->guessedCard);
    fflush(p->toServer);
    
    messageIn = get_server_message(p);
    sscanf(messageIn, "%s", command);

    while(process_command(p, command) != 7) {
        get_move(p);
        fprintf(p->toServer, "%c%c%c\n", p->playCard, p->targetPlayer,
                p->guessedCard);
        fflush(p->toServer);
        messageIn = get_server_message(p);
        sscanf(messageIn, "%s", command);
    }

    if (p->firstCard == p->playCard) {
        p->firstCard = p->secondCard;
    }
    p->secondCard = '-';

    card = p->playCard;   
    add_played_card(p, label, card);
}

/* Process new round message. This takes the param given (a card),
 * checks it, reads it as a card, initalises round, then gives the 
 * card to player as their first card.
 * @params p The player structure
 * @params param The parameter given in the newround command
 * should be a single card
 */
void new_round(struct Player *p, char *param) {
    char card;
    
    card = read_single_card(p, param);
    init_round(p);
    p->firstCard = card;
}


/* Check to make sure that a thishappened message is valid
 * @params p The player structure
 * @params param the details of the actions reported on by (contains cards
 * and players, with the order indicitive of what action happened.
 */
void check_this_happened_params(struct Player *p, char *param) {
    int paramLength;
    char source = param[0], discard = param[1], target = param[2], 
            guess = param[3], dropper = param[5], dropped = param[6], 
            out = param[7];

    paramLength = strlen(param);

    if (paramLength != 8 || param[4] != '/') {
        exit_player(p, BAD_MESSAGE);    
    }

    if (check_valid_move(source, discard, target, guess, p->players)) {
        exit_player(p, BAD_MESSAGE);
    }

    if (check_card(dropped) || (check_player(out, p->players) &&
            (out != '-'))) {
        exit_player(p, BAD_MESSAGE);
    }

    if ((dropped == '-' && dropper != '-') || (dropped != '-' && 
            dropper == '-')) {
        exit_player(p, BAD_MESSAGE);
    }

    if (out != '-' && out != dropper) {
        exit_player(p, BAD_MESSAGE);
    }

    switch (discard) {
        case '8':
            if (out != source) {
                exit_player(p, BAD_MESSAGE);
            }
            break;
        case '7':
        case '6':
        case '4':
        case '2':
            if (dropped != '-' || dropper != '-' || out != '-') {
                exit_player(p, BAD_MESSAGE);
            }
    }
}


/* Set a Player as eliminated by marking it with a '-' symbol.
 * @params p The player structure
 * @params eliminatedPlayer The player to be eliminated
 */
void eliminate_player(struct Player *p, char eliminatedPlayer) {

    switch (eliminatedPlayer) {
        case 'A':
            p->statusA = '-';
            break;
        case 'B':
            p->statusB = '-';
            break;
        case 'C':
            p->statusC = '-';
            break;
        case 'D':
            p->statusD = '-';
    }

    if (eliminatedPlayer == p->label) {
        p->firstCard = '-';
        p->secondCard = '-';
    }
}

/* Prints information about what happened to stdout
 */
void print_this_happened(char source, char discard, 
        char target, char guess, char dropper, char dropped, char out) {

    fprintf(stdout, "Player %c discarded %c", source, discard);

    if (target != '-') {
        fprintf(stdout, " aimed at %c", target);
    }

    if (guess != '-') {
        fprintf(stdout, " guessing %c", guess);
    } 
    fprintf(stdout, ".");

    if (dropped != '-') {
        fprintf(stdout, " This forced %c to discard %c.", dropper, dropped);
    }

    if (out != '-') {
        fprintf(stdout, " %c was out.", out);
    }

    fprintf(stdout, "\n");
    fflush(stdout);
}

/* Process a thishappened message from the hub and take the appropriate actions
 * @params p The player structure
 * @params param the details of the actions reported on by (contains cards 
 * and players, with the order indicitive of what action happened.
 */
void this_happened(struct Player *p, char *param) {
    char sourcePlayer, playedCard, cardDropper, droppedCard, eliminatedPlayer;
    check_this_happened_params(p, param);

    sourcePlayer = param[0];
    playedCard = param[1];
    cardDropper = param[5];
    droppedCard = param[6];
    eliminatedPlayer = param[7];
    
    remove_protection(p, sourcePlayer);

    print_this_happened(sourcePlayer, playedCard, param[2], param[3], 
            cardDropper, droppedCard, eliminatedPlayer);  

    if (sourcePlayer != p->label) {
        add_played_card(p, sourcePlayer, playedCard);
    }
    
    add_played_card(p, cardDropper, droppedCard);
    
    if (eliminatedPlayer != '-') {
        eliminate_player(p, eliminatedPlayer);
    }

    if (playedCard == '4') {
        protect_player(p, sourcePlayer);
    }
    
    if (droppedCard == '4' && eliminatedPlayer == '-' && playedCard == '5') {
        protect_player(p, cardDropper);
    }
}


/* Reads, validates and prints the scores messaged recieved from stdin
 * Exits if invalid message
 * @param p The player structure
 * @param messageIn A scores message
 */
void scores(struct Player *p, char *messageIn) {
    int scoreA = 0, scoreB = 0, scoreC = 0, scoreD = 0, scoresCount = 0,
            messageLength; 

    messageLength = strlen(messageIn);
    if (messageLength != (11 + 2 * ((p->players) - 2))) {
        exit_player(p, BAD_MESSAGE);
    }

    scoresCount = sscanf(messageIn, "%*s %d %d %d %d", &scoreA, &scoreB, 
            &scoreC, &scoreD);    
    if (scoresCount != p->players) {
        exit_player(p, BAD_MESSAGE);
    }

    if (scoreA < 0 || scoreB < 0 || scoreC < 0 || scoreD < 0 ||
            scoreA > 4 || scoreB > 4 || scoreC > 4 || scoreD > 4) {
        exit_player(p, BAD_MESSAGE);
    }

    fprintf(stdout, "Scores: %s=%d %s=%d", p->playerAName, scoreA, 
            p->playerBName, scoreB);
    switch (p->players) {
        case 4:
            fprintf(stdout, " %s=%d %s=%d", p->playerCName, scoreC,
                    p->playerDName, scoreD);
            break;
        case 3:
            fprintf(stdout, " %s=%d", p->playerCName, scoreC);
    }
    fprintf(stdout, "\n");
    fflush(stdout);

    return;
}


/* Play a game of Love Letter.
 * Gets a message from the server and performs the appropriate actions.
 * Exits if instructed via gameover or upon hubloss or invalid input.
 * @param p The player structure
 */
void play_game(struct Player *p) {
    char *messageIn, command[23], param[23];
    int argCount, commandCode;

    messageIn = get_server_message(p);

    argCount = sscanf(messageIn, "%s %[^\n]", command, param);
    commandCode = process_command(p, command);

    if (commandCode == 1) { 
        if (argCount == 1) {
            fprintf(stdout, "Game over\n");
            fflush(stdout);
            exit_player(p, NO_ERROR);
        }
        exit_player(p, BAD_MESSAGE);
    }

    if (argCount != 2) {
        exit_player(p, BAD_MESSAGE);
    }

    switch (commandCode) {
        case 2:
            new_round(p, param);
            break;
        case 3:
            your_turn(p, param);
            break;
        case 4:
            this_happened(p, param);
            break;
        case 5:
            replace(p, param);
            break;
        case 6:
            scores(p, messageIn);
            break;
    }
}


int main(int argc, char *argv[]) {
    struct Player *p = NULL;

    p = malloc(sizeof(*p));

    if (argc != 5 && argc != 4) {
        exit_player(p, BAD_ARG_NUMBER);
    }

    if (argc == 4) {
        // Set something about local host
    }

    parse_args(p, argc, argv);

    connect_to_server(p);

    get_game_information(p);

    init_round(p);

    while (1) {
        play_game(p);
    }
}

