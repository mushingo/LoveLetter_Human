/* server.c - Michael Scotson 
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "shared.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>

#define MAXHOSTNAMELEN 128
#define NO_ERROR 0 
#define BAD_ARGS 1
#define DECKFILE_FAIL 2
#define DECK_FAIL 3
#define BAD_PORT 4
#define LISTEN_FAIL 5
#define BAD_SYSTEM 9


struct Decks {
    char card[17];
    struct Decks *next;
};


struct Players {
    struct Players *nextPlayer;
    char *name;
    int roundsWon;
    int gamesWon;
    int gamesPlayed;
};

struct Port {
    struct Port *nextPort;
    int port;
    int fd;
    char *deckfile;
    struct Decks *firstDeck;
    struct Game *headGame;
};

struct Game {
    int gameReady;        
    struct Game *nextGame;
    char* gameName;
    int emptyDeck;
    int nextCard;
    int players;
    int alivePlayers;
    char burntCard;
    char move[5];
    struct Decks *currentDeck;   

    char *playerAName;
    FILE *fromA;
    FILE *toA;
    int fdA;
    int pointsA;
    char firstCardA;
    char secondCardA;
    int winnerA;

    char *playerBName;
    FILE *fromB;
    FILE *toB;
    int fdB;
    int pointsB;
    char firstCardB;
    char secondCardB;
    int winnerB;

    char *playerCName;
    FILE *fromC;
    FILE *toC;
    int fdC;
    int pointsC;
    char firstCardC;
    char secondCardC;
    int winnerC;

    char *playerDName;
    FILE *fromD;
    FILE *toD;
    int fdD;
    int pointsD;
    char firstCardD;
    char secondCardD;
    int winnerD;
};

struct Server {
    int adminPort;
    int fdAdminPort;
    struct Port *headPort;
    struct Players *headPlayer;
};

struct Port* create_port(void) {
    struct Port *p;
    p = malloc(sizeof(*p));
    p->nextPort = NULL;
    return p;
}

/* Exits server with the appropriate message upon error
 *
 */
void exit_server(struct Server *s, int status) {

    switch(status) {
        case NO_ERROR:
            exit(NO_ERROR);
        case BAD_ARGS:
            fprintf(stderr, "Usage: 2310serv adminport [[port deck]...]\n");
            exit(BAD_ARGS);
        case DECKFILE_FAIL:
            fprintf(stderr, "Unable to access deckfile\n");
            exit(DECKFILE_FAIL);
        case DECK_FAIL:
            fprintf(stderr, "Error reading deck\n");
            exit(DECK_FAIL);
        case LISTEN_FAIL:
            fprintf(stderr, "Unable to listen on port\n");
            exit(LISTEN_FAIL);
        case BAD_PORT:
            fprintf(stderr, "Invalid port number\n");
            exit(BAD_PORT);
    }
}

/* Flush the file pointers that go to each player
 * @params g The game structure 
 */
void flush_streams(struct Game *g) {
    fflush(g->toA);
    fflush(g->toB);
    fflush(g->toC);
    fflush(g->toD);
}

/*Open a socket, bind to it and listen for connections
 * @return returns a file descriptor for the new connection
 */
int open_listen(struct Server *s, int port) {
    int fd;  
    struct sockaddr_in serverAddr;
    int optVal;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        exit_server(s, LISTEN_FAIL);
    }

    optVal = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int)) < 0) {
        exit_server(s, LISTEN_FAIL);
    }

    serverAddr.sin_family = AF_INET;    
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(fd, (struct sockaddr*)&serverAddr, 
            sizeof(struct sockaddr_in)) < 0) {
        exit_server(s, LISTEN_FAIL);
    }

    if(listen(fd, SOMAXCONN) < 0) {
        exit_server(s, LISTEN_FAIL);
    }

    return fd;
}

/* Send a game over message to all of the players who are
 * playing in the game
 */
void game_over(struct Game *g) {
    fprintf(g->toA, "gameover\n");
    fprintf(g->toB, "gameover\n");

    if (g->players > 2) {
        fprintf(g->toC, "gameover\n");
    }

    if (g->players > 3) {
        fprintf(g->toD, "gameover\n");
    }
    flush_streams(g);    
}

/* Check to see if a port is valid. If it is not
 * the program will exit.
 */
void check_valid_port(struct Server *s, char *portRaw) {
    int port;
    char *next;

    port = strtol(portRaw, &next, 10);

    if (*next != '\0' || port < 1 || port > 65535) {
        exit_server(s, BAD_PORT);
    }

}

/* Check that a deck (supplied as a string) is valid by checking the length
 * then counting to make sure it contains exactly the right number and type
 * of each card.
 * @params g The server structure
 * @params deck A string containing the cards of a deck 
 * @return returns a 1 if invalid, 0 if valid
 */
int check_deck (struct Server *s, char *deck) { 
    int i, ones = 0, twos = 0, threes = 0, fours = 0, fives = 0, sixes = 0,
            sevens = 0, eights = 0;

    if (deck[16] != '\n' || deck[17] != 0) {
        return DECK_FAIL;
    }

    for (i = 0; i < 16; ++i) {
        if (deck[i] < '1' || deck[i] > '8') {
            return 1;
        }
        switch (deck[i]) {
            case '1':
                ones++;
                break;
            case '2':
                twos++;
                break;
            case '3':
                threes++;
                break;
            case '4':
                fours++;
                break;
            case '5':
                fives++;
                break;
            case '6':
                sixes++;
                break;
            case '7':
                sevens++;
                break;
            case '8':
                eights++;
        }
    }
    if (ones != 5 || twos != 2 || threes != 2 || fours != 2 || fives != 2 || 
            sixes != 1 || sevens != 1 || eights != 1) {
        return DECK_FAIL;
    }
    return 0;
}

/* Load the supplied string containing a deck into supplied linked list entry
 * @params deckCards a string with each number referring to a card in the deck
 * @params deck The listed link entry (Decks struct) to load the deck to
 */
void load_deck(char *deckCards, struct Decks *deck) {
    int i;

    for (i = 0; i < 16; ++i) {
        deck->card[i] = deckCards[i];
    }
    deck->card[16] = 'E';
    //deck->nextCard = 0;
}

/* Create a new deck on the tail end of the decks linked list. The next deck
 * is the head, which creates a looped linked list.
 * @params head a pointer to the head of the decks linked list
 * @return Returns a pointer to the newly created Decks Structure
 */
struct Decks* create_deck(struct Decks *head) {
    struct Decks *d;
    d = malloc(sizeof(*d));
    d->next = head;
    return d;
}

/* Create a Decks structure, load cards to it and put structures
 * into the appropriate Port structure.
 * @return returns 0 if successful or the appropriate error code if not
 */
int load_deckfile(struct Server *s, struct Port *currentPort) {
    char deckCards[18];
    struct Decks *head, *newDeck, *lastDeck;
    int i = 0;

    FILE *deckfile = fopen(currentPort->deckfile, "r");
    if (deckfile == NULL) {
        return DECKFILE_FAIL;
    }

    head = malloc(sizeof(*head));
    head->next = head;
    currentPort->firstDeck = head;
    lastDeck = head;

    while (fgets(deckCards, 18, deckfile)) { 
        if (check_deck(s, deckCards)) {
            return DECK_FAIL;
        }
        if (i == 0) {
            load_deck(deckCards, head);
            i++;
            continue;
        }
        newDeck = create_deck(head);
        lastDeck->next = newDeck;
        load_deck(deckCards, newDeck);
        lastDeck = newDeck;
        i++;
    }
    return 0;
}

/* Gets a new cards from the deck. If the card is 'E', which is a dummy
 * card singling the end of the deck, the deck is set as empty.
 * @params g The game structure 
 * @return the new card selected from the deck
 */
char new_card(struct Game *g) {
    struct Decks *deck;
    char card;
    int newCard;

    deck = g->currentDeck;
    newCard = g->nextCard++;

    card = deck->card[newCard];

    if (card == 'E') {
        g->emptyDeck = 1;
    }
    return card;
}

/* Checks if the player supplied is dead. If the player is dead a 1 is 
 * returned, otherwise a 0 is returned.
 * @params g The game structure 
 * @return A 1 if the player is dead, a 0 if alive.
 */
int player_dead(struct Game *g, int player) {
    switch (player) {
        case 0:
            if (g->firstCardA == '!' || g->secondCardA == '!') {
                return 1;
            }
            return 0;
        case 1:
            if (g->firstCardB == '!' || g->secondCardB == '!') {
                return 1;
            }
            return 0;
        case 2:
            if (g->firstCardC == '!' || g->secondCardC == '!') {
                return 1;
            }
            return 0;
        case 3:
            if (g->firstCardD == '!' || g->secondCardD == '!') {
                return 1;
            }
            return 0;
    }
    return 0;
}

/* Finds the highest value from a supplied list of four characters and 
 * returns this value.
 * @params statusA Card player A was holding, or '!'/'-' if dead/not playing
 * @params statusB Card player B was holding, or '!'/'-' if dead/not playing
 * @params statusC Card player C was holding, or '!'/'-' if dead/not playing
 * @params statusD Card player D was holding, or '!'/'-' if dead/not playing
 * @return returns the highest supplied character
 */
char find_highest(char statusA, char statusB, char statusC, char statusD) {
    char i = '9';

    while (i > 0) {
        if (i == statusA || i == statusB || i == statusC || i == statusD) {
            return i;
        }
        i--;
    }
    return 0;
}

/* Finds the player with the highest card - eliminated players are marked 
 * using '!' which is lower than the numbered cards alive players have.
 * Non playing players are initialised with a '-', which is also lower than
 * any numbered cards.
 * If a player has this card their points are increased and they are 
 * announced as one of the winners.
 * @params g The game structure 
 */
void print_winner(struct Game *g, char statusA, char statusB, char statusC, 
        char statusD) {
    char highest = find_highest(statusA, statusB, statusC, statusD);

    fprintf(stdout, "Round winner(s) holding %c:", highest);
    
    if (highest == statusA) {
        fprintf(stdout, " A");
        g->pointsA++;
    }
    if (highest == statusB) {
        fprintf(stdout, " B");
        g->pointsB++;
    }
    if (highest == statusC) {
        fprintf(stdout, " C");
        g->pointsC++;
    }
    if (highest == statusD) {
        fprintf(stdout, " D");
        g->pointsD++;
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}

/* Sends the scores of each player to all of the players.
 * @params g The game structure 
 */
void send_scores(struct Game *g) {
    char scores[16];
    int players = g->players;
 
    if (players == 2) {
        sprintf(scores, "scores %d %d", g->pointsA, g->pointsB);
    }
    if (g->players == 3) {
        sprintf(scores, "scores %d %d %d", g->pointsA, g->pointsB, g->pointsC);
    }
    if (g->players == 4) {
        sprintf(scores, "scores %d %d %d %d", g->pointsA, g->pointsB,
                g->pointsC, g->pointsD);
    } 

    fprintf(g->toA, "%s\n", scores);
    fprintf(g->toB, "%s\n", scores);
    if (g->players > 2) {
        fprintf(g->toC, "%s\n", scores);
    }
    if (g->players > 3) {
        fprintf(g->toD, "%s\n", scores);
    }

    flush_streams(g);
}

/* Gets the next deck ready at the end of a round, prints the winner
 * of the last round and sends the scores to each player. 
 * @params g The game structure 
  */
void end_of_round(struct Game *g) {
    struct Decks *deck;
    char statusA, statusB, statusC, statusD;

    deck = g->currentDeck;
    g->nextCard = 0;

    deck = (deck->next);

    g->currentDeck = deck;
    g->emptyDeck = 0;

    statusA = g->firstCardA;
    statusB = g->firstCardB;
    statusC = g->firstCardC;
    statusD = g->firstCardD;

    print_winner(g, statusA, statusB, statusC, statusD);

    send_scores(g);
}

/* Gets a move from the specified player and puts it in game struct
 * @params g The game structure 
 * @params player The player to get the move from
 * @return returns a 1 if EOF is reached (player exited) otherwise 0
 */
int get_move(struct Game *g, int player) {
    char move[5];

    switch (player) {
        case 0:
            if (fgets(move, 5, g->fromA) == NULL) {
                return 1;
            }
            move[3] = 'A';
            break;
        case 1:
            if (fgets(move, 5, g->fromB) == NULL) {
                return 1;
            }
            move[3] = 'B';
            break;
        case 2:
            if (fgets(move, 5, g->fromC) == NULL) {
                return 1;
            }
            move[3] = 'C';
            break;
        case 3:
            if (fgets(move, 5, g->fromD) == NULL) {
                return 1;
            }
            move[3] = 'D';
            break;
    }
    strncpy(g->move, move, 5);
    return 0;

}

/* Moves the second card to the first card position and clears the second card
 * This is only done if the first card was discarded. otherwise second card 
 * only is cleared.
 */
void clear_second_card(struct Game *g, char player, char discard) {
    char firstCard, secondCard;
    switch (player) {
        case 'A':
            firstCard = g->firstCardA;
            secondCard = g->secondCardA;
            if (discard == firstCard) {
                g->firstCardA = secondCard;
            }
            g->secondCardA = '-';
            break;
        case 'B':
            firstCard = g->firstCardB;
            secondCard = g->secondCardB;
            if (discard == firstCard) {
                g->firstCardB = secondCard;
            }
            g->secondCardB = '-';
            break;
        case 'C':
            firstCard = g->firstCardC;
            secondCard = g->secondCardC;
            if (discard == firstCard) {
                g->firstCardC = secondCard;
            }
            g->secondCardC = '-';
            break;
        case 'D':
            firstCard = g->firstCardD;
            secondCard = g->secondCardD;
            if (discard == firstCard) {
                g->firstCardD = secondCard;
            }
            g->secondCardD = '-';
            break;
    }
}

/* Checks to make sure the card discarded supplied by the player whose turn it
 * is is actually held by the player.
 * @params g The game structure 
 * @params player The player whose hand to check
 * @params discard The card the player discarded to compare against hand
 */
int check_card_held(struct Game *g, int player, char discard) {
    char firstCard, secondCard;

    switch (player) {
        case 0:
            firstCard = g->firstCardA;
            secondCard = g->secondCardA;
            break;
        case 1:         
            firstCard = g->firstCardB;
            secondCard = g->secondCardB;
            break;
        case 2:
            firstCard = g->firstCardC;
            secondCard = g->secondCardC;
            break;
        case 3:
            firstCard = g->firstCardD;
            secondCard = g->secondCardD;
            break;
    }
    if (discard != firstCard && discard != secondCard) {
        return 1; 
    }
    return 0;
}

/* Print a NO message to the player supplied
 */
void print_no(struct Game *g, int player) {
    switch (player) {
        case 0:
            fprintf(g->toA, "NO\n");
            break;
        case 1:
            fprintf(g->toB, "NO\n");
            break;
        case 2:
            fprintf(g->toC, "NO\n");
            break;
        case 3:
            fprintf(g->toD, "NO\n");
    }
    flush_streams(g);
}

/* Print a YES message to the player supplied
 */
void print_yes(struct Game *g, int player) {
    switch (player) {
        case 0:
            fprintf(g->toA, "YES\n");
            break;
        case 1:
            fprintf(g->toB, "YES\n");
            break;
        case 2:
            fprintf(g->toC, "YES\n");
            break;
        case 3:
            fprintf(g->toD, "YES\n");
    }
    flush_streams(g);
}

/* Get the current hand (firstCard) of the specified player, when it is not
 * their turn. 
 * @params g The game structure 
 * @params player
 * @return The firstCard (hand) of the relevant player. Blank if specified 
 * player is not A,B,C or D: which should not occur.
 */
char get_hand(struct Game *g, char player) {
    switch (player) {
        case 'A':
            return (g->firstCardA);
        case 'B':
            return (g->firstCardB);
        case 'C':
            return (g->firstCardC);
        case 'D':
            return (g->firstCardD);
    }
    return '-';
}

/* Marks a player as out by setting their cards to '!' and decrements the
 * alive player count by one. If the player supplied is not A, B, C or D
 * no action is taken.
 * @params g The game structure
 * @params out The player to mark as out
 */
void set_player_out(struct Game *g, char out) {
    switch (out) {
        case 'A':
            g->firstCardA = '!';
            g->secondCardA = '!';
            break;
        case 'B':
            g->firstCardB = '!';
            g->secondCardB = '!';
            break;
        case 'C':
            g->firstCardC = '!';
            g->secondCardC = '!';
            break;
        case 'D':
            g->firstCardD = '!';
            g->secondCardD = '!';
            break;
        default:
            return;

    }
    g->alivePlayers--;
}

/* Sends a thishappened message to all players reporting what happened as a
 * result of the last players turn. '-' is used if a param is N/A. If a 
 * player becomes out then they are set to be out.
 * @params g The game structure 
 * @params source The label of the player whose turn it was
 * @params discard The card discarded by source player
 * @params target The player targeted by the source player. 
 * @params guess The card guessed by the source player. 
 * @params dropper The player who dropped a card as a result of the turn
 * @params dropped The card dropped (by the dropper) as a result of the turn
 * @params out The player who became out due to the turn
 */
void this_happened(struct Game *g, char source, char discard, char target, 
        char guess, char dropper, char dropped, char out) {
    char message[23];

    sprintf(message, "thishappened %c%c%c%c/%c%c%c\n", source, discard, 
            target, guess, dropper, dropped, out);

    fprintf(g->toA, "%s", message);
    fprintf(g->toB, "%s", message);
    if (g->players > 2) {
        fprintf(g->toC, "%s", message);
    }
    if (g->players > 3) {
        fprintf(g->toD, "%s", message);
    }

    flush_streams(g);

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

    set_player_out(g, out); 
}

/* Given a supplied target and source player compares their hands and 
 * sets the lower hand as out. If hands are equal no one is set as out.
 * This is called when a player plays card '3'.
 * The result is then sent to the players with a thishappened message.
 * @params g The game structure 
 * @params target the player targeted for comparison by the source player
 * @params source the player who caused this action by player the '3' card
 */
void get_out(struct Game *g, char target, char source) {
    char out = '-', nill = '-', hand = 0, discard = '3';

    if (get_hand(g, source) < get_hand(g, target)) {
        out = source;
    } else if (get_hand(g, source) > get_hand(g, target)) {
        out = target;
    }
    if (out != '-') {
        hand = get_hand(g, out);
        this_happened(g, source, discard, target, nill, out, hand, out);
    } else {
        this_happened(g, source, discard, target, nill, nill, nill, nill);
    }
}

/* Sends a replace message to the specified player, their current hand
 * is changed to the card supplied
 * @params g The game structure 
 * @params target the player targeted by the replace message
 * @params card the card to replace the targets hand with
 */
void replace_hand(struct Game *g, char target, char card) {

    switch (target) {
        case 'A':
            g->firstCardA = card;
            fprintf(g->toA, "replace %c\n", card);
            break;
        case 'B':
            g->firstCardB = card;
            fprintf(g->toB, "replace %c\n", card);
            break;
        case 'C':
            g->firstCardC = card;
            fprintf(g->toC, "replace %c\n", card);
            break;
        case 'D':
            g->firstCardD = card;
            fprintf(g->toD, "replace %c\n", card);
            break;
    }
    flush_streams(g);
}

/* Forces a player to discard their hand and take a new card by replacing
 * their current card with a newly drawn card (from deck or the burntCard)
 * If the discard causes a player to drop an eight, they are also set as out
 * A thishappened message is then sent to all players. 
 * @params g The game structure 
 * @params source the player who forced the discard
 * @params target the player forced to discard their hand
 */
void discard_hand(struct Game *g, char source, char target) {
    char hand = get_hand(g, target), discard = '5', nill = '-', out = '-';
    char newCard;

    if (target == '-') {
        this_happened(g, source, discard, nill, nill, nill, nill, nill);
        return;
    }

    if (hand == '8') {
        out = target;
    } 
    
    newCard = new_card(g);
    if (newCard == 'E') {
        newCard = g->burntCard;
        g->emptyDeck = 1;
    }
    
    replace_hand(g, target, newCard);
    
    this_happened(g, source, discard, target, nill, target, hand, out);
    
}

/* Swaps the hands of the two supplied players and sends a thishappened
 * message reporting the swap (but not contents of the hands) to all players.
 * @params g The game structure 
 * @params target the player forced to swap
 * @params source the player who initiated the swap
 */
void swap_hands(struct Game *g, char target, char source) {
    char discard = '6', nill = '-';
    char targetHand = get_hand(g, target);
    char sourceHand = get_hand(g, source);

    if (target != '-') {
        replace_hand(g, target, sourceHand);
        replace_hand(g, source, targetHand);
    }

    this_happened(g, source, discard, target, nill, nill, nill, nill);
}

/* Invoked when a player plays an eight (and has card position set to '-')
 * The card that is not '-' is returned.
 * @return the value of the card that's not blank ('-')
 */
char get_other_card(struct Game *g, char player) {
    char firstCard, secondCard;

    switch (player) {
        case 'A':
            firstCard = g->firstCardA;
            secondCard = g->secondCardA;
            break;
        case 'B':
            firstCard = g->firstCardB;
            secondCard = g->secondCardB;
            break;
        case 'C':
            firstCard = g->firstCardC;
            secondCard = g->secondCardC;
            break;
        case 'D':
            firstCard = g->firstCardD;
            secondCard = g->secondCardD;
            break;
    }
    if (firstCard == '8' && secondCard != '-') {
        return secondCard;
    }
    return firstCard;
}

/* Checks to see if a player is out or not.
 * @return 1 if player is out, a 0 if still alive
 */
int check_target_out (struct Game *g, char player) {
    char firstCard, secondCard;

    switch (player) {
        case 'A':
            firstCard = g->firstCardA;
            secondCard = g->secondCardA;
            break;
        case 'B':
            firstCard = g->firstCardB;
            secondCard = g->secondCardB;
            break;
        case 'C':
            firstCard = g->firstCardC;
            secondCard = g->secondCardC;
            break;
        case 'D':
            firstCard = g->firstCardD;
            secondCard = g->secondCardD;
            break;
    }
    if (firstCard == '!' || secondCard == '!') {
        return 1;
    }
    return 0;
}

/* Receives a move from a player and checks to see if it was valid.
 * If valid the results of this move are calculated and printed to stdout
 * and sent to all players as a thishappened message. 
 * @params g The game structure 
 * @params player The player who made the move
 * @return 1 is returned if player exited (get_move hit EOF), otherwise 0
 */
int process_move(struct Game *g, int player) {
    if (get_move(g, player)) {
        return 1;
    }

    char source = g->move[3], discard = g->move[0], target = g->move[1],
            guess = g->move[2], nill = '-';

    while (check_card_held(g, player, discard) || 
            check_valid_move(source, discard, target, guess, g->players) ||
            check_target_out(g, target)) {
        print_no(g, player);
        if (get_move(g, player)) {
            return 1;
        }
        source = g->move[3]; 
        discard = g->move[0]; 
        target = g->move[1];
        guess = g->move[2];
    }
    print_yes(g, player);
    clear_second_card(g, source, discard);

    switch (discard) {
        case '1':
            if (guess == get_hand(g, target)) {
                this_happened(g, source, discard, target, guess, target, 
                        guess, target);
            } else {
                this_happened(g, source, discard, target, guess, nill, nill, 
                        nill);
            }
            break;
        case '3':
            get_out(g, target, source);
            break;
        case '5':
            discard_hand(g, source, target);
            break;            
        case '6':
            swap_hands(g, target, source);
            break;
        case '8':
            this_happened(g, source, discard, nill, nill, source, 
                    get_other_card(g, source), source);
            break;
        default:
            this_happened(g, source, discard, nill, nill, nill, nill, nill);
    }
    return 0;
}

/* Sends a yourturn message to the indicated player telling them their card
 */
void send_your_turn(struct Game *g, int player, char card) {
    switch (player) {
        case 0:
            fprintf(g->toA, "yourturn %c\n", card);
            g->secondCardA = card;
            break;
        case 1: 
            fprintf(g->toB, "yourturn %c\n", card);
            g->secondCardB = card;
            break;
        case 2:
            fprintf(g->toC, "yourturn %c\n", card);
            g->secondCardC = card;
            break;
        case 3:
            fprintf(g->toD, "yourturn %c\n", card);
            g->secondCardD = card;
            break;
    }
}

/* A turn of the game is repeatedly played until there is only one alive 
 * player left or the deck runs out of cards. Alive players are given a new
 * card in turn and their move is processed.Once a round is over the winner(s)
 * are printed and housekeeping is performed and the next deck readied. 
 * @params g The game structure 
 * @return A 1 is returned if EOF is sent by the player instead of a move
 * otherwise 0 is returned.
 */
int play_round(struct Game *g) {
    char card;
    int player;

    while (g->alivePlayers > 1) { 
        for (player = 0; player < g->players; ++player) {
            if (player_dead(g, player)) {
                continue;
            }
            if (g->alivePlayers < 2) {
                end_of_round(g);
                return 0;
            }
            if (!(g->emptyDeck)) {
                card = new_card(g);
            } else {
                end_of_round(g);
                return 0;
            }
            if (card != 'E') {
                send_your_turn(g, player, card);		
                flush_streams(g);
				
                if (process_move(g, player)) {
                    game_over(g);
                    return 1;
                }
            } else {
                g->emptyDeck = 1;
                end_of_round(g);
                return 0;
            }
        }
    }
    end_of_round(g);
    return 0;
}


/* Initiate a new round by distributing new cards to the playing players
 * using the newround message and setting the number of alive players to
 * the number of players. Also makes sure to set aside the first card in
 * the deck
 * @params g The game structure 
 */
void new_round(struct Game *g) {

    g->burntCard = new_card(g);
    
    g->firstCardA = new_card(g);
    g->firstCardB = new_card(g);
    g->firstCardC = '-';
    g->firstCardD = '-';
    g->secondCardA = '-';
    g->secondCardB = '-';
    g->secondCardC = '-';
    g->secondCardD = '-';
    g->alivePlayers = g->players;
   
    fprintf(g->toA, "newround %c\n", g->firstCardA);
    fprintf(g->toB, "newround %c\n", g->firstCardB);

    if (g->players > 2) {
        g->firstCardC = new_card(g);
        fprintf(g->toC, "newround %c\n", g->firstCardC);
    }
    if (g->players > 3) {
        g->firstCardD = new_card(g);
        fprintf(g->toD, "newround %c\n", g->firstCardD);
    }
    flush_streams(g);
}

/* Sends the required game information (palyer number and player names) to 
 * each of the participating players.
 */
void send_game_info(struct Game *g) {
    int playerNo = g->players;

    char *playerA = g->playerAName, *playerB = g->playerBName;
    char *playerC = g->playerCName, *playerD = g->playerDName;

    if (g->players == 2) {
        fprintf(g->toA, "%d A\n%s\n%s\n", playerNo, playerA, playerB);
        fprintf(g->toB, "%d B\n%s\n%s\n", playerNo, playerA, playerB);
        return;
    }
    if (g->players == 3) {
        fprintf(g->toA, "%d A\n%s\n%s\n%s\n", playerNo, playerA, playerB, 
                playerC);
        fprintf(g->toB, "%d B\n%s\n%s\n%s\n", playerNo, playerA, playerB, 
                playerC);
        fprintf(g->toC, "%d C\n%s\n%s\n%s\n", playerNo, playerA, playerB, 
                playerC);
        return;
    }
    if (g->players == 4) {
        fprintf(g->toA, "%d A\n%s\n%s\n%s\n%s\n", playerNo, playerA, playerB, 
                playerC, playerD);
        fprintf(g->toB, "%d B\n%s\n%s\n%s\n%s\n", playerNo, playerA, playerB, 
                playerC, playerD);
        fprintf(g->toC, "%d C\n%s\n%s\n%s\n%s\n", playerNo, playerA, playerB, 
                playerC, playerD);
        fprintf(g->toD, "%d D\n%s\n%s\n%s\n%s\n", playerNo, playerA, playerB,
                playerC, playerD);
    }

    flush_streams(g);

}


/* Send game information to players and the play rounds of the game until
 * a player reaches 4 points.
 * When the game is over a message is printed indicating the winners. 
 * @return A void pointer is returned if game finished, or ended early due to 
 * one of the players quiting
 */
void* new_game(void* arg) {
    struct Game *g = (struct Game*)arg;

    g->gameReady = 0;

    send_game_info(g);

    while (g->pointsA < 4 && g->pointsB < 4 && g->pointsC < 4 && 
            g->pointsD < 4) {
        new_round(g);
        if (play_round(g)) {
            return NULL;
        }
    }

    fprintf(stdout, "Winner(s):");

    if (g->pointsA == 4) {
        fprintf(stdout, " A");
        g->winnerA = 1;
    }
    if (g->pointsB == 4) {
        fprintf(stdout, " B");
        g->winnerB = 1;
    }
    if (g->pointsC == 4) {
        fprintf(stdout, " C");
        g->winnerC = 1;
    }
    if (g->pointsD == 4) {
        fprintf(stdout, " D");
        g->winnerD = 1;
    }
    fprintf(stdout, "\n");
    fflush(stdout);

    game_over(g);

    flush_streams(g);

    return NULL;
}

/* Create a game struct and initiate all of the members of that struct 
 * The game name is set to the name of the game
 * @return a game struct that has been initiated
 */
struct Game* create_game(char *gameName) {

    struct Game *newGame;
    newGame = malloc(sizeof(*newGame));
    newGame->nextGame = NULL;
    newGame->gameName = NULL;

    if (gameName != NULL) {
        newGame->gameName = gameName;
    }
    newGame->gameReady = 0;
    
    newGame->emptyDeck = 0;
    newGame->nextCard = 0;
    newGame->players = 0;
    newGame->alivePlayers = 0;
    newGame->playerAName = NULL;
    newGame->playerBName = NULL;
    newGame->playerCName = NULL;
    newGame->playerDName = NULL; 
    newGame->pointsA = 0;
    newGame->pointsB = 0;
    newGame->pointsC = 0;
    newGame->pointsD = 0;
    newGame->firstCardA = '-';
    newGame->firstCardB = '-';
    newGame->firstCardC = '-';
    newGame->firstCardD = '-';
    newGame->secondCardA = '-';
    newGame->secondCardB = '-';
    newGame->secondCardC = '-';
    newGame->secondCardD = '-';
    newGame->fdA = 0;
    newGame->fdB = 0;
    newGame->fdC = 0;
    newGame->fdD = 0;             
    newGame->fromA = NULL;
    newGame->fromB = NULL;
    newGame->fromC = NULL;
    newGame->fromD = NULL;
    newGame->toA = NULL;
    newGame->toB = NULL;
    newGame->toC = NULL;
    newGame->toD = NULL;
    newGame->winnerA = 0;
    newGame->winnerB = 0;
    newGame->winnerC = 0;
    newGame->winnerD = 0;

    return newGame;
}

/* Gets a message (of any size) from the supplied player file stream
 * @return a pointer to a heap allocated message
 */
char* get_message (FILE *fromPlayer) {
    int i = 0;
    char *buffer = calloc(2, sizeof(char));
    char messagePart;

    while(1) {
        messagePart = fgetc(fromPlayer);

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
        return NULL;
    }

    return buffer;
}

/* Get the number of players who participated in a game
 * @return the number of players who participated in a game
 */
int get_player_no(struct Game *g) {
    int playerNo = 0;

    if (g->playerAName != NULL) {
        playerNo++;
    }
    if (g->playerBName != NULL) {
        playerNo++;
    }
    if (g->playerCName != NULL) {
        playerNo++;
    }
    if (g->playerDName != NULL) {
        playerNo++;
    }
    return playerNo;
}

/* Put player information back into the game struct in alphabetical order
 * This information was sorted by a the function that calls this function
 */
void put_sorted_players_back(struct Game *sort, char **names, FILE **from, 
        FILE **to, int *fd) {
    sort->playerAName = names[0]; //Function to put theseBack
    sort->playerBName = names[1];
    sort->playerCName = names[2];
    sort->playerDName = names[3];

    sort->fromA = from[0];
    sort->fromB = from[1];
    sort->fromC = from[2];
    sort->fromD = from[3];

    sort->toA = to[0];
    sort->toB = to[1];
    sort->toC = to[2];
    sort->toD = to[3];

    sort->fdA = fd[0];
    sort->fdB = fd[1];
    sort->fdC = fd[2];
    sort->fdD = fd[3];
}	

/* Sort by player name such that playerA is the lowest value when all values
 * are compared with strcmp. PlayerB is teh next lowest and so on.
 * Also makes sure to move all the other data associated with each player 
 */
void sort_players(struct Game *sort) {
    char *nameTemp, *names[4];
    FILE *fromTemp, *from[4];
    FILE *toTemp, *to[4];
    int fdTemp, fd[4], playersToSort, unSorted = 0;

    names[0] = sort->playerAName;
    names[1] = sort->playerBName;
    names[2] = sort->playerCName;
    names[3] = sort->playerDName;

    from[0] = sort->fromA;
    from[1] = sort->fromB;
    from[2] = sort->fromC;
    from[3] = sort->fromD;

    to[0] = sort->toA;
    to[1] = sort->toB;
    to[2] = sort->toC;
    to[3] = sort->toD;

    fd[0] = sort->fdA;
    fd[1] = sort->fdB;
    fd[2] = sort->fdC;
    fd[3] = sort->fdD;

    playersToSort = (get_player_no(sort) - 1); 
  
    do {
        unSorted = 0;
        for (int i = 0; i < playersToSort; ++i) {
            if (strcmp(names[i], names[i + 1]) > 0) {
                nameTemp = names[i];
                names[i] = names[i + 1];
                names[i + 1] = nameTemp;
                fromTemp = from[i];
                from[i] = from[i + 1];
                from[i + 1] = fromTemp;
                toTemp = to[i];
                to[i] = to[i + 1];
                to[i + 1] = toTemp;
                fdTemp = fd[i];
                fd[i] = fd[i + 1];
                fd[i + 1] = fdTemp;
                unSorted = 1;
            }
        }
    } while (unSorted);
	
    put_sorted_players_back(sort, names, from, to, fd);
}

/* Add a new player to the next player that has not yet been added in a game
 */
void add_new_player(struct Game *gameWait, FILE *fromPlayer, FILE *toPlayer, 
        int fd, char* playerName) {
    if (gameWait->playerAName == NULL) {
        gameWait->playerAName = playerName;
        gameWait->fromA = fromPlayer;
        gameWait->toA = toPlayer;
        gameWait->fdA = fd;
    } else if (gameWait->playerBName == NULL) {
        gameWait->playerBName = playerName;
        gameWait->fromB = fromPlayer;
        gameWait->toB = toPlayer;
        gameWait->fdB = fd;
        if (gameWait->players == 2) {
            gameWait->gameReady = 1;
        }
    } else if (gameWait->playerCName == NULL) {
        gameWait->playerCName = playerName;
        gameWait->fromC = fromPlayer;
        gameWait->toC = toPlayer;
        gameWait->fdC = fd;
        if (gameWait->players == 3) {
            gameWait->gameReady = 1;
        }
    } else {
        gameWait->playerDName = playerName;
        gameWait->fromD = fromPlayer;
        gameWait->toD = toPlayer;
        gameWait->fdD = fd;
        gameWait->gameReady = 1;
    }
    sort_players(gameWait);
}

/* Gets information from player and then adds that player to the supplied game
 * If a game is full then a new game is created.
 */
void add_to_game(struct Game *headGame, int fd) {
    FILE *fromPlayer, *toPlayer;
    char *gameName, *playerName;
    struct Game *currentGame, *newGame;

    fromPlayer = fdopen(fd, "r");
    toPlayer = fdopen(fd, "w");

    playerName = get_message(fromPlayer); ///Return NULL, close connection
    gameName = get_message(fromPlayer); /// Return NULL, close connection

    currentGame = headGame;

    if (headGame->gameName == NULL) {
        headGame->gameName = gameName;
        add_new_player(currentGame, fromPlayer, toPlayer, fd, playerName);
        if (gameName[0] == '2') {
            headGame->players = 2;
        } else if (gameName[0] == '3') {
            headGame->players = 3;
        } else {
            headGame->players = 4;
        }
        return;
    }

    while (currentGame != NULL) {
        if (!(strcmp(currentGame->gameName, gameName))) {
            add_new_player(currentGame, fromPlayer, toPlayer, fd, playerName);
            return;
        }
        if (currentGame->nextGame == NULL) {
            break; 
        }
        currentGame = currentGame->nextGame;
    }

    newGame = create_game(gameName);
    currentGame->nextGame = newGame;
    if (gameName[0] == '1') {
        newGame->players = 1;
    } else if (gameName[0] == '2') {
        newGame->players = 2;
    } else {
        newGame->players = 4;
    }
    add_new_player(newGame, fromPlayer, toPlayer, fd, playerName);

}
    

/* Wait for connections from players on a port, adds them to games indicated,
 * then  starts new games with a new thread.
 @return doesn't return, but a void pointer is indicated
 */
void* connection_wait(void* arg) {
    struct Port *currentPort = (struct Port*)arg;

    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;
    int error, fdServer = currentPort->fd;
    char hostname[MAXHOSTNAMELEN];//WHATS GOING ON HERE
    pthread_t threadId;
    struct Game *headGame, *currentGame;

    headGame = create_game(NULL);
    currentPort->headGame = headGame;

    while(1) {
        fromAddrSize = sizeof(struct sockaddr_in);
        // Block, waiting for new connection. (fromAddr structure will be 
        // populated with details of client address
        fd = accept(fdServer, (struct sockaddr*)&fromAddr, &fromAddrSize);
        if(fd < 0) {
            perror("Error accepting connection");
            exit(1);
        }
     
        // Convert IP address to hostname
        error = getnameinfo((struct sockaddr*)&fromAddr, fromAddrSize, 
                hostname, MAXHOSTNAMELEN, NULL, 0, 0);
        if(error) {
            fprintf(stderr, "Error getting hostname: %s\n", 
                    gai_strerror(error));
        } else {
            add_to_game(headGame, fd);
        }
        
        currentGame = headGame; 

        while (currentGame != NULL) {
            if (currentGame->gameReady) {
                currentGame->currentDeck = currentPort->firstDeck;
                pthread_create(&threadId, NULL, new_game, (void*)currentGame);
                pthread_detach(threadId);
            }
            currentGame = currentGame->nextGame;
        }
    }
    return NULL;
}

/* Checks to make sure the port indicated hasn't already been used
 * Exits with appropriate message if it has
 */
void check_for_duplicate_port(struct Server *s, int port, struct Port *head) {
    struct Port *currentPort;

    currentPort = head;

    while (currentPort->nextPort != NULL) {
        if (currentPort->port == port || s->adminPort == port) {
            exit_server(s, BAD_PORT);
        }
        currentPort = currentPort->nextPort;
    }
}

/* Opens a port as supplied by the admin and returns a file descriptor for that port
 * @returns file descripter for port supplied
 */
int open_admin_listen(int port, FILE *toAdmin) {
    int fd; ///MAYBE CHANGE THIS
    struct sockaddr_in serverAddr;
    int optVal;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        fprintf(toAdmin, "Unable to listen on port\n");
        return 0;
    }

    optVal = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int)) < 0) {
        fprintf(toAdmin, "Unable to listen on port\n");
        return 0;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(fd, (struct sockaddr*)&serverAddr, 
            sizeof(struct sockaddr_in)) < 0) {
        fprintf(toAdmin, "Unable to listen on port\n");
        return 0;
    }

    if(listen(fd, SOMAXCONN) < 0) {
        fprintf(toAdmin, "Unable to listen on port\n");
        return 0;
    }

    return fd;
}

/* Open a new port to listen for games (on the end of Port Struct linked list)
 * as supplied by the admin.
 */
void open_new_port(struct Server *s, int port, char* deckfile, FILE *toAdmin) {
    struct Port *headPort = s->headPort, *currentPort, *newPort;
    int deckError;
    pthread_t threadId;

    currentPort = headPort;

    while (currentPort->nextPort != NULL) {
        if (currentPort->port == port || s->adminPort == port) {
            fprintf(toAdmin, "Invalid port number\n");
            return;
        }
        currentPort = currentPort->nextPort;
    } // After this current port will be last port
    
    newPort = malloc(sizeof(*newPort));
    currentPort->nextPort = newPort;
    newPort->nextPort = NULL;
    newPort->port = port;
    newPort->deckfile = malloc(sizeof(strlen(deckfile) + 1));
    strncpy(newPort->deckfile, deckfile, (strlen(deckfile) + 1));

    deckError = load_deckfile(s, newPort);
    if (deckError == DECKFILE_FAIL) {
        fprintf(toAdmin, "Unable to access deckfile\n");
        currentPort->nextPort = NULL;
        free(newPort);
        return;
    } else if (deckError == DECK_FAIL) {
        fprintf(toAdmin, "Error reading deck\n");
        currentPort->nextPort = NULL;
        free(newPort);
        return;
    }

    newPort->fd = open_admin_listen(port, toAdmin);

    if (newPort->fd == 0) {
        currentPort->nextPort = NULL;
        free(newPort);
        return;
    }
    
    pthread_create(&threadId, NULL, connection_wait, (void*)newPort);
    pthread_detach(threadId);
    fprintf(toAdmin, "OK\n");
    return;
}

/* Create a new Player struct and indiatlise it to NULL/0 values
 * @return return a Player Struct
 */
struct Players* create_player(void) {
    struct Players *newPlayer;

    newPlayer = malloc(sizeof(*newPlayer));

    newPlayer->nextPlayer = NULL;
    newPlayer->name = NULL;
    newPlayer->gamesPlayed = 0;
    newPlayer->roundsWon = 0;
    newPlayer->gamesWon = 0;

    return newPlayer;
}

/* Checks if the supplied player name already exits in list of players
 * @return 1 if it does exit, 0 otherwise
 */
int check_if_new_player(struct Players *headPlayer, char *name) {
    struct Players *currentPlayer;

    currentPlayer = headPlayer;

    if (name == NULL) {
        return 0;
    }

    while (currentPlayer != NULL) {
        if (!strcmp(currentPlayer->name, name)) {
            return 0;
        }
        currentPlayer = currentPlayer->nextPlayer;
    }
    return 1;
}

/* Add game stats for a new player to list of players
 */
void add_new_player_stats(struct Server *s, char *name, int points, 
        int winner) {
    struct Players *newPlayer, *headPlayer, *previousPlayer;

    headPlayer = s->headPlayer;
    previousPlayer = headPlayer;

    while (previousPlayer->nextPlayer != NULL) {
        previousPlayer = previousPlayer->nextPlayer;
    }

    newPlayer = create_player();
    newPlayer->name = name;
    newPlayer->gamesPlayed = 1;
    newPlayer->roundsWon = points;
    newPlayer->gamesWon = winner;
    previousPlayer->nextPlayer = newPlayer;
}

/* Update the statistics with new information for the player supplied
 */
void update_stats(struct Players *p, int points, int winner) {
    p->roundsWon = (p->roundsWon) + points;
    p->gamesWon = (p->gamesWon) + winner;
    p->gamesPlayed = (p->gamesPlayed) + 1;
}

/* Adds a new player or updates an existing player game statistics
 */
void add_player_stats(struct Game *g, struct Server *s) {
    struct Players *headPlayer, *currentPlayer;
    int skipA = 0;

    headPlayer = s->headPlayer;
    currentPlayer = headPlayer;

    if (headPlayer->nextPlayer == NULL) {
        headPlayer->name = g->playerAName;
        headPlayer->roundsWon = g->pointsA;
        headPlayer->gamesWon = g->winnerA;
        headPlayer->gamesPlayed = 1;
        skipA = 1;
    }

    while (currentPlayer != NULL) {
        if ((!strcmp(g->playerAName, currentPlayer->name)) && !skipA) {
            update_stats(currentPlayer, g->pointsA, g->winnerA);
        } else {
            skipA = 0;
        }
        if (!(strcmp(g->playerBName, currentPlayer->name))) {
            update_stats(currentPlayer, g->pointsB, g->winnerB);
        }
        
        if (g->playerCName != NULL && !(strcmp(g->playerCName, 
                currentPlayer->name))) {
            update_stats(currentPlayer, g->pointsC, g->winnerC);
        }
        if (g->playerDName != NULL && !(strcmp(g->playerDName, 
                currentPlayer->name))) {
            update_stats(currentPlayer, g->pointsD, g->winnerD);
        }
        currentPlayer = currentPlayer->nextPlayer;
    }

    if (check_if_new_player(headPlayer, g->playerAName)) {
        add_new_player_stats(s, g->playerAName, g->pointsA, g->winnerA);
    }
    if (check_if_new_player(headPlayer, g->playerBName)) {
        add_new_player_stats(s, g->playerBName, g->pointsB, g->winnerB);
    }
    if (check_if_new_player(headPlayer, g->playerCName)) {
        add_new_player_stats(s, g->playerCName, g->pointsC, g->winnerC);
    }
    if (check_if_new_player(headPlayer, g->playerDName)) {
        add_new_player_stats(s, g->playerDName, g->pointsD, g->winnerD);
    }
}   

/* Checks if the supplied name is the lowest name (with strcmp) out of those 
 * that have not been printed yet
 * @return 0 if it is not lowest, 1 if is lowest
 */
int check_if_first_name(struct Players *headPlayer, 
        struct Players *checkPlayer, char **printedPlayers, int playerNo) {
    char *nameCheck = checkPlayer->name;
    int firstName = 0;

    struct Players *currentPlayer = headPlayer;

    for (int j = 0; j < playerNo; j++) {
        if (printedPlayers[j] != NULL && 
                (!strcmp(printedPlayers[j], nameCheck))) {
            return 0;
        }
    }
    

    while (currentPlayer != NULL) {
        if (strcmp(nameCheck, currentPlayer->name) > 0) {
            firstName = 0; 
        } else {
            firstName = 1;
        }
        for (int i = 0; i < playerNo; i++) {
            if (printedPlayers[i] != NULL && 
                    (!strcmp(printedPlayers[i], currentPlayer->name))) {
                firstName = 1;
            }
        }
        if (firstName == 0) {
            return 0;
        } else {
            currentPlayer = currentPlayer->nextPlayer;
        }
    }
    return 1;
}

/* Prints the game statistics for each player 
 */
void print_statistics(struct Players *headPlayer, FILE *toAdmin) {
    struct Players *currentPlayer;
    int playerNo = 0, i = 0;

    currentPlayer = headPlayer;

    while (currentPlayer != NULL) {
        playerNo++;
        currentPlayer = currentPlayer->nextPlayer;
    }

    char *printedPlayers[playerNo]; 

    for (int k = 0; k < playerNo; k++) {
        printedPlayers[k] = NULL;
    }

    currentPlayer = headPlayer;

    while (currentPlayer != NULL) {
        if(check_if_first_name(headPlayer, currentPlayer, printedPlayers, 
                playerNo)) {
            fprintf(toAdmin, "%s,%d,%d,%d\n", currentPlayer->name, 
                    currentPlayer->gamesPlayed, currentPlayer->roundsWon, 
                    currentPlayer->gamesWon);
            printedPlayers[i] = currentPlayer->name;
            i++;
            if (i != playerNo) {
                currentPlayer = headPlayer;
            } else {
                currentPlayer = currentPlayer->nextPlayer;
            }
        } else {
            currentPlayer = currentPlayer->nextPlayer;
        }
    }
    fprintf(toAdmin, "OK\n");
}


/* Generate the game statistics and the print them to admin
 */
void get_statistics(struct Server *s, FILE *toAdmin) {
    struct Players *headPlayer;
    struct Port *currentPort;
    struct Game *currentGame;

    ///SEMAPHORE

    headPlayer = create_player();
    s->headPlayer = headPlayer;

    currentPort = s->headPort;

    if (currentPort == NULL || currentPort->headGame->playerAName == NULL) {
        return;
    }

    while (currentPort != NULL) {
        currentGame = currentPort->headGame;
        while (currentGame != NULL) {
            if (currentGame->playerAName != NULL) {
                add_player_stats(currentGame, s);
            }
            currentGame = currentGame->nextGame;
        }
        currentPort = currentPort->nextPort;
    }
    
    print_statistics(headPlayer, toAdmin);
}

/* Wait on the admin port for a connection and a message
 * Ignore all messages but P and S. Perform P and S commands
 */
void admin_wait(struct Server *s) {
    int fd = 0, fdAdmin = s->fdAdminPort, maxLength, argNo, newPort;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;
    char hostname[MAXHOSTNAMELEN], *adminMessage, adminCommand;
    FILE *fromAdmin, *toAdmin;

    while (1) {
        fromAddrSize = sizeof(struct sockaddr_in);

        //if (!fd) {
        fd = accept(fdAdmin, (struct sockaddr*)&fromAddr, &fromAddrSize);
        //}

        getnameinfo((struct sockaddr*)&fromAddr, fromAddrSize,
                hostname, MAXHOSTNAMELEN, NULL, 0, 0);
        fromAdmin = fdopen(fd, "r");
        toAdmin = fdopen(fd, "w"); //WHILE FROM HERE TO END

        adminMessage = get_message(fromAdmin);
        if (adminMessage == NULL) {
            close(fd);
            fd = 0;
            break;
        }
		
        maxLength = strlen(adminMessage);
        char deck[maxLength];
        argNo = sscanf(adminMessage, "%c%d %s", &adminCommand, &newPort, 
                deck);   

        if (adminCommand == 'P' && argNo == 3) {
            open_new_port(s, newPort, deck, toAdmin);
        } else if (adminCommand == 'S' && argNo == 1) {
            get_statistics(s, toAdmin);
        }
 		
        fflush(toAdmin);
    }
}

/* Create the head Port strucutre and put it in the Server structure
 * @Return the address of the head strucutre
 */
struct Port* create_head(struct Server *s) {
    struct Port *head;
	
    head = malloc(sizeof(*head));
    head->nextPort = NULL;
    s->headPort = head;	
	
    return head;
}

/* Pars arguments supplied on the commandline;
 * load decks, and listen on appropriate ports
 * Play games when the games are full
 */
void parse_args(struct Server *s, char *argv[], int argc) {
    char *next;
    int i, deckError;
    pthread_t threadId;
    struct Port *head, *previous, *new, *currentPort;

    if (argc % 2 != 0 || argc == 1) {
        exit_server(s, BAD_ARGS);
    }
	
    head = create_head(s);
    check_valid_port(s, argv[1]);
    s->adminPort = strtol(argv[1], &next, 10);
    for (i = 2; i < argc; i += 2) {
        check_valid_port(s, argv[i]);
    }

    if (argc > 2) {
        head->port = strtol(argv[2], &next, 10);
        if (head->port == s->adminPort) {
            exit_server(s, BAD_PORT);
        }
        head->deckfile = malloc(sizeof(strlen(argv[3]) + 1));
        strncpy(head->deckfile, argv[3], (strlen(argv[3]) + 1));
        previous = head;
        for (i = 4; i < argc; i += 2) {
            new = malloc(sizeof(*new));
            previous->nextPort = new;
            new->nextPort = NULL;   
            new->port = strtol(argv[i], &next, 10);
            new->deckfile = malloc(sizeof(strlen(argv[i + 1]) + 1));
            strncpy(new->deckfile, argv[i + 1], (strlen(argv[i + 1]) + 1));
            previous = new;
            check_for_duplicate_port(s, new->port, head);
        }
    }

    currentPort = head;
    s->fdAdminPort = open_listen(s, s->adminPort);

    while (currentPort != NULL && argc > 2) {
        currentPort->fd = open_listen(s, currentPort->port);
        if ((deckError = load_deckfile(s, currentPort))) {
            exit_server(s, deckError);
        }
        pthread_create(&threadId, NULL, connection_wait, (void*)currentPort);
        pthread_detach(threadId);
        currentPort = currentPort->nextPort;
    }
    admin_wait(s);
}

int main(int argc, char *argv[]) {
    struct Server *s = NULL;

    s = malloc(sizeof(*s));

    parse_args(s, argv, argc);

    //sem_init(&scoresUpdate, 0, 0);

}
