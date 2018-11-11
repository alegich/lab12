// TuningFork.c Lab 12
// Runs on LM4F120/TM4C123
// Use SysTick interrupts to create a squarewave at 440Hz.  
// There is a positive logic switch connected to PA3, PB3, or PE3.
// There is an output on PA2, PB2, or PE2. The output is 
//   connected to headphones through a 1k resistor.
// The volume-limiting resistor can be any value from 680 to 2000 ohms
// The tone is initially off, when the switch goes from
// not touched to touched, the tone toggles on/off.
//                   |---------|               |---------|     
// Switch   ---------|         |---------------|         |------
//
//                    |-| |-| |-| |-| |-| |-| |-|
// Tone     ----------| |-| |-| |-| |-| |-| |-| |---------------
//
// Daniel Valvano, Jonathan Valvano
// January 15, 2016

/* This example accompanies the book
   "Embedded Systems: Introduction to ARM Cortex M Microcontrollers",
   ISBN: 978-1469998749, Jonathan Valvano, copyright (c) 2015

 Copyright 2016 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */


#include "TExaS.h"
#include "..//tm4c123gh6pm.h"


// basic functions defined at end of startup.s
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
void WaitForInterrupt(void);  // low power mode

/** INVOKE calls macro(or function) FUNC for each variadic argument
    FUNC should have 2 arguments - first is lvalue (e.g. register), second is rvalue
		Example: #define SET_FLAG(REGISTER, VAL) REGISTER |= VAL
		So, if you call:
		INVOKE(SET_FLAG, 0x04, REG1, REG2);
		It will expand to:
		REG1 |= 0x11;
		REG2 |= 0x11;
		
		Maximum number of variadic arguments is 3, but it can be easily expanded by adding another
		INVOKE4, INVOKE5 etc
*/
#define INVOKE(FUNC, VAL, ...) GET_MACRO(__VA_ARGS__, INVOKE3, INVOKE2, INVOKE1)(FUNC, VAL, __VA_ARGS__)

#define GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define INVOKE1(FUNC, VAL, REG_1) FUNC(REG_1, VAL)
#define INVOKE2(FUNC, VAL, REG_1, REG_2) INVOKE1(FUNC, VAL, REG_1); INVOKE1(FUNC, VAL, REG_2)
#define INVOKE3(FUNC, VAL, REG_1, REG_2, REG_3) INVOKE2(FUNC, VAL, REG_1, REG_2); INVOKE1(FUNC, VAL, REG_3)


#define SET_FLAG(REGISTER, VAL) REGISTER |= VAL
#define CLEAR_FLAG(REGISTER, VAL) REGISTER &= ~VAL
#define TOGGLE_FLAG(REGISTER, VAL) REGISTER ^= VAL

#define PIN0 0x01
#define PIN1 0x02
#define PIN2 0x04
#define PIN3 0x08
#define PIN4 0x10
#define PIN5 0x20
#define PIN6 0x40
#define PIN7 0x80

// Port Mux Control
#define PMC2 0x00000F00 // pin 2
#define PMC3 0x0000F000 // pin 3

#define PORT_A 0x00000001

void ActivatePortA() {
	unsigned long volatile dummyDelay;
  INVOKE(SET_FLAG, PORT_A, SYSCTL_RCGC2_R); // activate port A
  dummyDelay = SYSCTL_RCGC2_R; // make a dummy delay by reading value from register
}

// input from PA3
void InitializeInput() {
	// Regular function
	INVOKE(CLEAR_FLAG, PMC3, GPIO_PORTA_PCTL_R);
	
	// Clear flag for pin 3 in registers Analog mode, Direction, Alternate function
	INVOKE(CLEAR_FLAG, PIN3, GPIO_PORTA_AMSEL_R, GPIO_PORTA_DIR_R, GPIO_PORTA_AFSEL_R);

	// Set flag for pin 3 in register Digital enable
	INVOKE(SET_FLAG, PIN3, GPIO_PORTA_DEN_R);
}

// output from PA2
void InitializeOutput() {
	// Regular function
	INVOKE(CLEAR_FLAG, PMC2, GPIO_PORTA_PCTL_R);
	
	// Clear flag for pin 2 in registers Analog mode, Alternate function
	INVOKE(CLEAR_FLAG, PIN2, GPIO_PORTA_AMSEL_R, GPIO_PORTA_AFSEL_R);
	
	// Set flag for pin 2 in registers Direction, 8-mA Drive Select, Digital enable
	INVOKE(SET_FLAG, PIN2, GPIO_PORTA_DIR_R, GPIO_PORTA_DR8R_R, GPIO_PORTA_DEN_R);
}

void InitializeSysTick() {
  NVIC_ST_CTRL_R = 0;           // disable SysTick during setup
  NVIC_ST_RELOAD_R = 90908;     // 1.13636 ms
  NVIC_ST_CURRENT_R = 0;        // any write to current clears it
  NVIC_SYS_PRI3_R = NVIC_SYS_PRI3_R&0x00FFFFFF; // priority 0                
  NVIC_ST_CTRL_R = 0x00000007;  // enable with core clock and interrupts
}

// input from PA3, output from PA2, SysTick interrupts
void Sound_Init(void) {
	ActivatePortA();
	InitializeInput();
	InitializeOutput();
	InitializeSysTick();
	EnableInterrupts();
}

// called at 880 Hz
void SysTick_Handler(void) {
	static unsigned int prevPressed = 0;
	static unsigned int enableSound = 0;
	
	unsigned int pressed = GPIO_PORTA_DATA_R & 0x8;
	
	if (pressed && !prevPressed) {
		enableSound = !enableSound;
		
		if (!enableSound) {
			// put zero to PA2 (turn beep off)
			INVOKE(CLEAR_FLAG, PIN2, GPIO_PORTA_DATA_R);
		}
	}

	prevPressed = pressed;
	
	if (enableSound) {
		// toggle PA2
		INVOKE(TOGGLE_FLAG, PIN2, GPIO_PORTA_DATA_R);
	}
}

int main(void) {
	// activate grader and set system clock to 80 MHz
  TExaS_Init(SW_PIN_PA3, HEADPHONE_PIN_PA2,ScopeOn); 
  Sound_Init();         
  EnableInterrupts();   // enable after all initialization are done
  
	while (1){
    // main program is free to perform other tasks
    // do not use WaitForInterrupt() here, it may cause the TExaS to crash
  }
}
