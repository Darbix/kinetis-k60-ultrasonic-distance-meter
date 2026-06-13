
/**
 * IMP - Project
 * Ultrasonic sensor distance measurement
 * FIT VUT, 2022/2023
 * Author: David Kedra, xkedra00
 */

// Header file with all the essential definitions for K60 FITKIT
#include "MK60DZ10.h"

// Macros for bit-level registers manipulation
#define GPIO_PIN_MASK	0x1Fu
#define GPIO_PIN(x)     (((1)<<(x & GPIO_PIN_MASK)))

// PIN setup
#define PIN_A 11	///< PTA segment A PIN
#define PIN_B 9		///< PTA segment B PIN
#define PIN_C 14	///< PTD segment C PIN
#define PIN_D 10	///< PTA segment D PIN
#define PIN_E 6		///< PTA segment E PIN
#define PIN_F 7		///< PTA segment F PIN
#define PIN_G 15	///< PTD segment G PIN

#define PIN_P1 8	///< PTD display position 1 PIN
#define PIN_P2 13	///< PTD display position 2 PIN
#define PIN_P3 12	///< PTD display position 3 PIN
#define PIN_P4 9	///< PTD display position 4 PIN

#define PIN_ECHO 27 ///< PTD sensor echo input
#define PIN_TRIG 29 ///< PTD sensor trigger output

// Particular display segment positions in PORTs
#define SEG_A GPIO_PIN(PIN_A)	///< Display segment A bit position
#define SEG_B GPIO_PIN(PIN_B)	///< Display segment B bit position
#define SEG_C GPIO_PIN(PIN_C)	///< Display segment C bit position
#define SEG_D GPIO_PIN(PIN_D)	///< Display segment D bit position
#define SEG_E GPIO_PIN(PIN_E)	///< Display segment E bit position
#define SEG_F GPIO_PIN(PIN_F)	///< Display segment F bit position
#define SEG_G GPIO_PIN(PIN_G)	///< Display segment G bit position

#define DGT_1 GPIO_PIN(PIN_P1)	///< 1st display bit position
#define DGT_2 GPIO_PIN(PIN_P2)	///< 2nd display bit position
#define DGT_3 GPIO_PIN(PIN_P3)	///< 3rd display bit position
#define DGT_4 GPIO_PIN(PIN_P4)	///< 4th display bit position

// Ultrasonic sensor
#define TRIG GPIO_PIN(PIN_TRIG) ///< Sensor trigger bit position
#define ECHO GPIO_PIN(PIN_ECHO)	///< Sensor echo bit position

// PIT timer initial counter values for a frequency of 50 MHz
#define US10_TO_PIT 0x1F4 		///<     500 PIT cycles ~ 10 us trigger enabled
#define MS30_TO_PIT 0x16E360 	///< 1500000 PIT cycles ~ 30 ms echo receiving
#define MS50_TO_PIT 0x2625A0 	///< 2500000 PIT cycles ~ 50 ms trigger period

uint32_t distance = 0;

/**
 * Delay in bound units
 * @param bound Number of bounds to wait
 */
void delay(long long bound) {
  long long i;
  for(i = 0; i < bound; i++);
}

/**
 * Basic initialization of GPIO features on PORTA and PORTD
 */
void ports_init(void){
	// Turn on clocks for PORTA and PORTD
	SIM->SCGC5 = SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTD_MASK;

	// Set corresponding PORTA pins for GPIO functionality
	PORTA->PCR[PIN_A] = PORT_PCR_MUX(0x01); // A
	PORTA->PCR[PIN_B] = PORT_PCR_MUX(0x01); // B
	PORTA->PCR[PIN_D] = PORT_PCR_MUX(0x01); // D
	PORTA->PCR[PIN_E] = PORT_PCR_MUX(0x01); // E
	PORTA->PCR[PIN_F] = PORT_PCR_MUX(0x01); // F

	// Sensor PINs setup
	PORTA->PCR[PIN_ECHO] = PORT_PCR_ISF_MASK	// Interrupt detecting
			 	 	 	 | PORT_PCR_IRQC(0x0B)	// Interrupt on either edge
						 | PORT_PCR_MUX(0x01)  	// GPIO
						 | PORT_PCR_PE_MASK		// Pull Enable
						 | PORT_PCR_PS_MASK;  	// Pull select
	PORTA->PCR[PIN_TRIG] = PORT_PCR_MUX(0x01);

	// Set PORTA display segments and trigger as output (1 in PDDR)
	PTA->PDDR = GPIO_PDDR_PDD(SEG_A | SEG_B | SEG_D | SEG_E | SEG_F | TRIG);

	/* Set corresponding PORTD pins for GPIO functionality */
	PORTD->PCR[PIN_C] = PORT_PCR_MUX(0x01);  // C
	PORTD->PCR[PIN_G] = PORT_PCR_MUX(0x01);  // G

	PORTD->PCR[PIN_P1] = PORT_PCR_MUX(0x01); // Digit 1
	PORTD->PCR[PIN_P2] = PORT_PCR_MUX(0x01); // Digit 2
	PORTD->PCR[PIN_P3] = PORT_PCR_MUX(0x01); // Digit 3
	PORTD->PCR[PIN_P4] = PORT_PCR_MUX(0x01); // Digit 4

	// Set PORTD display segments and display positions (1 in PDDR)
	PTD->PDDR = GPIO_PDDR_PDD(SEG_C | SEG_G | DGT_1 | DGT_2 | DGT_3 | DGT_4);
	// Turn off digit positions by log. 1
	PTD->PDOR |= GPIO_PDOR_PDO(DGT_1 | DGT_2 | DGT_3 | DGT_4);

	NVIC_ClearPendingIRQ(PORTA_IRQn);
	NVIC_EnableIRQ(PORTA_IRQn);
}

/**
 * Initialize all PIT timers
 */
void PIT_init(void){
	SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;	// Enable the PIT timer
	PIT->MCR &= ~PIT_MCR_MDIS_MASK;		// Clock for PIT Timers is enabled
	// DCO range setup to get PIT frequency close to 50 MHz
	MCG->C4 |= MCG_C4_DRST_DRS(0x01);
	MCG->C4 |= MCG_C4_DMX32_MASK;

	PIT->CHANNEL[0].LDVAL = PIT_LDVAL_TSV(US10_TO_PIT); // Starting counter value
	PIT->CHANNEL[0].TCTRL |= PIT_TCTRL_TIE_MASK;		// TIE enable
	NVIC_ClearPendingIRQ(PIT0_IRQn);
	NVIC_EnableIRQ(PIT0_IRQn);							// enable interrupts from PIT0

	PIT->CHANNEL[1].LDVAL = PIT_LDVAL_TSV(MS30_TO_PIT);	// Starting counter value
	PIT->CHANNEL[1].TCTRL |= PIT_TCTRL_TIE_MASK;		// TIE enable
	NVIC_ClearPendingIRQ(PIT1_IRQn);
	NVIC_EnableIRQ(PIT1_IRQn);							// enable interrupts from PIT1

	PIT->CHANNEL[2].LDVAL = PIT_LDVAL_TSV(MS50_TO_PIT); // Starting counter value
	PIT->CHANNEL[2].TCTRL |= PIT_TCTRL_TIE_MASK;		// TIE enable
	NVIC_ClearPendingIRQ(PIT2_IRQn);
	NVIC_EnableIRQ(PIT2_IRQn);							// enable interrupts from PIT2
}

/**
 * Display a single digit on a display
 * @param digit Digit 0-9
 * @param pos Position on the display 1-4
 */
void display_digit(uint32_t digit, uint32_t pos) {
	// PTA turn off display
	PTA->PDOR &= ~(SEG_A | SEG_B | SEG_D | SEG_E | SEG_F);
	// PTD turn off display
	PTD->PDOR |= DGT_1 | DGT_2 | DGT_3 | DGT_4; // Disabled by setting 1
	PTD->PDOR &= ~(SEG_C | SEG_G);

	// Activate a specific display position by setting 0
	switch(pos){
		case(1): PTD->PDOR &= ~DGT_1; break;
		case(2): PTD->PDOR &= ~DGT_2; break;
		case(3): PTD->PDOR &= ~DGT_3; break;
		case(4): PTD->PDOR &= ~DGT_4; break;
		default:
			break;
	}

	// Activate all segments for a digit
	switch(digit){
		case(0):
			PTA->PDOR |= SEG_A | SEG_B | SEG_D | SEG_E | SEG_F;
			PTD->PDOR |= SEG_C;
			break;
		case(1):
			PTA->PDOR |= SEG_B;
			PTD->PDOR |= SEG_C;
			break;
		case(2):
			PTA->PDOR |= SEG_A | SEG_B | SEG_D | SEG_E;
			PTD->PDOR |= SEG_G;
			break;
		case(3):
			PTA->PDOR |= SEG_A | SEG_B | SEG_D;
			PTD->PDOR |= SEG_C | SEG_G;
			break;
		case(4):
			PTA->PDOR |= SEG_B | SEG_F;
			PTD->PDOR |= SEG_C | SEG_G;
			break;
		case(5):
			PTA->PDOR |= SEG_A | SEG_D | SEG_F;
			PTD->PDOR |= SEG_C | SEG_G;
			break;
		case(6):
			PTA->PDOR |= SEG_A | SEG_D | SEG_E | SEG_F;
			PTD->PDOR |= SEG_C | SEG_G;
			break;
		case(7):
			PTA->PDOR |= SEG_A | SEG_B;
			PTD->PDOR |= SEG_C;
			break;
		case(8):
			PTA->PDOR |= SEG_A | SEG_B | SEG_D | SEG_E | SEG_F;
			PTD->PDOR |= SEG_C | SEG_G;
			break;
		case(9):
			PTA->PDOR |= SEG_A | SEG_B | SEG_D | SEG_F;
			PTD->PDOR |= SEG_C | SEG_G;
			break;
		default:
			break;
	}
}

/**
 * Count digits in a number
 * @param number Unsigned number
 * @return Number of digits
 */
int count_digits(uint32_t number){
	number /= 10;
    if (number == 0)
        return 1;
    return 1 + count_digits(number);
}

/**
 * Update a display number from a global variable 'distance'
 */
void display_loop() {
	while(1){
		int ndigits = count_digits(distance);

		// Current divider to get a digit at each position
		uint32_t div = 1;

		for(int d = 0; d < ndigits; d++){
			// Get each digit from right to left
			uint32_t digit = (distance / div) % 10;

			display_digit(digit, 4-d);
			delay(100);

			// Increase a divider to be able to get a new digit
			div *= 10;
		}
	}
}

/**
 * Configure a PIT0 interrupt handler for sending trigger pulse
 */
void PIT0_IRQHandler(void){
	// Clear the interrupt flag
	PIT->CHANNEL[0].TFLG |= PIT_TFLG_TIF_MASK;
	// Trigger pulse sent, disable the timer
	PIT->CHANNEL[0].TCTRL &= ~PIT_TCTRL_TEN_MASK;
	// Set a trigger value to 0
	PTA->PDOR &= ~GPIO_PDOR_PDO(TRIG);
}

/**
 * Configure a PIT1 interrupt handler for measuring echo wave width
 */
void PIT1_IRQHandler(void){
	// Clear the interrupt flag
	PIT->CHANNEL[1].TFLG |= PIT_TFLG_TIF_MASK;
	// Disable itself
	PIT->CHANNEL[1].TCTRL &= ~PIT_TCTRL_TEN_MASK;
}

/**
 * Configure a PIT2 interrupt handler for periodic trigger pulse PIT0 enable
 */
void PIT2_IRQHandler(void){
	// Clear the interrupt flag
	PIT->CHANNEL[2].TFLG |= PIT_TFLG_TIF_MASK;
	// Set a trigger value to 1
	PTA->PDOR |= GPIO_PDOR_PDO(TRIG);
	// Activate a trigger pulse timer PIT0
	PIT->CHANNEL[0].TCTRL |= PIT_TCTRL_TEN_MASK;
}

void PORTA_IRQHandler() {
	// If an interrupt is on ECHO PIN and the wave is rising
	if(PORTA->ISFR & GPIO_PDIR_PDI(ECHO)){
		if(PTA->PDIR & GPIO_PDIR_PDI(ECHO)){
			// Activate a timer for the echo pulse width measure
			PIT->CHANNEL[1].TCTRL |= PIT_TCTRL_TEN_MASK;
		}
		// If an interrupt is on ECHO PIN and the wave is falling
		else if(!(PTA->PDIR & GPIO_PDIR_PDI(ECHO))){
			// If the echo pulse was not longer than 30 ms (PIT1 is still enabled), get a distance
			if(PIT->CHANNEL[1].TCTRL & PIT_TCTRL_TEN_MASK){
				if((MS30_TO_PIT - PIT->CHANNEL[1].CVAL) / 50000 <= 25){
					/*
					  Calculate a distance in cm from the number of PIT1 cycles
					  s[cm] = t[us] * v[cm/us] / 2 = t[us] * 0.0343 / 2 =
					  s[cm] = (<pit_count> / <count_per_1us>)[us] * 0.017.. =
					  s[cm] = (MS30_TO_PIT - PIT->CHANNEL[1].CVAL) / (MS30_TO_PIT / 30000) / 58)
					*/
					distance = (uint32_t)((MS30_TO_PIT - PIT->CHANNEL[1].CVAL) / 50 / 58);
				}
				else
					distance = 0;
			}
		}
	}
	// Clear the interrupt
	PORTA->ISFR = ~0;
}

/**
 * Main loop
 */
int main(void){
	// Initialize the PORTs
	ports_init();

	// Initialize all PIT timers
	PIT_init();

	// Enable the 50 ms timer for starting a trigger pulse sending
	PIT->CHANNEL[2].TCTRL |= PIT_TCTRL_TEN_MASK;

	// Cyclic display updating to a 'distance' number
	display_loop();

	return 0;
}
