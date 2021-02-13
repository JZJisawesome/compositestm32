//Uses SPI, DMA, and interrupts to display composite video. Almost 0 cpu usage needed
//Max effective image size of H,V: 720,484
//Storage and display Size H,V: 944, 484. The horizontal is compressed because of spi
//SPI prescaler granularity is low, so in order to get a proper image, stretch horizontal pixels
//from 720 to 944, so that they are compressed back to 720 when displayed
//Limited by ram size for ram fb (H,V:472, 242)
//PA7 is video
//PB8 is sync

#include "bluepill.h"
#include "composite.h"
#include "softrenderer.h"
#include "bitmaps/vincent.h"
#include "bitmaps/about.h"

//              y, x
uint8_t ramFB[242][59];//59*8=472

int32_t rand();
void clearFaster();
void fillFaster();
void demo();

void main()
{
    //Static image (928x484)
    //Composite_init(about);
    //while (true);
    
    //Ram framebuffer (464 by 242)
    //fillFaster();
    Composite_init((uint8_t*)ramFB);
    //while (true);
    
    SR_setFrameBuffer((uint8_t*)ramFB);
    SR_setCharacterRom(vincentFont);
    
    demo();
}

//Random
uint32_t seed = 0x1234abcd;
#define RAND_SHIFT_VAL_1 13
#define RAND_SHIFT_VAL_2 17
#define RAND_SHIFT_VAL_3 5

int32_t rand()
{
    uint32_t temporary = seed << RAND_SHIFT_VAL_1;
    seed ^= temporary;
    temporary = seed >> RAND_SHIFT_VAL_2;
    seed ^= temporary;
    temporary = seed << RAND_SHIFT_VAL_3;
    seed ^= temporary;
    return seed;
}

void clearFaster()
{
    for (uint32_t i = 0; i < 242; ++i)
    {
        for (uint32_t j = 0; j < 59; ++j)
        {
            ramFB[i][j] = 0x00;
            __delayInstructions(1000);
        }
    }
}

void fillFaster()
{
    for (uint32_t i = 0; i < 242; ++i)
    {
        for (uint32_t j = 0; j < 59; ++j)
        {
            ramFB[i][j] = 0xFF;
            __delayInstructions(1000);
        }
    }
}

void demo()
{
    while (true)
    {
        //Intro
        SR_drawText(1, 8, "Hello World!");
        
        SR_drawText(13, 72, "Software Rendering+Composite Demo!");
        SR_drawText(14, 88, "By JZJ (In C and ARM assembly)");
        
        SR_drawText(1, 120, "Line Drawing Algorithm: Bresenham (Integer Arithmetic)");
        SR_drawText(1, 128, "RNG Algorithm:\t\t  Xorshift (32 bit)");
        SR_drawText(1, 136, "(Referenced wikipedia to implement these in C):");
        SR_drawText(1, 144, "en.wikipedia.org/wiki/Bresenham%27s_line_algorithm");
        SR_drawText(1, 152, "en.wikipedia.org/wiki/Xorshift");
        
        SR_drawText(1, 168, "Font: Vincent (forum.osdev.org/viewtopic.php?f=2&t=22033)");
        
        SR_drawText(1, 184, "ARM init code from Github referenced to help implement");
        
        SR_drawText(1, 200, "Microcontroller: STM32F103C8T6");
        SR_drawText(1, 208, "Resolution: 472 by 242 (Limited by 20KiB of SRAM)");
        
        __delayInstructions(200000000);
        fillFaster();
        
        //Dots
        SR_drawText_I(27, 104, "Dots!");
        __delayInstructions(72000000);  
        for (uint32_t i = 0; i < 10000; ++i)
        {
            uint32_t x = (uint32_t)rand() % 472;
            uint32_t y = (uint32_t)rand() % 242;
            
            SR_drawPoint_I(x, y);
            __delayInstructions(1000);
        }
        __delayInstructions(72000000);
        clearFaster();
        
        //Lines
        SR_drawText(27, 104, "Lines!");
        __delayInstructions(72000000); 
        for (uint32_t i = 0; i < 100; ++i)
        {
            uint32_t x0 = (uint32_t)rand() % 472;
            uint32_t x1 = (uint32_t)rand() % 472;
            uint32_t y0 = (uint32_t)rand() % 242;
            uint32_t y1 = (uint32_t)rand() % 242;
            
            SR_drawLine(x0, y0, x1, y1);
            __delayInstructions(1000000);
        }
        __delayInstructions(72000000);
        fillFaster();
        
        //Rectangles
        SR_drawText_I(23, 104, "Rectangles!");
        __delayInstructions(72000000); 
        for (uint32_t i = 0; i < 100; ++i)
        {
            uint32_t x = (uint32_t)rand() % 472;
            uint32_t y = (uint32_t)rand() % 242;
            uint32_t xCount = (uint32_t)rand() % (472 - x);
            uint32_t yCount = (uint32_t)rand() % (242 - y);
            
            SR_drawRectangle_I(x, y, xCount, yCount);
            __delayInstructions(1000000);
        }
        __delayInstructions(72000000);
        clearFaster();
        
        //Triangles
        SR_drawText(23, 104, "Triangles!");
        __delayInstructions(72000000); 
        for (uint32_t i = 0; i < 100; ++i)
        {
            uint32_t x0 = (uint32_t)rand() % 472;
            uint32_t x1 = (uint32_t)rand() % 472;
            uint32_t x2 = (uint32_t)rand() % 472;
            uint32_t y0 = (uint32_t)rand() % 242;
            uint32_t y1 = (uint32_t)rand() % 242;
            uint32_t y2 = (uint32_t)rand() % 242;
            
            SR_drawTriangle(x0, y0, x1, y1, x2, y2);
            __delayInstructions(1000000);
        }
        __delayInstructions(72000000);
        clearFaster();
    }
}
