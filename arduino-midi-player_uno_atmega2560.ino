/*
 *  Arduino MIDI Player
 *
 *  Setup Arduino and use timer2 to synthesize and output sine wave
 *
 *  2016 by ilufang
 *  2018 by jbschueler
 *       optimized
 *       atmega2560 support (pwm pin 10)
 */

/*
 * Part of this file contains code modified/referenced from
 * http://interface.khm.de/index.php/lab/interfaces-advanced/arduino-dds-sinewave-generator/
 *
 * DDS Sine Generator mit ATMEGS 168
 * Timer2 generates the  31250 KHz Clock Interrupt
 *
 * KHM 2009 /  Martin Nawrath
 * Kunsthochschule fuer Medien Koeln
 * Academy of Media Arts Cologne
 */

#include "avr/pgmspace.h"

#include "midi2wave.h"

typedef union phaccu_u
{
  uint16_t ulong;
  uint8_t  ubyte[2];
} phaccu_u;

// variables in interrupt service
volatile uint8_t timer_micro = 0; // timing counter in microseconds
volatile uint16_t timer_milli = 0; // timing counter in milliseconds

volatile phaccu_u phaccu[KEYBUF_SIZE]; // phase accumulator

volatile uint32_t tword_m[KEYBUF_SIZE]; // DDS tuning word m

void setup()
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  DDRB  |= 1<<4;// P10 (PB4) OUTPUT for ATMEGA2560
#else
  DDRB  |= 1<<3;// P11 (PB3) OUTPUT for UNO
#endif
  setupMidi();
  setupTimer2();
}

void loop()
{
  if ( timer_milli == 0 )  // wait for the next midi event
  {
    TIMSK2 &= ~(1<<TOIE2);
    loadNextEvent();
    // calculate new DDS tuning word

    for (uint8_t i=0; i<KEYBUF_SIZE; i++ )
    {
      tword_m[i]=PIANOINC(active_keys[i]);
      if (!tword_m[i]) phaccu[i].ulong = 0;
    }
    timer_milli = event_length;
    TIMSK2 |= (1<<TOIE2);
  }
}

/*
 * timer2 setup
 *
 * set pre-scaler to 1, PWM mode to phase correct PWM,  16000000/510 = 31372.55 Hz clock
 */
void setupTimer2()
{
  // Timer2 Clock Pre-scaler to : 1
  TCCR2B &= ~((1<<CS22) | (1<<CS21) | (1<<CS20)); // clear bits
  TCCR2B |=   (0<<CS22) | (0<<CS21) | (1<<CS20);  // set bits

  // Timer2 PWM Mode set to Phase Correct PWM
  TCCR2A &= ~((1<<COM2A1) | (1<<COM2A0) | (1<<WGM21) | (1<<WGM20)); // clear bits
  TCCR2A |=   (1<<COM2A1) | (0<<COM2A0) | (0<<WGM21) | (1<<WGM20);  // set bits
  TCCR2B &= ~(1<<WGM22); // clear bits
//  TCCR2B |=  (0<<WGM22);  // set bits

  // initialize DDS tuning word
  for ( uint8_t i=0; i<KEYBUF_SIZE; i++ )
  {
    tword_m[i]=0;
  }

  // disable Timer0 interrupts to avoid timing distortion
  TIMSK0 &= ~(1<<TOIE0);

  // start Timer2!
  TIMSK2 |= 1<<TOIE2;
}

/*
 * Timer2 Interrupt Service
 *
 * Running at 31372,550 KHz = 32uSec
 * this is the timebase REFCLOCK for the DDS generator
 * FOUT = (M (REFCLK)) / (2 exp 32)
 * runtime : 8 microseconds ( inclusive push and pop)
 */
ISR(TIMER2_OVF_vect)
{
  uint16_t phaccu_all=0;

  for ( uint8_t i=0; i<KEYBUF_SIZE; i++ )
  {
    phaccu[i].ulong += tword_m[i];
    phaccu_all += sine[phaccu[i].ubyte[1]];
  }

  // Write to PWM
#if KEYBUF_SIZE == 1
  OCR2A = phaccu_all;
#elif KEYBUF_SIZE == 2
  OCR2A = phaccu_all>>1;
#elif KEYBUF_SIZE == 4
  OCR2A = phaccu_all>>2;
#elif KEYBUF_SIZE == 8
  OCR2A = phaccu_all>>3;
#elif KEYBUF_SIZE == 16
  OCR2A = phaccu_all>>4;
#elif KEYBUF_SIZE == 32
  OCR2A = phaccu_all>>5;
#else
  OCR2A = phaccu_all/KEYBUF_SIZE;
#endif

  // Increment timing counter
  if( !(--timer_micro) ) {
    if ( timer_milli > 0 )
    {
      timer_milli--;
    }
    timer_micro=31;
  }
}
