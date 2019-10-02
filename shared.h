/* Shared.h - Michael Scotson
 */

#ifndef SHARED_H_
#define SHARED_H_

// Function Prototypes
int check_card(char card);
int check_player(char player, int players);
int check_valid_move(char source, char discard, char target, char guess,
        int players);

#endif

