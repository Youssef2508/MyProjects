#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Define Pins for 7-Segment Display Enable Control
#define ENABLE_PORT PORTA
#define ENABLE_DDR  DDRA

// Define Pins for Push Buttons and LEDs
#define BUTTON_PIN     PIND
#define BUTTON_PORT    PORTD
#define BUTTON_DDR     DDRD
#define BUTTON_PINB    PINB
#define BUTTON_PORTB   PORTB
#define BUTTON_DDRB    DDRB

#define LED_PORT   PORTD
#define LED_DDR    DDRD
#define RED_LED    PD4
#define YELLOW_LED PD5

// Define Buzzer Pin
#define BUZZER_PORT PORTD
#define BUZZER_DDR  DDRD
#define BUZZER_PIN  PD0

// Define Pins for 7-Segment Display Data (assuming PORTC is used)
#define SEGMENT_PORT PORTC
#define SEGMENT_DDR  DDRC

// Function Prototypes
void initGPIO(void);
void initTimer1(void);
void initExternalInterrupts(void);
void updateDisplayDigits(void);
void handleButtonPresses(void);
void handleTimeUpdate(void);
void multiplexDisplays(void);

// Global Variables
volatile uint8_t stopwatch_mode = 0;   // 0: Increment Mode, 1: Countdown Mode
volatile uint8_t current_display = 0;  // Current display being refreshed
volatile uint8_t display_digits[6] = {0, 0, 0, 0, 0, 0};  // Digits to display (HH:MM:SS)
volatile uint8_t seconds = 0;
volatile uint8_t minutes = 0;
volatile uint8_t hours = 0;
volatile uint8_t paused = 0;  // 0: Running, 1: Paused
volatile uint8_t countdown_started = 0;  // Flag to track countdown mode
volatile uint8_t timer1_interrupt_flag = 0;  // Flag to indicate Timer1 interrupt
volatile uint8_t button_toggle_pressed = 0;  // Flag to track button state
volatile uint8_t buzzer_triggered = 0;   // flag to track the buzzer state
volatile uint8_t buzzer_time = 0;  // Variable to know how long the buzzer is triggered
volatile uint8_t hours_inc_pressed = 0;   // Flag to to track the hours increment button state
volatile uint8_t hours_dec_pressed = 0;   // Flag to to track the hours decrement button state
volatile uint8_t minutes_inc_pressed = 0; // Flag to to track the minutes increment button state
volatile uint8_t minutes_dec_pressed = 0; // Flag to to track the minutes decrement button state
volatile uint8_t seconds_inc_pressed = 0; // Flag to to track the seconds increment button state
volatile uint8_t seconds_dec_pressed = 0; // Flag to to track the seconds decrement button state



// Main Function
int main(void) {
    // Initialize GPIO, Timer, and Interrupts
    initGPIO();
    initTimer1();
    initExternalInterrupts();


    sei(); // Enable Global Interrupts

    while (1) {
        handleButtonPresses();  // Handle button inputs
        handleTimeUpdate();     // Update time (increment or countdown)
        multiplexDisplays();    // Update the 7-segment displays
    }
}

void initGPIO(void) {
    // Configure Enable Pins for 7-Segment Displays as Output
    ENABLE_DDR = 0x3F;   // First 6 pins of PORTA for enable control of 7-segment displays
    ENABLE_PORT = 0x00;  // All displays initially disabled

    // Configure Button Pins as Input
    BUTTON_DDR &= ~(1 << PD2) &  // Reset Button (INT0)
                   ~(1 << PD3);  // Pause Button (INT1)
	BUTTON_DDRB &=  ~(1 << PB2) &  // Resume Button (INT2)
					~(1 << PB7) &  // Count Mode Button
					~(1 << PB1) &  // Hours Increment
					~(1 << PB0) &  // Hours Decrement
					~(1 << PB4) &  // Minutes Increment
					~(1 << PB3) &  // Minutes Decrement
					~(1 << PB6) &  // Seconds Increment
					~(1 << PB5);  // Seconds Decrement

    // Enable internal pull-up resistors for buttons that don't use external pull-ups
    BUTTON_PORT |= (1 << PD2);  // Reset Button (INT0)
    BUTTON_PORTB |= (1 << PB2) |  // Resume Button (INT2)
    				(1 << PB7) |  // Count Mode Button
					(1 << PB1) |  // Hours Increment
					(1 << PB0) |  // Hours Decrement
					(1 << PB4) |  // Minutes Increment
					(1 << PB3) |  // Minutes Decrement
					(1 << PB6) |  // Seconds Increment
					(1 << PB5);   // Seconds Decrement

    // Do not enable internal pull-up for PD3 (Pause Button) since it uses an external pull-up
    BUTTON_PORT &= ~(1 << PD3);  // Make sure internal pull-up is disabled for PD3

    // Configure LED Pins as Output
    LED_DDR |= (1 << RED_LED) | (1 << YELLOW_LED); // Red and Yellow LEDs as output
    LED_PORT &= ~((1 << RED_LED) | (1 << YELLOW_LED)); // LEDs off initially

    // Configure Buzzer Pin as Output
    BUZZER_DDR |= (1 << BUZZER_PIN);
    BUZZER_PORT &= ~(1 << BUZZER_PIN); // Buzzer off initially

    // Configure 7-Segment Display Data Pins as Output
    SEGMENT_DDR = 0xFF; // Assuming PORTC is used for segment data
}


// Initialize Timer1 for CTC Mode
void initTimer1(void) {
    TCCR1A |= (1<<FOC1A);   			 // Non PWM mode FOC1A = 1
    TCCR1B |= (1 << WGM12); 			 // CTC Mode WGM12=1
    TCCR1B |= (1 << CS12) | (1 << CS10); //clock = F_CPU/1024 CS10 = 1 CS11 = 0 CS12 = 1
    TIMSK |= (1 << OCIE1A); 			 // Enable Timer1 compare interrupt
    OCR1A = 15624;          			 // Set CTC compare value for 1Hz at 16MHz AVR clock with a prescaler of 1024
}



// Initialize External Interrupts for Buttons
void initExternalInterrupts(void) {
    // Configure INT0 (PD2) for Reset Button (falling edge)
    MCUCR |= (1 << ISC01);  // Falling edge on INT0 triggers interrupt
    GICR |= (1 << INT0);    // Enable INT0 interrupt

    // Configure INT1 (PD3) for Pause Button (Raising edge, using external pull-up)
    MCUCR |= (1 << ISC11) | (1 << ISC10);  // Raising edge trigger
    GICR |= (1 << INT1);    // Enable INT1 interrupt

    // Configure INT2 (PB2) for Resume Button (falling edge)
    MCUCSR &= ~(1 << ISC2); // Falling edge on INT2 triggers interrupt
    GICR |= (1 << INT2);    // Enable INT2 interrupt
}

// Function to handle button presses with rollover behavior
void handleButtonPresses(void) {
    // Toggle stopwatch mode (PB7)
	if ((!(PINB & (1 << PB7))) && button_toggle_pressed == 0) {
	    button_toggle_pressed = 1;  // Mark the button as pressed
	    stopwatch_mode = !stopwatch_mode;  // Toggle the mode
	} else if ((PINB & (1 << PB7)) && button_toggle_pressed == 1) {
	    button_toggle_pressed = 0;  // Reset the button press state
	}

    // Handle Hours Increment (PB1)
    if (!(BUTTON_PINB & (1 << PB1)) && hours_inc_pressed == 0) {
        hours_inc_pressed = 1;  // Mark the button as pressed
        hours = (hours + 1) % 24;  // Increment and wrap around at 24
        updateDisplayDigits();
    } else if ((BUTTON_PINB & (1 << PB1)) && hours_inc_pressed == 1) {
        hours_inc_pressed = 0;  // Reset the button press state
    }

    // Handle Hours Decrement (PB0)
    if (!(BUTTON_PINB & (1 << PB0)) && hours_dec_pressed == 0) {
        hours_dec_pressed = 1;  // Mark the button as pressed
        hours = (hours > 0) ? (hours - 1) : 23;  // Decrement and wrap around at 0
        updateDisplayDigits();
    } else if ((BUTTON_PINB & (1 << PB0)) && hours_dec_pressed == 1) {
        hours_dec_pressed = 0;  // Reset the button press state
    }

    // Handle Minutes Increment (PB4)
    if (!(BUTTON_PINB & (1 << PB4)) && minutes_inc_pressed == 0) {
        minutes_inc_pressed = 1;  // Mark the button as pressed
        minutes++;
        if (minutes >= 60) {  // Rollover minutes and increment hours
            minutes = 0;
            hours = (hours + 1) % 24;  // Wrap around at 24 hours
        }
        updateDisplayDigits();
    } else if ((BUTTON_PINB & (1 << PB4)) && minutes_inc_pressed == 1) {
        minutes_inc_pressed = 0;  // Reset the button press state
    }

    // Handle Minutes Decrement (PB3)
    if (!(BUTTON_PINB & (1 << PB3)) && minutes_dec_pressed == 0) {
        minutes_dec_pressed = 1;  // Mark the button as pressed
        if (minutes > 0) {
            minutes--;  // Decrement normally
        } else {  // Rollover minutes and decrement hours
            minutes = 59;
            hours = (hours > 0) ? (hours - 1) : 23;  // Wrap around at 0 hours
        }
        updateDisplayDigits();
    } else if ((BUTTON_PINB & (1 << PB3)) && minutes_dec_pressed == 1) {
        minutes_dec_pressed = 0;  // Reset the button press state
    }

    // Handle Seconds Increment (PB6)
    if (!(BUTTON_PINB & (1 << PB6)) && seconds_inc_pressed == 0) {
        seconds_inc_pressed = 1;  // Mark the button as pressed
        seconds++;
        if (seconds >= 60) {  // Rollover seconds and increment minutes
            seconds = 0;
            minutes++;
            if (minutes >= 60) {  // Rollover minutes and increment hours
                minutes = 0;
                hours = (hours + 1) % 24;
            }
        }
        updateDisplayDigits();
    } else if ((BUTTON_PINB & (1 << PB6)) && seconds_inc_pressed == 1) {
        seconds_inc_pressed = 0;  // Reset the button press state
    }

    // Handle Seconds Decrement (PB5)
    if (!(BUTTON_PINB & (1 << PB5)) && seconds_dec_pressed == 0) {
        seconds_dec_pressed = 1;  // Mark the button as presse && pausedd
        if (seconds > 0) {
            seconds--;  // Decrement normally
        } else {  // Rollover seconds and decrement minutes
            seconds = 59;
            if (minutes > 0) {
                minutes--;
            } else {  // Rollover minutes and decrement hours
                minutes = 59;
                hours = (hours > 0) ? (hours - 1) : 23;
            }
        }
        updateDisplayDigits();
    } else if ((BUTTON_PINB & (1 << PB5)) && seconds_dec_pressed == 1) {
        seconds_dec_pressed = 0;  // Reset the button press state
    }
}

// Function to handle time updates based on mode (increment or countdown)
void handleTimeUpdate(void) {
	if(buzzer_time == 5){ // Check if the buzzer triggered for 2 seconds
		BUZZER_PORT &= ~(1 << BUZZER_PIN);  // Turn off the buzzer
		stopwatch_mode = 0; // Back to the default (increment mode)
		buzzer_triggered = 0; // Clear the flag
		buzzer_time = 0;
		paused = 0;
	}
    if (timer1_interrupt_flag) {
        timer1_interrupt_flag = 0;

        if (!paused) {
            if (stopwatch_mode == 0) {           // Increment Mode
            	LED_PORT |= (1 << RED_LED);      // Enable the RED LED
            	LED_PORT &= ~(1 << YELLOW_LED);  // Disable the YELLOW LED
                seconds++;
                if (seconds >= 60) {
                    seconds = 0;
                    minutes++;
                    if (minutes >= 60) {
                        minutes = 0;
                        hours = (hours + 1) % 24;
                    }
                }
            } else {  // Countdown Mode
            	LED_PORT |= (1 << YELLOW_LED); // Enable the YELLOW LED
            	LED_PORT &= ~(1 << RED_LED);   // Disable the RED LED
                if (seconds > 0 || minutes > 0 || hours > 0) {
                    if (seconds == 0) {
                        if (minutes > 0) {
                            minutes--;
                            seconds = 59;
                        } else if (hours > 0) {
                            hours--;
                            minutes = 59;
                            seconds = 59;
                        }
                    } else {
                        seconds--;
                    }
                } else {
                    paused = 1;
                    BUZZER_PORT |= (1 << BUZZER_PIN);  // Trigger the buzzer
                    buzzer_triggered = 1;
                }
            }
            updateDisplayDigits();
        }
        else{
        	if(stopwatch_mode == 0){
            	LED_PORT |= (1 << RED_LED);      // Enable the RED LED
            	LED_PORT &= ~(1 << YELLOW_LED);  // Disable the YELLOW LED
        	}
        	else{
            	LED_PORT |= (1 << YELLOW_LED); // Enable the YELLOW LED
            	LED_PORT &= ~(1 << RED_LED);   // Disable the RED LED
        	}
        }
    }
}



// Function to multiplex the 7-segment displays
void multiplexDisplays(void) {
    ENABLE_PORT = 0x00;  // Turn off all displays
    SEGMENT_PORT = (SEGMENT_PORT & 0xF0) | (display_digits[current_display] & 0x0F);
    ENABLE_PORT |= (1 << current_display);  // Enable the corresponding display

    current_display = (current_display + 1) % 6;  // Increment to next display

    _delay_ms(2);  // Small delay for multiplexing
}


// ISR for Timer1 Compare Match A
ISR(TIMER1_COMPA_vect) {
    timer1_interrupt_flag = 1;  // Set flag to indicate Timer1 interrupt
    if(buzzer_triggered){
    	buzzer_time++;
    }
}

// Update digits to be displayed on each 7-segment display
void updateDisplayDigits(void) {
    display_digits[0] = hours / 10;   // Tens place of hours
    display_digits[1] = hours % 10;   // Units place of hours
    display_digits[2] = minutes / 10; // Tens place of minutes
    display_digits[3] = minutes % 10; // Units place of minutes
    display_digits[4] = seconds / 10; // Tens place of seconds
    display_digits[5] = seconds % 10; // Units place of seconds
}

// ISR for INT0 - Reset Button
ISR(INT0_vect) {
    // Reset Stopwatch
    hours = 0;
    minutes = 0;
    seconds = 0;
    stopwatch_mode = 0; // Back to the default (increment mode)
    paused = 0;  // Unpause the stopwatch
    updateDisplayDigits();
}

// ISR for INT1 - Pause Button (connected to PD3 with an external pull-up resistor)
ISR(INT1_vect) {
    paused = 1;  // Set paused flag, but do not stop the timer
}

// ISR for INT2 - Resume Button
ISR(INT2_vect) {
    if (paused) {  // Only resume if paused
        paused = 0;  // Unset paused flag
    }
}



