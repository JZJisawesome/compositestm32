/* Library to display composite video from a framebuffer on an STM32F103C8T6/B
 * 
 * Hardcoded to use DMA channel 3, SPI1, and TIM4.
 * Hardcoded to output
 * 
** Capabilities
 * Maximum resolution (H,V): 944, 484
 * H resolution may increase/decrease in steps of 8 pixels; can be scaled by powers of 2
 * V resolution may either be interlaced/not (484/242)
 * Note that a composite monitor will only use 720 horizontal pixel samples
 *  Because of SPI prescaler limitations, there is no way to get 720 pixels across without
 *  wasting space on the right side. Recommend stretching original 720x484 image to
 *  944x484 so that is compressed back to the correct aspect ratio
 * 
** Setting Resolution
 * Modify the preprocessor constants within this file
 * Vertical
 *  Decide if INTERLACING
 *  //TODO implement vertical scaling
 * Horizontal
 *  Choose SPI prescaler (length of horizontal pixels)
 *   (SPI_PRESCALER_VALUE:Max h pixels): 0b101:59 0b100:118 0b011:236 0b010:472 0b001:944
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
//Default resolution (H,V): 944, 484 (Interlacing)
//#define COMPOSITE_BYTES_PER_LINE 118
//#define COMPOSITE_INTERLACING
//#define COMPOSITE_SPI_PRESCALER_VALUE 0b001
//#define COMPOSITE_LINE_DIVISOR 1//Don't repeat any lines

//Testing (ram framebuffer 472x242)
#define COMPOSITE_BYTES_PER_LINE 59
#define COMPOSITE_SPI_PRESCALER_VALUE 0b010
#define COMPOSITE_LINE_DIVISOR 1//Don't repeat any lines

//Testing (about image 928x484)
//#define COMPOSITE_BYTES_PER_LINE 116
//#define COMPOSITE_INTERLACING
//#define COMPOSITE_SPI_PRESCALER_VALUE 0b001
////#define COMPOSITE_LINE_DIVISOR 1//Don't repeat any lines

/* Public functions */
void Composite_init(const uint8_t* fb);//Pointer to framebuffer
void Composite_setFramebuffer(const uint8_t* fb);//Pointer to framebuffer; use for double buffering
uint_fast16_t Composite_getCurrentStep();//Can help with screen tearing

#endif//COMPOSITE_H
