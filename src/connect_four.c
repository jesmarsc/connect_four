/*
===============================================================================
 Name        : connectfour.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#include <stdlib.h>
#include <string.h>
#include <board.h>


/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
#define DEFAULT_I2C          I2C0

#define I2C_EEPROM_BUS       DEFAULT_I2C
#define I2C_IOX_BUS          DEFAULT_I2C

#define SPEED_100KHZ         100000
#define SPEED_400KHZ         400000

static int mode_poll;   /* Poll/Interrupt mode flag */
static I2C_ID_T i2cDev = DEFAULT_I2C; /* Currently active I2C device */

/* EEPROM SLAVE data */
#define I2C_SLAVE_EEPROM_SIZE       64
#define I2C_SLAVE_EEPROM_ADDR       0x5A
#define I2C_SLAVE_TEMP_ADDR          0x24

/* Xfer structure for slave operations */
static I2C_XFER_T temp_xfer;
static I2C_XFER_T iox_xfer;

static uint8_t i2Cbuffer[2][256];
uint32_t drop = -16778241;
uint32_t left = -25165825;
uint32_t right = -150994945;
bool fDebouncing;

uint8_t redbuffer[8];
uint8_t greenbuffer[8];
uint8_t fullBuff[16];
uint8_t boardData[8][7];
uint8_t columnData[8];
uint8_t dropPosition;
bool redTurn;
bool drawFlag;
bool dropFlag;
bool moveFlag;

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* State machine handler for I2C0 and I2C1 */
static void i2c_state_handling(I2C_ID_T id)
{
	if (Chip_I2C_IsMasterActive(id)) {
		Chip_I2C_MasterStateHandler(id);
	} else {
		Chip_I2C_SlaveStateHandler(id);
	}
}

/* Set I2C mode to polling/interrupt */
static void i2c_set_mode(I2C_ID_T id, int polling)
{
	if(!polling) {
		mode_poll &= ~(1 << id);
		Chip_I2C_SetMasterEventHandler(id, Chip_I2C_EventHandler);
		NVIC_EnableIRQ(id == I2C0 ? I2C0_IRQn : I2C1_IRQn);
	} else {
		mode_poll |= 1 << id;
		NVIC_DisableIRQ(id == I2C0 ? I2C0_IRQn : I2C1_IRQn);
		Chip_I2C_SetMasterEventHandler(id, Chip_I2C_EventHandlerPolling);
	}
}

/* Initialize the I2C bus */
static void i2c_app_init(I2C_ID_T id, int speed)
{
	Board_I2C_Init(id);

	/* Initialize I2C */
	Chip_I2C_Init(id);
	Chip_I2C_SetClockRate(id, speed);

	/* Set default mode to interrupt */
	i2c_set_mode(id, 0);
}

static void i2c_read_setup(I2C_XFER_T *xfer, uint8_t addr, int numBytes)
{
	xfer->slaveAddr = addr;
	xfer->rxBuff = 0;
	xfer->txBuff = 0;
	xfer->txSz = 0;
	xfer->rxSz = numBytes;
	xfer->rxBuff = i2Cbuffer[1];
}


/*****************************************************************************
 * Public functions
 ****************************************************************************/

void I2C0_IRQHandler(void)
{
	i2c_state_handling(I2C0);
}

void writeDisplay(void){
	Chip_I2C_MasterSend(I2C0, 0x70, 0x00, 1);
	int i;
	for( i = 0; i < 8; i++){
		fullBuff[2*i] = redbuffer[i] & 0xFF;
		fullBuff[2*i + 1] = greenbuffer[i] & 0xFF;
	}
	Chip_I2C_MasterSend(I2C0, 0x70, &fullBuff, 16);
}

void GPIO_IRQHandler(void){
	uint32_t val;
	val = Chip_GPIO_ReadValue(LPC_GPIO, 2);
	if(fDebouncing) {}
	else {
		if(val == drop){
			dropFlag = true;
		}
		else if(val == right){
			moveFlag = true;
		}
		fDebouncing = true;
		Chip_TIMER_Enable(LPC_TIMER0);
	}
	Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT, GPIOINT_PORT2, 1 << 10);
	Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT, GPIOINT_PORT2, 1 << 27);
}

void TIMER0_IRQHandler(void){
	fDebouncing = false; 				  // Update boolean variable
	Chip_TIMER_Disable(LPC_TIMER0);		  // Stop TIMER0
	Chip_TIMER_Reset(LPC_TIMER0);		  // Reset TIMER0
	Chip_TIMER_ClearMatch(LPC_TIMER0,0);  // Clear TIMER0 interrupt
}

void drawPixel(uint16_t x, uint16_t y, uint16_t color) {
  if ((y < 0) || (y >= 8)) return;
  if ((x < 0) || (x >= 8)) return;

  if (color == 1) {
	  // Turn on green LED.
	  greenbuffer[y] |= 1 << (x);
	  // Turn off red LED
	  y+=1;
	  redbuffer[y] &= ~(1 << (x));
  } else if (color == 2) {
	  // Turn off green LED.
	  greenbuffer[y] &= ~(1 << (x));
	  // Turn on red LED.
	  y+=1;
	  redbuffer[y] |= 1 << x;
  } else if (color == 3) {
    // Turn on green and red LED.
    //displaybuffer[y] |= (1 << (x+8)) | (1 << x);
    greenbuffer[y] |= 1 << x;
    y += 1;
    redbuffer[y] |= 1 << x;
  } else if (color == 0) {
    // Turn off green and red LED.
    //displaybuffer[y] &= ~(1 << x) & ~(1 << (x+8));
	  greenbuffer[y] &= ~(1 << x);
	  y+=1;
	  redbuffer[y] &= ~(1 << x);
  }
}

void drawBoard(void){
	int i;
	int j;
	for(i = 0; i < 8; i++){
		for(j = 0; j < 6; j++){
			drawPixel(i, j, 3);
		}
	}
	writeDisplay();
}

void reDraw(void){
	int a;
	int b;
	for(a = 0; a < 8; a++){
		for(b = 0; b < 7; b++){
			uint8_t color;
			color = boardData[a][b];
			drawPixel(a, b, color);
		}
	}
	writeDisplay();
}

void clearBoard(void){
	int i, j;
	for(i = 0; i < 8; i++){
		for(j = 0; j < 7; j++){
			drawPixel(i, j, 0);
		}
	}
	writeDisplay();
}

typedef struct {
	int column, row;
} Move;

Move get_move(int column) {
	Move move;
	int row = 5;

	move.column = column;
	while(boardData[column][row] == 3 && row >= 0){
		row--;
	}
	move.row = row + 1;
	return move;
}

bool valid_move(int column) {
	if(boardData[column][5] == 3){
		return true;
	}
	return false;
}

void move_drop(void){
	boardData[dropPosition][6] = 0;
	if(dropPosition == 0){
		dropPosition = 7;
	}
	else {
		dropPosition -= 1;
	}
	boardData[dropPosition][6] = redTurn + 1;
	reDraw();
}

void make_move(int column) {
	if(!valid_move(column)) {
		return;
	}

	Move move = get_move(column);
	boardData[move.column][move.row] = redTurn + 1;
	redTurn = !redTurn;
	boardData[dropPosition][6] = 0;
	dropPosition = 7;
	boardData[dropPosition][6] = redTurn + 1;
	reDraw();
}

int win_check() {
	int i, j;
	int victory = 0;
	for(i = 0; i < 8; i++){
		for(j = 0; j < 6; j++){
			if(i < 5){ // check horizontal
				victory = boardData[i][j] + boardData[i+1][j] + boardData[i+2][j] + boardData[i+3][j];
			}
			else if((victory != 4) && (victory != 8) && j < 3){ // check vertical
				victory = boardData[i][j] + boardData[i][j+1] + boardData[i][j+2] + boardData[i][j+3];
			}
			else if((victory != 4) && (victory != 8) && i < 5 && j < 3){ // check right diagonal
				victory = boardData[i][j] + boardData[i+1][j+1] + boardData[i+2][j+2] + boardData[i+3][j+3];
			}
			else if((victory != 4) && (victory != 8) && i > 2 && j < 3){ // check left diagonal
				victory = boardData[i][j] + boardData[i+1][j-1] + boardData[i+2][j-2] + boardData[i+3][j-3];
			}
		}
	}
	return victory;
}

int main(void) {
	drawFlag = false;
	dropFlag = false;
	moveFlag = false;
	fDebouncing = false;
	redTurn = false;

	Board_Init();
	SystemCoreClockUpdate();
	i2c_app_init(I2C0, SPEED_400KHZ);
	i2c_set_mode(I2C0, 0);

	//BOARD STATE SETUP
	int x, y;
	for(x = 0; x < 8; x++){
		for(y = 0; y < 7; y++){
			boardData[x][y] = 3;
		}
	}

	boardData[7][6] = 1;
	dropPosition = 7;
	x = 0;
	for(x = 0; x < 7; x++){
		boardData[x][6] = 0;
	}

	uint8_t begin, display, brightness;
	begin = 0x21;		//system setup
	display = 0x81;		//display setup
	brightness = 0xEA;	//brightness setup

	Chip_I2C_MasterSend(I2C0, 0x70, &begin , 1);	//system setup
	Chip_I2C_MasterSend(I2C0, 0x70, &display, 1);	//display setup
	Chip_I2C_MasterSend(I2C0, 0x70, &brightness, 1);//brightness setup
	clearBoard();
	reDraw();

	//INTERRUPT SETUP
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 10);
	Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 10);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 27);
	Chip_GPIOINT_SetIntRising(LPC_GPIOINT, GPIOINT_PORT2, 1 << 27);
	//Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 26);
	//Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 26);
	/*joystick is actually just five separate gpios, initialized here
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 23);
	Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 23);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 25);
	Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 25);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 26);
	Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 26);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 27);
	Chip_GPIOINT_SetIntRising(LPC_GPIOINT, GPIOINT_PORT2, 1 << 27);
	*/

	//Timer Debounce (to avoid coupling);
	Chip_TIMER_Init(LPC_TIMER0);						// Initialize TIMER0
	Chip_TIMER_PrescaleSet(LPC_TIMER0,120000);			// Set prescale value
	Chip_TIMER_SetMatch(LPC_TIMER0,0,100);				// Set match value
	Chip_TIMER_MatchEnableInt(LPC_TIMER0, 0);			// Configure to trigger interrupt on match

	NVIC_ClearPendingIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER0_IRQn);

	NVIC_ClearPendingIRQ(GPIO_IRQn);
	NVIC_EnableIRQ(GPIO_IRQn);

	int gameOver;
	while(1){
		__WFI();
		if(drawFlag){
			drawFlag = false;
			reDraw();
		}
		if(moveFlag){
			moveFlag = false;
			move_drop();
		}
		if(dropFlag){
			dropFlag = false;
			make_move(dropPosition);
		}
	}
	Chip_I2C_DeInit(I2C0);
	return 0;
}
