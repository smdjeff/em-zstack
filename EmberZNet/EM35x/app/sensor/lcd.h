// *******************************************************************
//  lcd.h
//
//  common functions for lcd driver  128*64
//
//  Copyright by kaiser. All rights reserved.
// *******************************************************************
#ifndef _LCD_H_
#define _LCD_H_


/*----------------------------------------------------------------------------
 *        Macro
 *----------------------------------------------------------------------------*/
#define GPIO_PxCLR_BASE (GPIO_PACLR_ADDR)
#define GPIO_PxSET_BASE (GPIO_PASET_ADDR)
#define GPIO_PxOUT_BASE (GPIO_PAOUT_ADDR)
// Each port is offset from the previous port by the same amount
#define GPIO_Px_OFFSET  (GPIO_PBCFGL_ADDR-GPIO_PACFGL_ADDR)

#define LCD_SCK_PIN  PORTA_PIN(2)  	/*�ӿڶ���:lcd_sclk ���� LCD �� sclk*/
#define LCD_SID_PIN  PORTA_PIN(1)    /*�ӿڶ���:lcd_sid ���� LCD �� sid*/
#define LCD_RS_PIN   PORTA_PIN(0)    /*�ӿڶ���:lcd_rs ���� LCD �� rs*/
//#define lcd_reset=P1^0; /*�ӿڶ���:lcd_reset ���� LCD �� reset*/
#define LCD_CS1_PIN  PORTA_PIN(3)   /*�ӿڶ���:lcd_cs1 ���� LCD �� cs1*/

#define ROM_IN_PIN   PORTA_PIN(0)  /*�ֿ� IC �ӿڶ���:Rom_IN �����ֿ� IC �� SI*/
#define ROM_OUT_PIN  PORTA_PIN(1)   /*�ֿ� IC �ӿڶ���:Rom_OUT �����ֿ� IC �� SO*/
#define ROM_SCK_PIN  PORTA_PIN(2)   /*�ֿ� IC �ӿڶ���:Rom_SCK �����ֿ� IC �� SCK*/
#define ROM_CS_PIN   PORTC_PIN(2)    /*�ֿ� IC �ӿڶ��� Rom_CS �����ֿ� IC �� CS#*/

#define LCD_PIN_OUT_MODE GPIOCFG_OUT

/* LCD_SCK_ACTION **/
#define LCD_SCK(x)        \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(LCD_SCK_PIN/8)))) = BIT(LCD_SCK_PIN & 7); \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(LCD_SCK_PIN/8)))) = BIT(LCD_SCK_PIN & 7);  \
		  }                \
		}while(0)

/* LCD_SID_ACTION **/
#define LCD_SID(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(LCD_SID_PIN/8)))) = BIT(LCD_SID_PIN & 7); \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(LCD_SID_PIN/8)))) = BIT(LCD_SID_PIN & 7); \
		  }                \
		}while(0)

/* LCD_RS_PIN  **/
#define LCD_RS(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(LCD_RS_PIN/8)))) = BIT(LCD_RS_PIN & 7);   \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(LCD_RS_PIN/8)))) = BIT(LCD_RS_PIN & 7);   \
		  }                \
		}while(0)

/* LCD_CS1_PIN  **/
#define LCD_CS1(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(LCD_CS1_PIN/8)))) = BIT(LCD_CS1_PIN & 7);    \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(LCD_CS1_PIN/8)))) = BIT(LCD_CS1_PIN & 7);    \
		  }                \
		}while(0)

/* ROM_IN_PIN  **/
#define ROM_IN(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(ROM_IN_PIN/8)))) = BIT(ROM_IN_PIN & 7);    \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(ROM_IN_PIN/8)))) = BIT(ROM_IN_PIN & 7);     \
		  }                \
		}while(0)

/* ROM_OUT_PIN  **/
#define ROM_OUT(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(ROM_OUT_PIN/8)))) = BIT(ROM_OUT_PIN & 7);   \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(ROM_OUT_PIN/8)))) = BIT(ROM_OUT_PIN & 7);   \
		  }                \
		}while(0)

/* ROM_SCK_PIN  **/
#define ROM_SCK(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(ROM_SCK_PIN/8)))) = BIT(ROM_SCK_PIN & 7);   \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(ROM_SCK_PIN/8)))) = BIT(ROM_SCK_PIN & 7);   \
		  }                \
		}while(0)

/* ROM_CS_PIN  **/
#define ROM_CS(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(ROM_CS_PIN/8)))) = BIT(ROM_CS_PIN & 7);    \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(ROM_CS_PIN/8)))) = BIT(ROM_CS_PIN & 7);    \
		  }                \
		}while(0)


/*----------------------------------------------------------------------------
 *        Typedef
 *----------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------
 *        Global Extern
 *----------------------------------------------------------------------------*/
extern const int8u jiong1[];
extern const int8u lei1[];
extern const int8u bmp1[];

/*----------------------------------------------------------------------------
 *        Prototype
 *----------------------------------------------------------------------------*/
/****************************************************
 * LCD module initial.
***************************************************/
void initial_lcd();

/****************************************************
 * LCD screen clear.
***************************************************/
void clear_screen();

/****************************************************
 * Display image by 128*64 pixel.
***************************************************/
void display_128x64(const int8u *dp);

/****************************************************
 * Display string by 5*7 pixel.
***************************************************/
void display_string_5x7(int8u y, int8u x, int8u *text);

/****************************************************
 * Display by 16*16 pixel.
***************************************************/
void display_graphic_16x16(int32u page, int32u column, const int8u *dp);

/****************************************************
 * Display string and image by 8*16 pixel
***************************************************/
void display_graphic_8x16(int32u page, int8u column, int8u *dp);

/****************************************************
 * Display string and image by 5*7 pixel
***************************************************/
void display_graphic_5x7(int32u page, int8u column, int8u *dp);

/****************************************************
 * Display GB2312 string.
***************************************************/
void display_GB2312_string(int8u y, int8u x, int8u *text);

/****************************************************
 * Display string by 8x16 pixels.
***************************************************/
void display_string_8x16(int8u row, int8u col, int8u* str);
#endif//_LCD_H_
//eof