#include "bluepill.h"
#include "softrenderer.h"

/* Private Definitions */
//Private vars
static uint8_t* fb;
static const uint8_t* charRom;//128 bytes wide, 8 bytes down

//Private functions

//Private macros
#define SR_abs(num) ((uint32_t)(((int32_t)(num) < 0) ? -(int32_t)(num) : (int32_t)(num)))

/* Public Functions */

/* Initialization */
void SR_setFrameBuffer(uint8_t* frameBuffer)//Must have dimensions specified above
{
    fb = frameBuffer;
}

void SR_setCharacterRom(const uint8_t characterRom[128][8])//8x8 and in ASCII order
{
    charRom = (uint8_t*)(characterRom);
}

/* Point Drawing */
void SR_writeToByte(uint32_t xByte, uint32_t y, uint8_t data)
{
    uint8_t* const line = fb + (y * SR_BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + xByte;//Determine byte in line
    *destination = data;//Write data to byte
    __nop();//FIXME remove this once memset is implemented in bluepill.h
}

void SR_drawPointByByte(uint32_t xByte, uint32_t y)
{
    uint8_t* const line = fb + (y * SR_BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + xByte;//Determine byte in line
    *destination = 0xFF;//Write 0xFF to byte
    __nop();//FIXME remove this once memset is implemented in bluepill.h
}

void SR_drawPointByByte_I(uint32_t xByte, uint32_t y)
{
    uint8_t* const line = fb + (y * SR_BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + xByte;//Determine byte in line
    *destination = 0x00;//Write 0xFF to byte
    __nop();//FIXME remove this once memset is implemented in bluepill.h
}

void SR_drawPointByByte_X(uint32_t xByte, uint32_t y)
{
    //Does not xor individual bits; ors all bits in byte, xors with that, and writes to all bits
    uint8_t* const line = fb + (y * SR_BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + xByte;//Determine byte in line
    
    if (*destination)//Any of the bits of destination are set
        *destination = 0x00;
    else
        *destination = 0xFF;
}

void SR_drawPoint(uint32_t x, uint32_t y)
{
    uint8_t* const line = fb + (y * SR_BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + (x / 8);//Determine byte in line
    uint8_t bitmask = 0x80 >> (x % 8);//Determine bit in byte to set
    *destination |= bitmask;//Set bit in byte
}

void SR_drawPoint_I(uint32_t x, uint32_t y)
{
    uint8_t* const line = fb + (y * SR_BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + (x / 8);//Determine byte in line
    uint8_t bitmask = ~(0x80 >> (x % 8));//Determine bit in byte to clear
    *destination &= bitmask;//Clear bit in byte
}

void SR_drawPoint_X(uint32_t x, uint32_t y)
{
    uint8_t* const line = fb + (y * SR_BYTES_PER_LINE);//Determine line
    uint8_t* const destination = line + (x / 8);//Determine byte in line
    uint8_t bitmask = 0x80 >> (x % 8);//Determine bit in byte to set
    *destination ^= bitmask;//Xor bit in byte
}

/* Character Drawing */
void SR_drawCharByByte(uint32_t xByte, uint32_t y, char c)
{
    //Bounds checking
    assert(xByte < SR_BYTES_PER_LINE);
    assert(y < (SR_LINES - 8));
    
    //Copy character from charRom to fb. c is the offset into charRom
    const uint8_t* charPointer = charRom + (c * 8);//Index into character rom
    uint8_t* line = fb + (y * SR_BYTES_PER_LINE);//Address of first line to copy char to
    for (uint32_t i = 0; i < 8; ++i)
    {
        uint8_t* const destination = line + xByte;//Index into the line
        *destination |= *charPointer;//Or byte of character into framebuffer
        
        line += SR_BYTES_PER_LINE;//Go to the next line
        ++charPointer;//Go to next line of character
    }
}

void SR_drawCharByByte_I(uint32_t xByte, uint32_t y, char c)
{
    //Bounds checking
    assert(xByte < SR_BYTES_PER_LINE);
    assert(y < (SR_LINES - 8));
    
    //Copy character from charRom to fb. c is the offset into charRom
    const uint8_t* charPointer = charRom + (c * 8);//Index into character rom
    uint8_t* line = fb + (y * SR_BYTES_PER_LINE);//Address of first line to copy char to
    for (uint32_t i = 0; i < 8; ++i)
    {
        uint8_t* const destination = line + xByte;//Index into the line
        *destination &= ~(*charPointer);//And inverted byte of character into framebuffer
        
        line += SR_BYTES_PER_LINE;//Go to the next line
        ++charPointer;//Go to next line of character
    }
}

void SR_drawCharByByte_X(uint32_t xByte, uint32_t y, char c)
{
    //Bounds checking
    assert(xByte < SR_BYTES_PER_LINE);
    assert(y < (SR_LINES - 8));
    
    //Copy character from charRom to fb. c is the offset into charRom
    const uint8_t* charPointer = charRom + (c * 8);//Index into character rom
    uint8_t* line = fb + (y * SR_BYTES_PER_LINE);//Address of first line to copy char to
    for (uint32_t i = 0; i < 8; ++i)
    {
        uint8_t* const destination = line + xByte;//Index into the line
        *destination ^= *charPointer;//Xor byte of character into framebuffer
        
        line += SR_BYTES_PER_LINE;//Go to the next line
        ++charPointer;//Go to next line of character
    }
}

void SR_drawStringByByte(uint32_t xByte, uint32_t y, const char* string)
{
    while (true)
    {
        const char character = *string;
        
        if (character)//Not null byte
        {
            SR_drawCharByByte(character, xByte, y);
        }
        else
            return;//Null byte encountered
        
        ++string;
        ++xByte;
    }
}

void _SR_drawText(uint32_t xByte, uint32_t y, const char* string, void (*drawCharByByte)(uint32_t, uint32_t, char))
{
    while (true)
    {
        const char character = *(string++);//Dereference and increment
        
        switch (character)
        {
            case 0x00://Null byte
            {
                return;//Finished string
            }
            case '\t':
            {
                xByte += 4;//4 spaces
                break;
            }
            case '\n':
            case '\r':
            {
                //Cause wrap to occur by going past end of line
                xByte = SR_BYTES_PER_LINE;//1 past end of line
                break;
            }
            case '\v':
            {
                y += 8;//Move down one character
                break;
            }
            default:
            {
                drawCharByByte(xByte, y, character);
                ++xByte;
            }
        }
        
        if (xByte > (SR_BYTES_PER_LINE - 1))
        {
            //Wrap text
            xByte = 0;
            y += 8;
        }
        
        if (y > (SR_LINES - 1))
            y = 0;
    }
}

/* Line Drawing */
void SR_drawHLineByByte(uint32_t xByte, uint32_t y, uint32_t xCount)
{
    uint32_t end = xByte + xCount;
    while (xByte < end)
    {
        SR_drawPointByByte(xByte, y);
        ++xByte;//Go to next byte
    }
}

void SR_drawVLineByByte(uint32_t xByte, uint32_t y, uint32_t yCount)
{
    uint32_t end = y + yCount;
    while (y < end)
    {
        SR_drawPointByByte(xByte, y);
        ++y;//Go to next line
    }
}

void _SR_drawHLine(uint32_t x, uint32_t y, uint32_t xCount, void (*plot)(uint32_t, uint32_t))
{
    //TODO for speed, do the bulk of the work by byte
    uint32_t end = x + xCount;
    while (x < end)
    {
        plot(x, y);
        ++x;//Go to next pixel
    }
}
void _SR_drawVLine(uint32_t x, uint32_t y, uint32_t yCount, void (*plot)(uint32_t, uint32_t))
{
    uint32_t end = y + yCount;
    while (y < end)
    {
        plot(x, y);
        ++y;//Go to next line
    }
}

void _SR_drawLine(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, void (*plot)(uint32_t, uint32_t))
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
        plot(x, y);
        
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

//Shape Drawing
void _SR_drawRectangle(uint32_t x, uint32_t y, uint32_t xCount, uint32_t yCount, void (*plot)(uint32_t, uint32_t))
{
    _SR_drawHLine(x, y, xCount, plot);//Top
    _SR_drawHLine(x, y + yCount, xCount, plot);//Bottom
    _SR_drawVLine(x, y, yCount, plot);//Left
    _SR_drawVLine(x + xCount, y, yCount, plot);//Right
}

void SR_drawRectangleByByte_F(uint32_t xByte, uint32_t y, uint32_t xCount, uint32_t yCount)
{
    uint32_t end = y + yCount;
    while (y < end)
    {
        SR_drawHLineByByte(xByte, y, xCount);
        ++y;
    }
}

void SR_drawRectangle_F(uint32_t x, uint32_t y, uint32_t xCount, uint32_t yCount)
{
    uint32_t end = y + yCount;
    while (y < end)
    {
        SR_drawHLine(x, y, xCount);
        ++y;
    }
}

void SR_drawTriangle(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
    SR_drawLine(x0, y0, x1, y1);
    SR_drawLine(x1, y1, x2, y2);
    SR_drawLine(x2, y2, x0, y0);
}

void SR_drawCircleByByte(uint32_t xByte, uint32_t y, uint32_t radius)
{
    //https://en.wikipedia.org/wiki/Midpoint_circle_algorithm
    //TODO
}

void SR_drawCircleByByte_F(uint32_t xByte, uint32_t y, uint32_t radius)
{
    //TODO

}


/* Private Functions */

//None so far
