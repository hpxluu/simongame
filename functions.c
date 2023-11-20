#include <msp430.h>
#include <stdint.h>

// Update the LFSR
uint16_t update_LFSR(uint16_t LFSR)
{

  uint16_t new_val;

  new_val  = ((LFSR & BIT0)) ^  //create new bit to be rotated in
             ((LFSR & BIT1) >> 1) ^
             ((LFSR & BIT3) >> 3) ^
             ((LFSR & BIT5) >> 5);

  LFSR = LFSR >> 1;             //shift to perform rotation
  LFSR &= ~(BITF);              //have to clear bit because shift is arithmetic
  LFSR |= (new_val << 15);      //combine with new bit

  return LFSR;
}