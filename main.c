//*****************************************************************************
//   Ultra-Low Power TV IR Remote Control Transmitter
//
//                          MSP430F21x1
//                       -----------------
//                   /|\|                 |
//                    | |             P2.7|<- Dip Switch 1
//                    --|RST          P2.6|<- Dip Switch 2
//                      |                 |
//        IR LED Out <--|P1.6         P2.5|-> Row 5
//                      |             P2.4|-> Row 4
//              Col 3 ->|P1.3         P2.3|-> Row 3
//              Col 2 ->|P1.2         P2.2|-> Row 2
//              Col 1 ->|P1.1         P2.1|-> Row 1
//              Col 0 ->|P1.0         P2.0|-> Row 0
//
//
//   SMCLK = DCO = 1MHz
//
//*****************************************************************************
// THIS PROGRAM IS PROVIDED "AS IS". TI MAKES NO WARRANTIES OR
// REPRESENTATIONS, EITHER EXPRESS, IMPLIED OR STATUTORY,
// INCLUDING ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
// COMPLETENESS OF RESPONSES, RESULTS AND LACK OF NEGLIGENCE.
// TI DISCLAIMS ANY WARRANTY OF TITLE, QUIET ENJOYMENT, QUIET
// POSSESSION, AND NON-INFRINGEMENT OF ANY THIRD PARTY
// INTELLECTUAL PROPERTY RIGHTS WITH REGARD TO THE PROGRAM OR
// YOUR USE OF THE PROGRAM.
//
// IN NO EVENT SHALL TI BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
// CONSEQUENTIAL OR INDIRECT DAMAGES, HOWEVER CAUSED, ON ANY
// THEORY OF LIABILITY AND WHETHER OR NOT TI HAS BEEN ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGES, ARISING IN ANY WAY OUT
// OF THIS AGREEMENT, THE PROGRAM, OR YOUR USE OF THE PROGRAM.
// EXCLUDED DAMAGES INCLUDE, BUT ARE NOT LIMITED TO, COST OF
// REMOVAL OR REINSTALLATION, COMPUTER TIME, LABOR COSTS, LOSS
// OF GOODWILL, LOSS OF PROFITS, LOSS OF SAVINGS, OR LOSS OF
// USE OR INTERRUPTION OF BUSINESS. IN NO EVENT WILL TI'S
// AGGREGATE LIABILITY UNDER THIS AGREEMENT OR ARISING OUT OF
// YOUR USE OF THE PROGRAM EXCEED FIVE HUNDRED DOLLARS
// (U.S.$500).
//
// Unless otherwise stated, the Program written and copyrighted
// by Texas Instruments is distributed as "freeware".  You may,
// only under TI's copyright in the Program, use and modify the
// Program without any charge or restriction.  You may
// distribute to third parties, provided that you transfer a
// copy of this license to the third party and the third party
// agrees to these terms by its first use of the Program. You
// must reproduce the copyright notice and any other legend of
// ownership on each copy or partial copy, of the Program.
//
// You acknowledge and agree that the Program contains
// copyrighted material, trade secrets and other TI proprietary
// information and is protected by copyright laws,
// international copyright treaties, and trade secret laws, as
// well as other intellectual property laws.  To protect TI's
// rights in the Program, you agree not to decompile, reverse
// engineer, disassemble or otherwise translate any object code
// versions of the Program to a human-readable form.  You agree
// that in no event will you alter, remove or destroy any
// copyright notice included in the Program.  TI reserves all
// rights not specifically granted under this license. Except
// as specifically provided herein, nothing in this agreement
// shall be construed as conferring by implication, estoppel,
// or otherwise, upon you, any license or other right under any
// TI patents, copyrights or trade secrets.
//
// You may not use the Program in non-TI devices.
//*****************************************************************************

#include  "msp430x21x1.h"

#define   NoKey       0x001
#define   NoMatch     0x002
#define   HeldDown    0x001
#define   Toggle      0x002
#define   RETRANSMIT  1
#define   ENDTRANSMIT 0


// Table of pressed key codes
const char KeyTab[24]= {
  0x10,
  0x11,
  0x12,
  0x13,
  0x14,
  0x15,
  0x20,
  0x21,
  0x22,
  0x23,
  0x24,
  0x25,
  0x40,
  0x41,
  0x42,
  0x43,
  0x44,
  0x45,
  0x80,
  0x81,
  0x82,
  0x83,
  0x84,
  0x85
};

// Array for Function look-up Table
const char FuncTab[24] = {
  0x01,                                     // Key 0 - 
  0x02,                                     // Key 1 - 
  0x03,                                     // Key 2 - 
  0x04,                                     // Key 3 - 
  0x05,                                     // Key 4 - 
  0x06,                                     // Key 5 - 
  0x11,
  0x12,
  0x13,
  0x14,
  0x15,
  0x16,
  0x21,
  0x22,
  0x23,
  0x24,
  0x25,
  0x26,
  0x31,
  0x32,
  0x33,
  0x34,
  0x35,
  0x36
};

unsigned int RowMask;
unsigned int KeyHex;
unsigned int KeyVal;
unsigned int KeyPressed;
unsigned int Command;
unsigned int Trans_Flags;
unsigned int Error_Flags;                    // Error Flags
                                             // xxxx xxxx
                                             //        ||
                                             //        ||- No Key being pressed
                                             //        |---- No key match found

// System Routines

void Initialize(void);
void SetForPress(void);
void Debounce(void);
void KeyScan(void);
void KeyLookup(void);
void SetupForRelease(void);
void DetermineRelease(void);
void Transmit(void);
unsigned int TestRetransmit(void);
void DelayToNextTransmit(void);
void OutputHigh(unsigned int);
void OutputLow(unsigned int);

void main(void)
{
  Initialize();                             // Configure modules and control
                                            // registers

  while(1)                                  // Loop forever
  {
    unsigned int transmit;
    transmit = ENDTRANSMIT;                 // Transmit variable keeps track of
                                            // whether the button is being held
                                            // down - start with button not
                                            // pressed
    SetForPress();                          // Prepare for key press
    LPM4;                                   // Enter LPM4
    Debounce();                             // Debounce key press
    KeyScan();                              // Scan keypad
    KeyLookup();                            // Find which key was pressed
    if (Error_Flags == 0) {                 // If key was pressed, continue
      SetupForRelease();                    // Prepare for key release
      do {                                  //
        Transmit();                         // Transmit RC5 Code
        transmit = TestRetransmit();        // Test if button still held down
        DelayToNextTransmit();              // Wait for ~89ms before
                                            // transmitting again
      } while (transmit == RETRANSMIT);     // Keep transmitting while buttun
                                            // held down
    }
  }
}

void Initialize(void)
{
  WDTCTL = WDTPW+WDTHOLD;                   // Stop watchdog timer
  P2DIR = 0x3F;                             // P2.0 - P2.5 outputs, P2.6-P.7 inputs
  P1SEL |= BIT6;                            // P1.6 PWM output
  P1DIR |= BIT6;                            // P1.6 output
  P2OUT = 0;                                // Clear P2 outputs
  DCOCTL = CALDCO_1MHZ;                     // Set DCO for 1MHz using
  BCSCTL1 = CALBC1_1MHZ;                    // calibration registers
  Trans_Flags = 0;                          // Clear trans flags
  _EINT();                                  // Enable interrupts
}

void SetForPress(void)
{
  P1DIR &= ~0x0F;                           // P1.0 - P1.3 inputs
  P1OUT &= ~0x0F;                           // Clear P1.0 - P1.3 outputs, pull downs
  P1REN |= 0x0F;                            // Enable resistors on P1.0-P1.3
  P2OUT |= 0x3F;                            // Enable keypad
  
  P2SEL &= ~(BIT7 + BIT6);
  P2DIR &= ~(BIT7 + BIT6);
  P2REN |= BIT7 + BIT6;
  P2OUT &= ~(BIT7 + BIT6);
  
  P1IES &= ~0x0F;                           // L-to-H interrupts
  P1IFG = 0;                                // Clear any pending flags
  P1IE |= 0x0F;                             // Enable interrupts
  Error_Flags = 0;                          // Clear error flags
  Trans_Flags &= ~HeldDown;
 // Trans_Flags = 0;                          // Clear trans flags
}

void Debounce(void)
{
  TACTL = TASSEL1+TACLR+ID0+ID1;            // SMCLK/8, Clear TA
  TACCTL0 = CCIE;                           // Enable CCR0 interrupt
  TACCR0 = 5000 - 1;                        // Value for typ delay of ~40mS
  TACTL |= MC0;                             // Start TA in up mode
  LPM0;                                     // Sleep during debounce delay
  TACTL = TACLR;                            // Stop and clear TA
  TACCTL0 = 0;                              // Clear CCTL0 register
}

void KeyScan(void)
{
  unsigned int i;                           // Define counter variable

  RowMask = 0x01;                           // Initialize Row Mask
  KeyHex = 0;                               // Clear KeyHex
  P1OUT &= ~0x0F;                           // Clear column bits in P1OUT reg
  for (i=0; i<6; i++) {                     // Loop through all Rows
    P2OUT &= ~0x3F;                         // Stop driving rows
    P1DIR |= 0x0F;                          // Set column pins to output low
    P1OUT &= ~0x0F;                         // To bleed off charge and avoid
                                            // erroneous reads
    P1DIR &= ~0x0F;                         // Set column pins back to input
    P2OUT = RowMask;                        // Drive row
    if (P1IN & 0x0F) {                      // Test if any key pressed
      KeyHex |= i;                          // Set bit for row
      KeyHex |= (P1IN & 0x0F) << 4;         // Set column bit in key hex
    }
    RowMask = RowMask << 1;                 // Next row
  }
  if (KeyHex == 0) {                        // Was any key pressed?
    Error_Flags |= NoKey;                   // If not, set flag
  }
  P2OUT |= 0x3F;                            // Drive rows again
}

void KeyLookup(void)
{
  unsigned int i;                           // Define counter variable

  KeyPressed = 99;                          // Dummy key value
  for (i=0; i<24; i++) {                    // Loop through all keys
    if (KeyTab[i] == KeyHex) {              // Table value = KeyHex?
      KeyPressed = i;                       // If so, that is the key number
    }
  }
  if (KeyPressed == 99) {                   // Was key not found
    Error_Flags |= NoMatch;                 // If not found, set flag
  }
}

void SetupForRelease(void)
{
  P1DIR |= 0x0F;                            // Switch P1.0 - P1.3 to output, high
  P1OUT |= 0x0F;                            // to reduce power consumption
  Trans_Flags |= HeldDown;                  // Button is pressed
}

void DetermineRelease(void)
{
  P1OUT &= ~0x0F;                           // Switch P1.0 - P1.3 to input with pull
  P1DIR &= ~0x0F;                           // down resistor
  if (P1IN & 0x0F) {                        // Any key still pressed?
  	SetupForRelease();                  // If so, setup for release again
  } else {
    Trans_Flags &= ~HeldDown;               // If not, clear flag
  }
}

void Transmit(void)
{
  unsigned int i;                           // Define counter variable

  Command = FuncTab[KeyPressed];            // Get command code for key pressed
  
  //Command = Command + 0x3600;               // Add 2 start bits, 2 upper bits of address
  // DIP Switch:      Address
  // P1.7  P1.6
  //  0     0           24
  //  0     1           25
  //  1     0           28
  //  1     1           27
  //Command += (P2IN && BIT6) << 6;          // Add bit0 of address      
  //Command += ((P2IN & BIT7)&&(P2IN & BIT6)) << 7;  // Add bit1 of address  
  //Command += ((P2IN & BIT7)&& !(P2IN & BIT6)) << 8;  // Add bit2 of address address 
  
  Command +=  0x3000;
  
  
  switch(P2IN & (BIT6+BIT7))
  {
    case 0x00:
    {
      Command += 0x0600;        // address = 24
    }break;
   
  case 0x40:
    {
      Command += 0x0640;          // address = 25
    }break;
    
  case 0x80:
    {
      Command += 0x0700;          // address = 28
    }break;
    
  case 0xC0:
    {
      Command += 0x06C0;        // address = 27
    }break;
  }
  
  if (Trans_Flags & Toggle) {               // Toggle bit?
    Command = Command + 0x0800;             // Add toggle bit
  }
  Command = Command << 2;                   // Shift the transmit packet twice
                                            // to begin with the start bit
  //TACCR1 = 1;                             // CCR1 PWM Duty Cycle ~4%
  TACCR1 = 7;                               // CCR1 PWM Duty Cycle ~26%
  TACCTL0 = CCIE;                           // Enable CCR0 interrupt
  for (i=0; i<14; i++) {                    // Loop through all bits to send
    if (Command & 0x8000) {                 // Test next bit for "1" or "0"
                                            // Output routines called with
                                            // different values to account for
                                            // different software overheads
      // The values for how long to hold low and high take into account the
      // software overhead in between transmitting bits. The software overhead
      // after the second half of the bit is greater, so the values are lower
      OutputLow(780);                       // If "1" send output low first,
                                            // ~780us
      OutputHigh(32);                       // Then output high, count 32
                                            // interrupts
    } else {
      OutputHigh(35);                       // If "0" send output high first,
                                            // count 35 interrupts
      OutputLow(765);                       // Then output low, ~780us
    }
    Command = Command << 1;                 // Take next bit
  }
  TACTL = TACLR;                            // Stop and clear TA
}

unsigned int TestRetransmit(void)
{
  if (P1IFG == 0) {                         // Has P1 interrupt occured?
    return RETRANSMIT;                      // If not, transmit again
  } else {                                  // If so,
    DetermineRelease();                     // Test if button is still pressed
    if (Trans_Flags & HeldDown) {           // If so,
      return RETRANSMIT;                    // Transmit again
    } else {                                // If not, return to main and
      TACCTL0 = 0;                          // Clear CCTL0 register
      TACCTL1 = 0;                          // Clear CCTL1 register
    //  Trans_Flags &= ~Toggle;               // Clear the toggle bit
      Trans_Flags ^= Toggle;                    // Toggle the toggle bit

      return ENDTRANSMIT;
    }
  }
}

void DelayToNextTransmit(void)
{
  TACTL = TASSEL1+TACLR+ID0+ID1;            // SMCLK/8, Clear TA
  TACCR0 = 11125 - 1;                       // ~89ms interrupt
  TACCTL0 = CCIE;                           // Enable CCR0 interrupt
  TACTL |= MC0;                             // Start TA in up mode
  LPM0;                                     // Sleep during delay
  TACTL = TACLR;                            // Stop and clear TA
 // Trans_Flags ^= Toggle;                    // Toggle the toggle bit
}

void OutputHigh(unsigned int val)
{
  unsigned int j;                           // Declare counter variable
  TACCTL1 = OUTMOD_7;                       // CCR1 reset/set
  //TACCR0 = 26 - 1;                        // PWM Period ~ 38kHz
  TACCR0 = 25 - 1;                          // PWM Period ~ 40kHz
  TACTL = TASSEL1+TACLR;                    // SMCLK, Clear TA
  TACTL |= MC0;                             // Start TA in up mode
  for (j=val; j>0; j--) {                   // Count val interrupts
    LPM0;                                   // at 40KHz each interrupt = ~25us
  }                                         // at 38KHz each interrupt = ~26us
}

void OutputLow(unsigned int val)
{
  TACCTL1 = OUTMOD_0;                       // Set output of CC1 to 0
  TACTL = TASSEL1+TACLR;                    // SMCLK, Clear TA
  TACCR0 = val;                             // Delay of val in us
  TACTL |= MC0;                             // Start TA in up mode
  LPM0;                                     // Enter LPM0, wait for CCR0 int.
}

// P1.x Interrupt service routine
#pragma vector=PORT1_VECTOR
__interrupt void P1_ISR(void)
{
  LPM4_EXIT;                                // Exit LPM4 on return
  P1IFG = 0;                                // Clear interrupt flag
  P1IE = 0;                                 // Disable furthur P1 interrupts
}

// CCR0 Interrupt service routine
#pragma vector=TIMERA0_VECTOR
__interrupt void CCR0_ISR(void)
{
  LPM0_EXIT;                                // Exit LPM0 on return
}
