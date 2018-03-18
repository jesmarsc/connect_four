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
/**
 * @brief	SysTick Interrupt Handler
 * @return	Nothing
 * @note	Systick interrupt handler updates the button status
 */

void writeDisplay(void){
	Chip_I2C_MasterSend(I2C0, 0x70, 0x00, 1);
	int i;
	for( i = 0; i < 8; i++){
		fullBuff[2*i] = redbuffer[i] & 0xFF;
		fullBuff[2*i + 1] = greenbuffer[i] & 0xFF;
	}
	Chip_I2C_MasterSend(I2C0, 0x70, &fullBuff, 16);
}
void SysTick_Handler(void)
{
}

/**
 * @brief	I2C Interrupt Handler
 * @return	None
 */
void I2C1_IRQHandler(void)
{
	i2c_state_handling(I2C1);
}

/**
 * @brief	I2C0 Interrupt handler
 * @return	None
 */
void I2C0_IRQHandler(void)
{
	i2c_state_handling(I2C0);
}

void GPIO_IRQHandler(void){
	uint32_t val;
	val = Chip_GPIO_ReadValue(LPC_GPIO, 2);
	if(val == drop){
		if(fDebouncing){
			printf("drop button\n");
			fDebouncing = false;
			//printf("value is %d\n" , val);
		}
		Chip_TIMER_Enable(LPC_TIMER0);  // Start TIMER0
	}
	else if(val == right){
		if(fDebouncing){
			printf("right on joystick\n");
			boardData[dropPosition][7] = 0;
			dropPosition -= 1;
			dropPosition = dropPosition % 8;
			if(dropPosition < 0){
				dropPosition = dropPosition * -1;
			}
			if(redTurn == true){
				boardData[dropPosition][7] = 2;
			}
			else{
				boardData[dropPosition][7] = 1;
			}
			reDraw();
			fDebouncing = false;
			//printf("value is %d\n" , val);
		}
		Chip_TIMER_Enable(LPC_TIMER0);  // Start TIMER0
	}
	//printf("%d\n", val);
	Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT, GPIOINT_PORT2, 1 << 10);  // Clear interrupt flag on all of port 2
	//Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT, GPIOINT_PORT2, 1 << 22);
	//Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT, GPIOINT_PORT2, 1 << 23);
	//Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT, GPIOINT_PORT2, 1 << 25);
	Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT, GPIOINT_PORT2, 1 << 27);
}

void TIMER0_IRQHandler(void){
	fDebouncing = true; 				  // Update boolean variable
	Chip_TIMER_Disable(LPC_TIMER0);		  // Stop TIMER0
	Chip_TIMER_Reset(LPC_TIMER0);		  // Reset TIMER0
	Chip_TIMER_ClearMatch(LPC_TIMER0,0);  // Clear TIMER0 interrupt
}

void reDraw(void){
	int a;
	int b;
	for(a = 0; a < 8; a++){
		for(b = 0; b < 7; b++){
			uint8_t color;
			color = boardData[a][b];
			drawPixel(a,b, color);
		}
	}
	writeDisplay();
}

void clearBoard(void){
	int i;
	int j;
	for(i = 0; i < 8; i++){
		for(j = 0; j <7; j++){
			drawPixel(i, j, 0);
		}
	}
	writeDisplay();
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
void drawPixel(int16_t x, int16_t y, uint16_t color) {
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


/**
 * @brief	Main program body
 * @return	int
 */
int main(void)
{
	int tmp;
	int xflag = 0;
	fDebouncing = true;
	Board_Init();
	SystemCoreClockUpdate();
	//Interrupt stuff
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 10);
	Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 10);
	//joystick is actually just five separate gpios, initialized here
	/*Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 23);
	Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 23);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 25);
	Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 25);*/
	//Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 26);
	//Chip_GPIOINT_SetIntFalling(LPC_GPIOINT, GPIOINT_PORT2, 1 << 26);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, GPIOINT_PORT2, 27);
	Chip_GPIOINT_SetIntRising(LPC_GPIOINT, GPIOINT_PORT2, 1 << 27);

	NVIC_ClearPendingIRQ(GPIO_IRQn);
	NVIC_EnableIRQ(GPIO_IRQn);

	//timer stuff (to avoid coupling);
	Chip_TIMER_Init(LPC_TIMER0);					   // Initialize TIMER0
	Chip_TIMER_PrescaleSet(LPC_TIMER0,120000);  // Set prescale value
	Chip_TIMER_SetMatch(LPC_TIMER0,0,100);			   // Set match value
	Chip_TIMER_MatchEnableInt(LPC_TIMER0, 0);		   // Configure to trigger interrupt on match

	NVIC_ClearPendingIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER0_IRQn);

	i2c_app_init(I2C0, SPEED_400KHZ);
	i2c_set_mode(I2C0, 0);

	int x;
	int y;
	for(x = 0; x < 8; x++){
		for(y =0; y < 6; y++){
			boardData[x][y] = 3;
		}
	}
	boardData[7][7] = 1;
	x = 0;
	for(x = 0; x < 7; x++){
		boardData[x][6] = 0;
	}
	dropPosition = 7;
	int begin, brightness, data, display, row;
	begin = 0x21;//setup
	display = 0x81;
	row = 0xA1;
	data = 0xFF;
	brightness = 0xEA;

	row = 0x00;

	Chip_I2C_MasterSend(I2C0, 0x70, &begin , 1); //setup
	Chip_I2C_MasterSend(I2C0, 0x70, &display, 1); //set up display
	//Chip_I2C_MasterSend(I2C0, 0x70, &row, 1);
	//Chip_I2C_MasterSend(I2C0, 0x70, &data, 3);
	Chip_I2C_MasterSend(I2C0, 0x70, &brightness, 1);
	clearBoard();
	reDraw();
	//drawPixel(2, -1, 2);
	//writeDisplay();
	//drawPixel(2, 2, 2);
	//writeDisplay();

	//i2c_read_setup(&temp_xfer, (I2C_SLAVE_TEMP_ADDR << 1), 2);
	while(1){
		__WFI();
	}
	Chip_I2C_DeInit(I2C0);
	return 0;
}
