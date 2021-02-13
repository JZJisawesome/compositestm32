/* Useful library for drawing to composite frame buffers
 * NO OUT OF BOUNDS CHECKING OTHER THAN SOME ASSERTIONS! BE CAREFUL! #define NDEBUG for speed
 * 
** Function Listing
 * Most functions can be suffixed with _I for white on black instead of the usual black on white 
 * framebuffer and _X for xoring onto the framebuffer, so only the "base" functions are listed.
 * 
 * Initialization (No suffixes)
 *  void SR_setFrameBuffer(uint8_t* frameBuffer);//Composite framebuffer for all functions
 *  void SR_setCharacterRom(const uint8_t characterRom[128][8]);//8x8 and in ASCII order; W on B
 * 
 * Screen Manipulation
 *  //TODO
 * 
 * Point Drawing
 *  void SR_writeToByte(uint32_t xByte, uint32_t y, uint8_t data);//No Suffixes
 *  void SR_drawPointByByte(uint32_t xByte, uint32_t y);
 *  void SR_drawPoint(uint32_t x, uint32_t y);
 * 
 * Character/String Drawing
 *  void SR_drawCharByByte(uint32_t xByte, uint32_t y, char c);
 *  void SR_drawStringByByte(uint32_t xByte, uint32_t y, const char* string);//No Suffixes//TODO
 *  void SR_drawText(uint32_t xByte, uint32_t y, const char* string);
 * 
 * Line Drawing
 *  void SR_drawHLineByByte(uint32_t xByte, uint32_t y, uint32_t xCount);//No Suffixes//TODO
 *  void SR_drawVLineByByte(uint32_t xByte, uint32_t y, uint32_t yCount);//No Suffixes//TODO
 *  void SR_drawHLine(uint32_t x, uint32_t y, uint32_t xCount);
 *  void SR_drawVLine(uint32_t x, uint32_t y, uint32_t yCount);
 * 
 * Shape Drawing (Also _F suffix for filled (Shape outline is default))
 *  void SR_drawRectangleByByte(uint32_t xByte, uint32_t y, uint32_t xCount, uint32_t yCount);
 *  void SR_drawRectangle(uint32_t x, uint32_t y, uint32_t xCount, uint32_t yCount);
 *  void SR_drawTriangle(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);//No Suffixes//TODO
 *  
*/

#ifndef SOFTRENDERER_H
#define SOFTRENDERER_H

#include "bluepill.h"

/* Settings (must be filled in) */
//Defaults
#define SR_BYTES_PER_LINE 59
#define SR_LINES 241

/* Public functions and macros */
//ByByte functions are faster as they don't require bit manipulation, but give you less control
//Functions suffixed with _I mean inverted (draw black pixels instead of white)
//Functions suffixed with _X xor existing pixels to invert things
//Functions suffixed with _OW mean that they overwrite other bits within a byte for speed
//Shape drawing functions suffixed with _F fill their shape in (instead of just a border)

//Initialization Functions (Pointers used by drawing functions)
//Note that these can be changed at any point (provided a draw function isn't running)
//This can allow for double/multiple buffering to avoid screen tearing
void SR_setFrameBuffer(uint8_t* frameBuffer);//Must have dimensions specified above
void SR_setCharacterRom(const uint8_t characterRom[128][8]);//8x8 and in ASCII order; W on black

//Point Drawing
void SR_writeToByte(uint32_t xByte, uint32_t y, uint8_t data);
void SR_drawPointByByte(uint32_t xByte, uint32_t y);
void SR_drawPointByByte_I(uint32_t xByte, uint32_t y);
void SR_drawPointByByte_X(uint32_t xByte, uint32_t y);
void SR_drawPoint(uint32_t x, uint32_t y);
void SR_drawPoint_I(uint32_t x, uint32_t y);
void SR_drawPoint_X(uint32_t x, uint32_t y);

//Screen Manipulation
//TODO implement memset in bluepill.h
//#define SR_clear(fb) {memset(fb, 0x00, BYTES_PER_LINE * LINES);}
//#define SR_fill(fb) {memset(fb, 0xFF, BYTES_PER_LINE * LINES);}

//Char/String Drawing (characters in charRom should be 8 by 8 and in ascii order sequentially)
void SR_drawCharByByte(uint32_t xByte, uint32_t y, char c);
void SR_drawCharByByte_I(uint32_t xByte, uint32_t y, char c);
void SR_drawCharByByte_X(uint32_t xByte, uint32_t y, char c);
void SR_drawStringByByte(uint32_t xByte, uint32_t y, const char* string);//Faster
//SR_drawText(_I,_X) take (uint32_t xByte, uint32_t y, const char* string)
#define SR_drawText(xByte, y, string) _SR_drawText(xByte, y, string, SR_drawCharByByte)
#define SR_drawText_I(xByte, y, string) _SR_drawText(xByte, y, string, SR_drawCharByByte_I)
#define SR_drawText_X(xByte, y, string) _SR_drawText(xByte, y, string, SR_drawCharByByte_X)

//Line Drawing
void SR_drawHLineByByte(uint32_t xByte, uint32_t y, uint32_t xCount);
void SR_drawVLineByByte(uint32_t xByte, uint32_t y, uint32_t yCount);
//SR_drawHLine and SR_drawHLine_X take (uint32_t x, uint32_t y, uint32_t xCount)
#define SR_drawHLine(x, y, xCount) _SR_drawHLine(x, y, xCount, SR_drawPoint)
#define SR_drawHLine_I(x, y, xCount) _SR_drawHLine(x, y, xCount, SR_drawPoint_I)
#define SR_drawHLine_X(x, y, xCount) _SR_drawHLine(x, y, xCount, SR_drawPoint_X)
//SR_drawVLine and SR_drawVLine_X take (uint32_t x, uint32_t y, uint32_t yCount)
#define SR_drawVLine(x, y, yCount) _SR_drawVLine(x, y, yCount, SR_drawPoint)
#define SR_drawVLine_I(x, y, yCount) _SR_drawVLine(x, y, yCount, SR_drawPoint_I)
#define SR_drawVLine_X(x, y, yCount) _SR_drawVLine(x, y, yCount, SR_drawPoint_X)

//SR_drawLineByByte(_I,_X) takes (uint32_t xByte1, uint32_t y1, uint32_t xByte2, uint32_t y2);
#define SR_drawLineByByte(x0, y0, x1, y1) _SR_drawLine(x0, y0, x1, y1, SR_drawPointByByte)
#define SR_drawLineByByte_I(x0, y0, x1, y1) _SR_drawLine(x0, y0, x1, y1, SR_drawPointByByte_I)
#define SR_drawLineByByte_X(x0, y0, x1, y1) _SR_drawLine(x0, y0, x1, y1, SR_drawPointByByte_X)
//SR_drawLine(_I,_X) takes (uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
#define SR_drawLine(x0, y0, x1, y1) _SR_drawLine(x0, y0, x1, y1, SR_drawPoint)
#define SR_drawLine_I(x0, y0, x1, y1) _SR_drawLine(x0, y0, x1, y1, SR_drawPoint_I)
#define SR_drawLine_X(x0, y0, x1, y1) _SR_drawLine(x0, y0, x1, y1, SR_drawPoint_X)

//Shape Drawing
//SR_drawRectangleByByte(_I,_X) takes (uint32_t xByte, uint32_t y, uint32_t xCount, uint32_t yCount)
#define SR_drawRectangleByByte(xByte, y, xCount, yCount) _SR_drawRectangle(xByte, y, xCount, yCount, SR_drawPointByByte)
#define SR_drawRectangleByByte_I(xByte, y, xCount, yCount) _SR_drawRectangle(xByte, y, xCount, yCount, SR_drawPointByByte_I)
#define SR_drawRectangleByByte_X(xByte, y, xCount, yCount) _SR_drawRectangle(xByte, y, xCount, yCount, SR_drawPointByByte_X)
void SR_drawRectangleByByte_F(uint32_t xByte, uint32_t y, uint32_t xCount, uint32_t yCount);
//SR_drawRectangle(_I, _X) takes (uint32_t x, uint32_t y, uint32_t xCount, uint32_t yCount)
#define SR_drawRectangle(x, y, xCount, yCount) _SR_drawRectangle(x, y, xCount, yCount, SR_drawPoint)
#define SR_drawRectangle_I(x, y, xCount, yCount) _SR_drawRectangle(x, y, xCount, yCount, SR_drawPoint_I)
#define SR_drawRectangle_X(x, y, xCount, yCount) _SR_drawRectangle(x, y, xCount, yCount, SR_drawPoint_X)
void SR_drawRectangle_F(uint32_t x, uint32_t y, uint32_t xCount, uint32_t yCount);

void SR_drawTriangle(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);

void SR_drawCircleByByte(uint32_t xByte, uint32_t y, uint32_t radius);
void SR_drawCircleByByte_F(uint32_t xByte, uint32_t y, uint32_t radius);

/* Public Functions Called By Public Macros */
//Costs performance, but improves maintainability

//Char/String Drawing
void _SR_drawText(uint32_t xByte, uint32_t y, const char* string, void (*drawCharByByte)(uint32_t, uint32_t, char));

//Line Drawing
void _SR_drawHLine(uint32_t x, uint32_t y, uint32_t xCount, void (*plot)(uint32_t, uint32_t));
void _SR_drawVLine(uint32_t x, uint32_t y, uint32_t yCount, void (*plot)(uint32_t, uint32_t));
void _SR_drawLine(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, void (*plot)(uint32_t, uint32_t));

//Shape Drawing
void _SR_drawRectangle(uint32_t x, uint32_t y, uint32_t xCount, uint32_t yCount, void (*plot)(uint32_t, uint32_t));

#endif//SOFTRENDERER_H
