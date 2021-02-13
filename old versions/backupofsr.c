#include "bluepill.h"
#include "softrenderer.h"

/* Private Definitions */
//Private vars
static uint8_t* fb;
static const uint8_t charRom[128][8];

//Private functions


/* Public Functions */

/* Initialization */
void SR_setFrameBuffer(uint8_t* frameBuffer)//Must have dimensions specified above
{
    fb = frameBuffer;
}

void SR_setCharacterRom(const uint8_t characterRom[128][8])//8x8 and in ASCII order
{
    charRom = characterRom;
}

/* Point Drawing */
void SR_writeToByte(uint8_t* fb, uint32_t xByte, uint32_t y, uint8_t data)
{
    uint8_t* const line = fb + (y * BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + xByte;//Determine byte in line
    *destination = data;//Write data to byte
    __nop();//FIXME remove this once memset is implemented in bluepill.h
}

void SR_drawPoint(uint8_t* fb, uint32_t x, uint32_t y)
{
    uint8_t* const line = fb + (y * BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + (x / 8);//Determine byte in line
    uint8_t bitmask = 0x80 >> (x % 8);//Determine bit in byte
    *destination |= bitmask;//Set bit in byte
}

void SR_drawPoint_I(uint8_t* fb, uint32_t x, uint32_t y)
{
    //TODO
}

/* Character Drawing */
void SR_drawCharByByte(uint8_t* fb, const uint8_t charRom[128][8], char c, uint32_t xByte, uint32_t y)
{
    //Bounds checking
    assert(xByte < BYTES_PER_LINE);
    assert(y < (LINES - 8));
    
    //Copy character from charRom to fb. c is the offset into charRom
    uint8_t* line = fb + (y * BYTES_PER_LINE);//Address of first line to copy char to
    for (uint32_t i = 0; i < 8; ++i)
    {
        uint8_t* const destination = line + xByte;//Index into the line
        *destination |= charRom[c][i];//Or byte of character into framebuffer
        
        line += BYTES_PER_LINE;//Go to the next line
    }
}

void SR_drawCharByByte_I(uint8_t* fb, const uint8_t charRom[128][8], char c, uint32_t xByte, uint32_t y)
{
    //Bounds checking
    assert(xByte < BYTES_PER_LINE);
    assert(y < (LINES - 8));
    
    //Copy character from charRom to fb. c is the offset into charRom
    uint8_t* line = fb + (y * BYTES_PER_LINE);//Address of first line to copy char to
    for (uint32_t i = 0; i < 8; ++i)
    {
        uint8_t* const destination = line + xByte;//Index into the line
        *destination &= ~charRom[c][i];//And inverted byte of character into framebuffer
        
        line += BYTES_PER_LINE;//Go to the next line
    }
}

/* Line Drawing */
void SR_drawHLineByByte(uint8_t* fb, uint32_t xByte, uint32_t y, uint32_t count)
{
    while (count > 0)
    {
        SR_drawPointByByte(fb, xByte, y);
        ++xByte;//Increment byte being accessed
        --count;//Decrement count
    }
}

void SR_drawVLineByByte(uint8_t* fb, uint32_t xByte, uint32_t y, uint32_t count)
{
    while (count > 0)
    {
        SR_drawPointByByte(fb, xByte, y);
        ++y;//Increment line being accessed
        --count;//Decrement count
    }
}

void SR_drawHLine(uint8_t* fb, uint32_t x, uint32_t y, uint32_t count)
{
    //TODO
}
void SR_drawVLine(uint8_t* fb, uint32_t x, uint32_t y, uint32_t count)
{
    //TODO
}

void SR_drawLineByByte(uint8_t* fb, uint32_t xByte0, uint32_t y0, uint32_t xByte1, uint32_t y1)
{
    //https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
    //TODO understand how this works and add comments
    
    //Initial Parameters (Note: SR_abs works with unsigned values)
    int32_t deltaX = SR_abs(xByte1 - xByte0);
    int32_t deltaY = -SR_abs(y1 - y0);
    
    int32_t xAddend = (xByte0 < xByte1) ? 1 : -1;
    int32_t yAddend = (y0 < y1) ? 1 : -1;
    
    //Drawing Loop
    int32_t errorXY = deltaX + deltaY;
    int32_t doubleErrorXY;
    uint32_t x = xByte0, y = y0;
    while (true)
    {
        SR_drawPointByByte(fb, x, y);
        
        if ((x == xByte1) && (y == y1))
            return;//We're reached our destination point
            
        doubleErrorXY = 2 * errorXY;
        
        if (doubleErrorXY >= deltaY)//Wiki comment: (errorXY + errorX) > 0
        {
            errorXY += deltaY;
            x += xAddend;
        }
        
        if (doubleErrorXY <= deltaX)//Wiki comment: (errorXY + errorY) < 0
        {
            errorXY += deltaX;
            y += yAddend;
        }
    }
}

void SR_drawLine(uint8_t* fb, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    //https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
    //TODO understand how this works and add comments
    
    //Initial Parameters (Note: SR_abs works with unsigned values)
    int32_t deltaX = SR_abs(x1 - x0);
    int32_t deltaY = -SR_abs(y1 - y0);
    
    int32_t xAddend = (x0 < x1) ? 1 : -1;
    int32_t yAddend = (y0 < y1) ? 1 : -1;
    
    //Drawing Loop
    int32_t errorXY = deltaX + deltaY;
    int32_t doubleErrorXY;
    uint32_t x = x0, y = y0;
    while (true)
    {
        SR_drawPoint(fb, x, y);
        
        if ((x == x1) && (y == y1))
            return;//We're reached our destination point
            
        doubleErrorXY = 2 * errorXY;
        
        if (doubleErrorXY >= deltaY)//Wiki comment: (errorXY + errorX) > 0
        {
            errorXY += deltaY;
            x += xAddend;
        }
        
        if (doubleErrorXY <= deltaX)//Wiki comment: (errorXY + errorY) < 0
        {
            errorXY += deltaX;
            y += yAddend;
        }
    }
}

/* Private Functions */

//None so far
