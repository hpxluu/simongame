#include <msp430.h>
#include <stdint.h>
#include "lcd.h"

unsigned int note=851;
unsigned char unlock;
unsigned long seed;
unsigned int LSFRarr[32];
unsigned int blink;
unsigned int j = 0;
unsigned int incrementCounter = 0;
int unsigned wintracker;
int unsigned tapCounter;
int unsigned TOTALWINS;
unsigned int button;
int unsigned start;
enum states{ START, LSFR, GAME, SEQUENCE, WAIT, GUESSSEQUENCE, WIN, LOSE }; // the states
enum states state = START;
int unsigned flagStart = 0;
unsigned char flag;
unsigned int letgo;
int unsigned delayStart;
unsigned int winSong[32] = {2272.727273, 1911.095822, 3405.298645, 1702.649322, 1702.649322, 1516.852228, 1431.721215, 1431.721215,
                            1431.721215, 1275.510204, 1516.852228, 1516.852228, 1702.649322, 1911.095822, 1911.095822, 1702.649322,
                            2272.727273, 1911.095822, 1702.649322, 1702.649322, 1702.649322, 1431.721215, 1275.510204, 1275.510204,
                            1275.510204, 1136.363636, 1072.593101, 1072.593101, 1136.363636, 1275.510204, 1136.363636, 1702.649322};
unsigned long winSongTimes[32] = {4096, 4096, 8192, 8192, 4096, 4096, 8192, 8192, 4096, 4096, 8192,
                                  8192, 4096, 4096, 4096, 16384, 4096, 4096, 8192, 8192, 4096, 4096,
                                 8192, 8192, 4096, 4096, 8192, 8192, 4096, 4096, 4096, 16384};

void msp_init() {
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    P2DIR   &= ~(BIT1|BIT2|BIT3|BIT4);    // Set pin P2.2, P2.3 to be an input; up / down buttons
    P2REN   |=  (BIT1|BIT2|BIT3|BIT4);    // Enable internal pullup/pulldown resistor on P2.2, P2.3
    P2OUT   |=  (BIT1|BIT2|BIT3|BIT4);    // Pullup selected on P2.2, P2.3

    P1DIR   |=  BIT5;           // Set pin 1.5 to be an output; Buzzer
    P1SEL1  |= BIT5;            // connect buzzer to
    P1SEL0  |= BIT5;            // TA0.0 which can be driven by outmode from  TA0CCR0
    P2DIR |=  (BIT6 | BIT7);
    P3DIR |=  (BIT6 | BIT7);

    PM5CTL0 &= ~LOCKLPM5;       // Unlock ports from power manager

    P2IES   |=  (BIT1|BIT2|BIT3|BIT4);    // Make P2.0 interrupt happen on the falling edge
    P2IFG   &= ~(BIT1|BIT2|BIT3|BIT4);    // Clear the P2.0 interrupt flag
    P2IE    |=  (BIT1|BIT2|BIT3|BIT4);    // Enable P2.0 interrupt

    TA0CTL = MC_0 | ID_0 | TASSEL_2 | TACLR; // Timer sound the note
                                // Setup timer A in Hold mode
                                // SMCLK = 1MHz
    TA0CCTL0 = OUTMOD_4;        // Setup CCR 0 in outmode toggle
    TA0CCR0 = note;             // Set timer to "note" frequency

    TA2CTL = MC_2 | ID_0 | TASSEL_1 | TACLR;
    TA1CTL = MC_1 | ID_0 | TASSEL_1 | TACLR;
    TB0CTL = MC_1 | ID_0 | TASSEL_1 | TBCLR;
    __enable_interrupt();       // Set global interrupt enable bit in SR register
    P2OUT &= ~(BIT6|BIT7);
    P3OUT &= ~(BIT6|BIT7);
}

void LSFRarray(void) {
    unsigned int i = 0;
    uint16_t LFSR = seed;
    while (i<32) {
        LFSR = update_LFSR(LFSR);
        LFSR = update_LFSR(LFSR);
        LSFRarr[i++] = LFSR & (BIT0|BIT1);
    }
}


void delay(unsigned int delay_cycles)
{
    TA1CCR0 = delay_cycles; // Set the count limit
    TA1CCTL0 |= CCIE; // Enable interrupt
    __bis_SR_register(LPM2_bits + GIE); // Enter LPM2 mode and enable interrupts
}


void winTune(void) {
    unsigned int i = 0;
    while(i<32) {
        if (flag == 0) {
            TB0CCR0 = 16384; flag = 0;
        }
        else {
            TB0CCR0 = 8192; flag = 1;
        }
        TB0CCTL0 |= CCIE; TA0CCR0 = winSong[i]; TA0CTL |= MC_1;
        delay(winSongTimes[i]); TA0CTL &= ~MC_1; i++;
    }
    TB0CCTL0 &= ~CCIE; P2OUT &= ~(BIT6 | BIT7); P3OUT &= ~(BIT6 | BIT7);
}

void turnOnLED(int LSFR) {
    switch (LSFR) {
        case 0: P2OUT |= BIT6; TA0CCR0 = 2024; TA0CTL |= MC_1; delay(16384); break;
        case 1: P2OUT |= BIT7; TA0CCR0 = 1203; TA0CTL |= MC_1; delay(16384); break;
        case 2: P3OUT |= BIT6; TA0CCR0 = 1607; TA0CTL |= MC_1; delay(16384); break;
        case 3: P3OUT |= BIT7; TA0CCR0 = 2407; TA0CTL |= MC_1; delay(16384); break;
    }
    P2OUT &= ~(BIT6 | BIT7); P3OUT &= ~(BIT6 | BIT7); TA0CTL &= ~MC_1; delay(16384);
}


int main(void) {
    volatile unsigned int i;
    msp_init(); lcd_init(); lcd_clear();
    while (1) {
        switch(state) {
            case START:
                P2IE &= ~(BIT1|BIT2|BIT3|BIT4);
                if (!(P2IN & BIT1) | !(P2IN & BIT2) | !(P2IN & BIT3) | !(P2IN & BIT4))
                    flagStart = 1;
                else if (flagStart == 1) {
                    P2IE |= BIT1|BIT2|BIT3|BIT4;
                    state = LSFR;
                }
                else
                    seed++;
                break;
            case LSFR:
                lcd_clear(); displayNum(0); LSFRarray();
                state = SEQUENCE;
                break;

            case SEQUENCE:
                delay(32768); blink = 0;
                __disable_interrupt();
                while (blink <= j) {
                    turnOnLED(LSFRarr[blink++]);
                }
                wintracker = 0; incrementCounter = 0;
                state = GUESSSEQUENCE;
                __enable_interrupt();
                break;
            case GUESSSEQUENCE:
                if (blink == wintracker) {
                    j++; displayNum(++TOTALWINS); state = SEQUENCE;
                }
                if (wintracker == 5) {
                    displayNum(wintracker); state = WIN; TOTALWINS = 0;
                }
                break;
            case WIN:
                delay(32768);
                __disable_interrupt();
                winTune(); j = 0; flagStart = 0; state = START;
                __enable_interrupt();
                break;
            case LOSE:
                delay(16384/2);
                __disable_interrupt();
                P2OUT |= BIT6; P2OUT |= BIT7; P3OUT |= BIT6; P3OUT |= BIT7;
                unsigned int k = 0;
                TA0CCR0 = 6097; TA0CTL |= MC_1;
                while (k<3) {
                    delay(16384); k++;
                }
                __enable_interrupt();
                TA0CTL &= ~MC_1;
                P2OUT &= ~(BIT6 | BIT7); P3OUT &= ~(BIT6 | BIT7);
                j = 0; k = 0; TOTALWINS = 0; flagStart = 0; state = START;
                break;
                }
    }
}

#pragma vector = PORT2_VECTOR
__interrupt void p2_ISR() {
    P2IFG &= ~(BIT1|BIT2|BIT3|BIT4);
    P2IE &= ~(BIT1|BIT2|BIT3|BIT4);
    TA2CCR0  = TA2R+500;
    TA2CCTL0 |= CCIE;
}

#pragma vector= TIMER2_A0_VECTOR
__interrupt void TIMER2_A0_ISR(void) {
    P2IE |= BIT1|BIT2|BIT3|BIT4;
    if (flagStart == 1) {
        if (!(P2IN & BIT1) | !(P2IN & BIT2) | !(P2IN & BIT3) | !(P2IN & BIT4)) {
            P2IES &= ~(BIT1|BIT2|BIT3|BIT4);
            button = (~(P2IN >> 1)) & 0x0F;
            int binary_num = 0, i = 1;
            while (button > 0) {
                binary_num += (button & 1) * i; button >>= 1; i *= 10;
            }
            int pos = 0;
            while (binary_num > 0) {
                if (binary_num & 1) {
                    button = pos; break;
                }
                binary_num >>= 1; pos++;
            }

            switch(button) {
                case 0:
                    P2OUT |= BIT6; TA0CCR0 = 2024; TA0CTL |= MC_1; break;
                case 1:
                    P2OUT |= BIT7; TA0CCR0 = 1203; TA0CTL |= MC_1; break;
                case 2:
                    P3OUT |= BIT6; TA0CCR0 = 1607; TA0CTL |= MC_1; break;
                case 3:
                    P3OUT |= BIT7; TA0CCR0 = 2407; TA0CTL |= MC_1; break;
            }
        }
        else {
            P2IES |= BIT1 | BIT2 | BIT3 |BIT4;
            TA0CTL &= ~MC_1; P2OUT &= ~(BIT6|BIT7); P3OUT &= ~(BIT6|BIT7); letgo = 1;
        }
        if (letgo == 1) {
            if (button == LSFRarr[incrementCounter]) {
                wintracker++;
                incrementCounter++;
            }
            else if (button != LSFRarr[incrementCounter]) {
                state = LOSE;
            }
            letgo = 0;
        }
        TA2CCTL0 &= ~CCIE;
        P2IFG = 0;
    }
}

#pragma vector= TIMER1_A0_VECTOR
__interrupt void TIMER1_A0_ISR(void) {
    __bic_SR_register_on_exit(LPM2_bits);
}


#pragma vector= TIMER0_B0_VECTOR
__interrupt void TIMER0_B0_ISR(void) {
    P2OUT ^= (BIT6 | BIT7);
    P3OUT ^= (BIT6 | BIT7);
}
