/*
===============================================================================
 Name        : connect_four.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>

// TODO: insert other include files here

// TODO: insert other definitions and declarations here
static bool board[6][7];

typedef struct {
	int x, y;
} Move;

void make_move(int y) {
	if(!valid_move(y)){
		exit(0);
	}
	Move move = get_move(y);
	board[move->x][move->y] = true;
}

bool valid_move(int y) {
	if(board[0][y] == 0){
		return true;
	}
	return false;
}

Move get_move(int y) {
	Move move;
	int x = 0;

	move->y = y;
	while(board[x][y] == false && x <= 5){
		x++;
	}
	move->x = x-1;
	return move;
}

bool win_check(Move move) {
	int i, j;
	bool victory = false;
	for(i = 0; i < 6; i++){
		for(j = 0; j < 7; j++){
			if(i < 3){ // check vertical
				victory = board[i][j] && board[i+1][j] && board[i+2][j] && board[i+3][j];
			}
			if(j < 4){ // check horizontal
				victory = board[i][j] && board[i][j+1] && board[i][j+2] && board[i][j+3];
			}
			if(i < 3 && j < 4){ // check right diagonal
				victory = board[i][j] && board[i+1][j+1] && board[i+2][j+2] && board[i+3][j+3];
			}
			if(i < 3 && j > 2){ // check left diagonal
				victory = board[i][j] && board[i+1][j-1] && board[i+2][j-2] && board[i+3][j-3];
			}
		}
	}
	return victory;
}

int main(void) {

#if defined (__USE_LPCOPEN)
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();
#if !defined(NO_BOARD_LIB)
    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();
    // Set the LED to the state of "On"
    Board_LED_Set(0, true);
#endif
#endif

    // TODO: insert code here

    // Force the counter to be placed into memory
    volatile static int i = 0 ;
    // Enter an infinite loop, just incrementing a counter
    while(1) {
        i++ ;
    }
    return 0 ;
}
