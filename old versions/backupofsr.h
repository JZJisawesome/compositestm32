 
/* Useful library for drawing to composite frame buffers
 * NO OUT OF BOUNDS CHECKING OTHER THAN SOME ASSERTIONS! BE CAREFUL! #define NDEBUG for speed
 * 
*/

#ifndef SOFTRENDERER_H
#define SOFTRENDERER_H

#include "bluepill.h"

/* Settings (must be filled in) */
//Defaults
#define BYTES_PER_LINE 58
#define LINES 241

/* Public functions and macros */
//ByByte functions are faster as they don't require bit manipulation, but give you less control
//Functions suffixed with _I mean inverted (draw black pixels instead of white)
//Functions suffixed with _OW mean that they overwrite other bits within a byte for speed

//Initialization Functions (Pointers used by drawing functions)
//Note that these can be changed at any point (provided a draw function isn't running)
//This can allow for double/multiple buffering to avoid screen tearing
void SR_setFrameBuffer(uint8_t* frameBuffer);//Must have dimensions specified above
void SR_setCharacterRom(const uint8_t characterRom[128][8]);//8x8 and in ASCII order

//Point Drawing
void SR_writeToByte(uint8_t* fb, uint32_t xByte, uint32_t y, uint8_t data);
#define SR_drawPointByByte(fb, xByte, y) do {SR_writeToByte(fb, xByte, y, 0xFF);} while (0)
#define SR_drawPointByByte_I(fb, xByte, y) do {SR_writeToByte(fb, xByte, y, 0x00);} while (0)
void SR_drawPoint(uint8_t* fb, uint32_t x, uint32_t y);
void SR_drawPoint_I(uint8_t* fb, uint32_t x, uint32_t y);

//Screen Manipulation
//TODO implement memset in bluepill.h
//#define SR_clear(fb) {memset(fb, 0x00, BYTES_PER_LINE * LINES);}
//#define SR_fill(fb) {memset(fb, 0xFF, BYTES_PER_LINE * LINES);}

//Character Drawing (characters in charRom should be 8 by 8 and in ascii order sequentially)
void SR_drawCharByByte(uint8_t* fb, const uint8_t charRom[128][8], char c, uint32_t xByte, uint32_t y);
void SR_drawCharByByte_I(uint8_t* fb, const uint8_t charRom[128][8], char c, uint32_t xByte, uint32_t y);

//Line Drawing
void SR_drawHLineByByte(uint8_t* fb, uint32_t xByte, uint32_t y, uint32_t count);
void SR_drawVLineByByte(uint8_t* fb, uint32_t xByte, uint32_t y, uint32_t count);
void SR_drawHLine(uint8_t* fb, uint32_t x, uint32_t y, uint32_t count);
void SR_drawVLine(uint8_t* fb, uint32_t x, uint32_t y, uint32_t count);
void SR_drawLineByByte(uint8_t* fb, uint32_t xByte1, uint32_t y1, uint32_t xByte2, uint32_t y2);
void SR_drawLine(uint8_t* fb, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);

//Shape Drawing

/* Private Macros */

//Lots of casting to ensure that even unsigned numbers work
#define SR_abs(num) ((uint32_t)(((int32_t)(num) < 0) ? -(int32_t)(num) : (int32_t)(num)))

#endif//SOFTRENDERER_H
