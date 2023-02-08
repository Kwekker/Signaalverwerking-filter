#define F_CPU 32000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
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

#define N		8UL
#define F_CLK	300UL
#define TC_PER	(F_CPU / ((N * F_CLK)) - 1)

//DAC
#define MAX_VALUE_DAC 4095
#define CAL_DAC 1.000  // Calibratiewaarde DAC

#define ADC2DAC (float)(VREF * MAX_VALUE_DAC * CAL_DAC) / ((MAX_VALUE_ADC + 1) * VCC)
// 8445.9375 / 6758.4 = 1.24969482
#define ADC2DAC_NUM 8446
#define ADC2DAC_DEN 6758

void init_dac(void);
void init_adc(void);
void init_timer(void);


// Coëfficients. * 10^11 / 10^11
int64_t numerator[]		= {467263, 1401790, 1401790, 467263};
int64_t denominator[]	= {100000000000, -295175000000, 290568000000, -95389600000};
	
// numerator * c:
// c*a*x[n] + c*b*x[n-1] ...


typedef struct {
	int16_t buffer[4];
	uint8_t index : 2;
} cycle_t;

int16_t cycleGet(cycle_t cycle, int8_t index);
void cyclePush(cycle_t* cycle, int16_t value);


int main(void) {
	PORTC.DIRSET = PIN0_bm;	// the LED (bit 0 on port C) is set as output.
	PORTB.DIRSET = PIN2_bm;
	PORTF.DIRSET = PIN0_bm | PIN1_bm;

	init_clock();
	init_timer();			// init timer
	init_dac();				// init DAC
	init_adc();				// init ADC
		
	PMIC.CTRL     |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm;		// set low and medium level interrupts
	sei();					//Enable interrupts
	
	while (1) {
		asm volatile("nop");
	}
}



ISR(ADCA_CH0_vect){
	PORTC.OUTTGL = PIN0_bm;	//Toggle the LED
	
	// ay[n] + by[n-1] + cy[n-2] + dy[n-3] = px[n] + qx[n-1] + rx[n-2] + ux[n-3]
	// ay[n] = px[n] + qx[n-1] + rx[n-2] + ux[n-3] - by[n-1] - cy[n-2] - dy[n-3]
	// y[n]  = (px[n] + qx[n-1] + rx[n-2] + ux[n-3] - by[n-1] - cy[n-2] - dy[n-3]) / a
	
	int64_t endValue = 0;
	static cycle_t input = {{0, 0, 0, 0}, 0};
	static cycle_t output = {{0, 0, 0, 0}, 0};	
	
	cyclePush(input, ADCA.CH0.RES);	

	for (uint8_t i = 0; i < 4; i++) {
		endValue += numerator[i] * cycleGet(input, -i);
	}
	for (uint8_t i = 1; i < 4; i++) {
		endValue += denominator[i] * cycleGet(output, -i);
	}
	
	cyclePush(&output, endValue);


	endValue *= ADC2DAC_NUM;
	endValue /= ADC2DAC_DEN;
	
	// Check if output is not fucked
	if (endValue >= (0b1 << 12)) PORTF.OUTSET = PIN0_bm;
	
	// Fucking amputate the LS 12 bits
	DACB.CH0DATA = endValue & 0x0fff;
	while (!DACB.STATUS & DAC_CH0DRE_bm);
}


//Make use of the 2 bit index variable
int16_t cycleGet(cycle_t cycle, int8_t index) {
	cycle.index += index;
	return cycle.buffer[cycle.index];
}

void cyclePush(cycle_t* cycle, int16_t value) {
	cycle->index++;
	cycle->buffer[cycle->index] = value;
}



void init_timer(void){
	TCE0.CTRLB     = TC_WGMODE_NORMAL_gc;	// Normal mode
	TCE0.CTRLA     = TC_CLKSEL_DIV8_gc;	// prescaling 8 (N = 64)
	//TCE0.INTCTRLA  = TC_OVFINTLVL_MED_gc;	// enable overflow interrupt medium level
	TCE0.PER       = TC_PER;	//(62499/2)*0.002;			// sampletijd ( (62499/2) = 1 sample/seconde).
}

void init_adc(void){
	PORTA.DIRCLR		= PIN2_bm|PIN3_bm;
	ADCA.CH0.MUXCTRL	= ADC_CH_MUXPOS_PIN2_gc |	// PA2 (PIN A2) to + channel 0
	ADC_CH_MUXNEG_GND_MODE3_gc;						// GND to - channel 0
	ADCA.CH0.CTRL	= ADC_CH_INPUTMODE_DIFF_gc;		// channel 0 differential mode
	ADCA.REFCTRL	= ADC_REFSEL_INTVCC_gc;
	ADCA.CTRLB		= ADC_RESOLUTION_12BIT_gc |
	ADC_CONMODE_bm;				// signed conversion
	ADCA.PRESCALER	= ADC_PRESCALER_DIV16_gc;
	ADCA.EVCTRL		= ADC_SWEEP_0_gc |			// sweep channel 0
	ADC_EVSEL_0123_gc |		// select event channel 0, 1, 2, 3
	ADC_EVACT_CH0_gc;			// event system triggers channel 0
	EVSYS.CH0MUX	= EVSYS_CHMUX_TCE0_OVF_gc;	// Timer overflow E0 event
	ADCA.CH0.INTCTRL	= ADC_CH_INTMODE_COMPLETE_gc | ADC_CH_INTLVL_LO_gc; // enable ADCA CH0 interrupt when conversion is complete
	ADCA.CTRLA		= ADC_ENABLE_bm;
}

void init_dac(void){
	DACB.CTRLC = DAC_REFSEL_AVCC_gc;
	DACB.CTRLB = DAC_CHSEL_SINGLE_gc;
	DACB.CTRLA = DAC_CH0EN_bm | DAC_ENABLE_bm;
}
