#define F_CPU 32000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "stream.h"
#include "clock.h"

// De code is gemaakt door Eric Hoekstra voor de vakken Regeltechniek en Signaalverwerking.
// De code is gebaseerd op het boek van Dhr W. Dolman, waarvoor dank.
// Zie http://dolman-wim.nl/xmega/index.php voor de voorbeelden
// De Code is bestemd voor de ATxmega256a3u

// Dit programma leest d.m.v de ADC de spanning op PIN A2.
// Dit is de input van je digitale filter. Dit filter moet je zelf bouwen.
// De output van je digitale filter wordt d.m.v. de DAC op PIN A10 gezet.
// De samplefrequentie van deze routine wordt bepaald door een timer interrupt.


//ADC_voltage:
#define MAX_VALUE_ADC   2047                               // only 11 bits are used
#define VCC			(float) 3.3
#define VREF		(float) VCC / 1.6

#define N		256UL
#define F_CLK	1000UL
#define TC_PER	(F_CPU / (N * F_CLK)) - 1

//DAC
#define MAX_VALUE_DAC 4095
#define CAL_DAC 1.000  // Calibratiewaarde DAC

#define ADC2DAC (float)(VREF * MAX_VALUE_DAC * CAL_DAC) / ((MAX_VALUE_ADC + 1) * VCC)

#define FIR_LAYERS 10


void init_timer(void)
{
	TCE0.CTRLB     = TC_WGMODE_NORMAL_gc;	// Normal mode
	TCE0.CTRLA     = TC_CLKSEL_DIV256_gc;	// prescaling 8 (N = 64)
	TCE0.INTCTRLA  = TC_OVFINTLVL_LO_gc;	// enable overflow interrupt low level
	TCE0.PER       = TC_PER;//(62499/2)*0.002;			// sampletijd ( (62499/2) = 1 sample/seconde).
	PMIC.CTRL     |= PMIC_LOLVLEN_bm;		// set low level interrupts
}

void init_adc(void)
{
	PORTA.DIRCLR     = PIN2_bm|PIN3_bm;
	ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN2_gc |				// PA2 (PIN A2) to + channel 0
	ADC_CH_MUXNEG_GND_MODE3_gc;								// GND to - channel 0
	ADCA.CH0.CTRL    = ADC_CH_INPUTMODE_DIFF_gc;			// channel 0 differential
	ADCA.REFCTRL     = ADC_REFSEL_INTVCC_gc;
	ADCA.CTRLB       = ADC_RESOLUTION_12BIT_gc |
	ADC_CONMODE_bm;											// signed conversion
	ADCA.PRESCALER   = ADC_PRESCALER_DIV16_gc;
	ADCA.CTRLA       = ADC_ENABLE_bm;
}

void init_dac(void)
{
	DACB.CTRLC = DAC_REFSEL_AVCC_gc;
	DACB.CTRLB = DAC_CHSEL_SINGLE_gc;
	DACB.CTRLA = DAC_CH0EN_bm | DAC_ENABLE_bm;
}

int16_t read_adc(void)			// return a signed
{
	int16_t res;                                           // is also signed

	ADCA.CH0.CTRL |= ADC_CH_START_bm;
	while ( !(ADCA.CH0.INTFLAGS & ADC_CH_CHIF_bm) ) ;
	res = ADCA.CH0.RES;
	ADCA.CH0.INTFLAGS |= ADC_CH_CHIF_bm;

	return res;
}


ISR(TCE0_OVF_vect)				//TIMER Interupt
{
// 	int16_t  voltage ;		// contains read in voltage (mVolts)
	int32_t res;            // contains read in adc value
	int16_t BinaryValue; 	// contains value to write to DAC

	PORTC.OUTTGL = PIN0_bm;	//Toggle the LED
	
	res = read_adc();			//Read out the ADC (PIN A2)
// 	voltage = (double) res * 1000 * VREF / (MAX_VALUE_ADC + 1);					// Measured voltage in Volts.
// 	if (voltage < 0) voltage = 0;		


	//	<Jochem code>
	
	static int16_t prevs[FIR_LAYERS];
	static uint8_t prevsIndex = 0;
	int16_t oldRes = res;
	
	for(uint8_t i = 0; i < FIR_LAYERS; i++) {
		res += prevs[i];
	}
	
	prevsIndex = (prevsIndex + 1) % FIR_LAYERS;
	prevs[prevsIndex] = oldRes;
	
	res /= FIR_LAYERS + 1;
	
	//	</Jochem code>

	BinaryValue = res * ADC2DAC; //Bitwaarde
	DACB.CH0DATA = BinaryValue;											//write &USBDataIn to DAC (PIN A10)
	while (!DACB.STATUS & DAC_CH0DRE_bm);
}


int main(void)
{
	PORTC.DIRSET = PIN0_bm;	// the LED (bit 0 on port C) is set as output.
	PORTB.DIRSET = PIN2_bm;
	PORTF.DIRSET = PIN0_bm | PIN1_bm;

	init_clock();
	init_stream(F_CPU);		// init uart ("stream.h")
	init_timer();			// init timer
	init_dac();				// init DAC
	init_adc();				// init ADC
	
	sei();					//Enable interrupts
	

	while (1) {
	}
}
