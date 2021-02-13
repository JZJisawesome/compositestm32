/* Library to display composite video from a framebuffer on an STM32F103C8T6/B
 * 
 * Hardcoded to use DMA channel 3, SPI1, and TIM4.
 * Hardcoded to output
 * 
** Capabilities
 * Maximum resolution (H,V): 928, 483
 * H resolution may increase/decrease in steps of 8 pixels; can be scaled by powers of 2
 * V resolution may either be interlaced/not (483/241)
 * Note that a composite monitor will only use 720 horizontal pixel samples
 *  Because of SPI prescaler limitations, there is no way to get 720 pixels across without
 *  wasting space on the right side
 * Note
 *  If using an odd resolution must make actual framebuffer even (eg. 241->242, 483->484)
 * //TODO determine if this means that a top pixel is being cut off
 * 
** Setting Resolution
 * Modify the preprocessor constants within this file
 * Vertical
 *  Decide if INTERLACING
 *  //TODO implement vertical scaling
 * Horizontal
 *  Choose SPI prescaler (length of horizontal pixels)
 *   (SPI_PRESCALER_VALUE:Max h pixels): 0b101:58 0b100:116 0b011:232 0b010:464 0b001:928
 *   Can choose other vals, but those are the most useful. 0b000 is overclocking, but gives 1856
 *   //TODO validate those are indeed the maximum h pixels on screen
 *  Choose number of byte per line. 1 byte = 8 pixels. Can be less than max h pixels
 *  
** Hardware
 *  Schematic (ALL OUTPUTS ARE PUSH-PULL)
 *   Video pin -> anode-diode-cathode -> 195 ohm resistor -> Output
 *   Sync pin -> anode-diode-cathode -> 650 ohm resistor -> Output
 *   Inside monitor: Output -> 75 ohm resistor -> ground
 *  Voltage Levels For Composite
 *   Video low, Sync low: 0v: hsync/vsync
 *   Video low, Sync high: 0.3v: Front/back borch and black pixel
 *   Video high, sync low: 1v: White pixel
 *
** Resistance Calculations
 * Remember 75ohm resistance to ground inside of display from input
 * 
 * Scenario: Syncing (want 0v)
 *      Video and sync low, diodes off, output pulled down by 75 ohm input impedance. Get 0v
 * 
 * Scenario: Drawing white pixel (want 1v)
 *      Remember 75ohm input impedance. Set video high, and sync low. Sync diode becomes high Z.
 *      Video voltage before diode (3.3v) lowered by 0.7v, now 2.6v
 *      Ratio: (1v/2.6v) = (75/R_video)
 *      Rearrange: R_video = 75(2.6) = 195 ohms
 * 
 * Scenario: Front/back porch and black pixel (want 0.3v)
 *      Remember 75ohm input impedance. Set video low, and sync high. Video diode becomes high Z.
 *      Sync voltage before diode (3.3v) lowered by 0.7v, now 2.6v
 *      Ratio: (0.3v/2.6v) = (75/R_sync)
 *      Rearrange and solve: R_sync = 650 ohms
*/

#ifndef COMPOSITE_H
#define COMPOSITE_H

#include "bluepill.h"

/* Settings */
//Default resolution (H,V): 928, 483 (Interlacing)
/*
#define COMPOSITE_BYTES_PER_LINE 116
#define COMPOSITE_INTERLACING
#define COMPOSITE_SPI_PRESCALER_VALUE 0b001
*/
//Testing
#define COMPOSITE_BYTES_PER_LINE 58
//#define COMPOSITE_INTERLACING
#define COMPOSITE_SPI_PRESCALER_VALUE 0b010

/* Public functions */
void Composite_init(const uint8_t* fb);//Pointer to framebuffer
uint_fast16_t getCurrentLine();//Along with isEvenFrame, can help with screen tearing
bool isEvenFrame();//AKA is odd field

#endif//COMPOSITE_H
