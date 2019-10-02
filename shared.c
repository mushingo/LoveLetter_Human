/* Check if the supplied character is a valid card in the love letter game. 
 * Valid cards are '1', '2', '3', '4', '5', '6', '7' or '8'.
 * @params card The card to check_card
 * @return 0 if the card is valid, 1 if the card is invalid
 */
int check_card(char card) {
    if ((card < '1' || card > '8') && card != '-') {
        return 1;
    }
    return 0;
}

/* Check if the a player label is valid for the number of players in the game
 * A, B in two player, A,B,C in three player and A,B,C,D in four player games
 * Also checks that the number of players is valid (between 2 - 4)
 * @params player the player label to check_card
 * @return returns a 1 if the player label is invalid and a 0 if label valid
 */
int check_player(char player, int players) {

    if (players < 2 || players > 4) {
        return 1;
    }

    if ((player > (players + 64)) || player < 'A') {
        return 1;
    }

    return 0;
}

/* Check if the move reported is valid according to the rules of love letter.
 * @params source The player making the move
 * @params discard The card the player dsicarded
 * @params target The player targeted by the discarder
 * @params guess The card guessed by the player
 * @params players The number of players playing the game
 * @return A 1 if the move is invalid and a 0 if the move is valid.
 */
int check_valid_move(char source, char discard, char target, char guess, 
        int players) {

    if (((source == target) && discard != '5') || (discard == '5' &&
            target == '-')) {
        return 1;
    }

    if (check_player(source, players) || check_card(discard) || 
            check_card(guess)) {
        return 1;
    }

    if (discard != '1' && guess != '-') {
        return 1;
    }

    switch (discard) {
        case '6':
        case '5':
        case '3':
            if (check_player(target, players)) {
                if (target != '-') {
                    return 1;
                }
            }
            break;
        case '1':
            if (target == '-' && guess != '-') {
                return 1;
            }
            if (guess == '-' && target != '-') {
                return 1;
            }
            break;
        default: 
            if ((guess != '-') || (target != '-')) {
                return 1;
            }
    }
    return 0;
}
