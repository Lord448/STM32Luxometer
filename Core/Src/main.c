/**
 * 	Parsed Loop Scheme, cooperative tasks
 * 	Error Codes
 * 	0x0000:
 *  EEPROM Memory Map
 *  Memory regions
 *  0x0000 - 0x0002 Menu Configurations
 *  Variables
 *	//Menu Configurations
 *	0x0000: Variable that contains if the system is in Factory Values : 8 bits
 *	0x0001: Variable Mode : 8 bits
 *	0x0002: Variable Resolutions : 8 bits
 *
 *	Version 0.3.1
 *	Version E.3.1
 *	Watchdog timer in 400ms
 */

#include "main.h"
#include "ssd1306.h"
#include "fonts.h"
#include "Rojo_BH1750.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define EEPROM_ADDR 0b10100000
#define ReadMask (uint32_t) 0x1F
#define EndOfCounts250ms 65454 //@274PSC: 250ms
#define EndOfCounts50ms 13139  //@274PS: 50ms
#define SizeOfSlotsArray 5
#define Seconds(x) x*4 //Only valid for the Timer_Delay_250ms
#define DefaultSampleTime 10
#define DefaultResolution 54612

//#define USER_PLOT_DEBUG
//#define USER_CONF_P_DEBUG
//#define SHOW_LOADING //On the initial boot

#define ONE_SENSOR
#define USER_DEBUG
#define ECONOMIC_VERSION //For the versions of the instrument that doesn't have the EEPROM

#ifdef ECONOMIC_VERSION
#define VERSION "Version E.3"
#else
#define VERSION "Version 0.3"
#endif

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
TIM_HandleTypeDef htim3; //Paused Cycle handler, ISR @ 10ms
TIM_HandleTypeDef htim4; //Used for generic delay proposes, PSC@274, Period of 0.000003806
IWDG_HandleTypeDef hiwdg;

enum Sensors
{
	_BH1750,
	_TSL2561
}Sensor;

enum ISR
{
	MCU_Reset,
	Menu,
	None
}volatile ISR;

typedef enum PlotType
{
	BothAxis,
	YAxis
}PlotType;

typedef enum Buttons
{
	Up = 0b11110,
	Down = 0b11101,
	Right = 0b11011,
	Left = 0b10111,
	Ok = 0b01111
}Buttons;

typedef enum Mode
{
	Continuous,
	Hold,
	Plot,
	Config_Plot,
	Select_Sensor,
	Reset_Sensor,
	Idle,
	Select_Diode,
}Modes;

struct Errors
{
	bool EEPROM_Fatal;
	bool BH1750_Fatal;
	bool BH1750_NoConn;
}Errors;

struct Configs
{
	uint8_t Factory_Values;
	Modes Mode;
	Modes Last_Mode;
	BH1750_Resolutions Resolution;
}Configs;

struct YAxisPosition
{
	uint16_t HigherRes;
	uint16_t ThreeQuartersRes;
	uint16_t MiddleRes;
	uint16_t QuarterRes;
	char HigherBuffer[7];
	char ThreeQuarterBuffer[7];
	char MiddleBuffer[7];
	char QuarterBuffer[7];
}static YAxisPosition =
{
	.HigherRes = DefaultResolution,
	.ThreeQuartersRes = DefaultResolution * 0.75,
	.MiddleRes = DefaultResolution * 0.5,
	.QuarterRes = DefaultResolution * 0.25
};

typedef struct PlotConfigs
{
	PlotType PlotType;
	uint16_t SampleTime;
	uint16_t Resolution;
	bool PrintLegends;
}PlotConfigs;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_IWDG_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
void Continous_mode(void);
void Hold_mode(void);
void Plot_mode(void);
void SSD1306_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, SSD1306_COLOR_t color);
void Print_OkToContinue(uint16_t XOffset, uint16_t YLimit);

void Config_plot_mode(void);
void Config_PlotSelectAnim(char *string, uint16_t CoordinateX, uint16_t CoordinateY);

void Reset_sensor_mode(void);
void Flash_configs(void);
void MenuGUI(void);
void MCU_Reset_Subrutine(void);
void Fatal_Error_EEPROM(void);
void Fatal_Error_BH1750(void);
void NoConnected_BH1750(void);
void Select_animation(char String[], uint16_t x, uint16_t y);
void Print_Measure(float Measure, uint16_t x, uint16_t y);
void wait_until_press(Buttons Button);
void Timer_Delay_250ms(uint16_t Value);
void Timer_Delay_50ms(uint16_t Value);
void Timer_Delay_at_274PSC(uint16_t Counts, uint16_t Overflows); //Period of 0.000003806
void SensorRead(void);
void Errors_init();
void Configs_init(void);
void CharNumberFromFloat(float Number, uint16_t DecimalsToConsider, uint16_t CountStringFinisher, uint16_t *NumberOfIntegers, uint16_t *NumberOfDecimals);
uint16_t CharsNumberFromInt(uint32_t Number, uint16_t CountFinisherChar);
uint16_t NumberOfCharsUsed(char *String, uint16_t CountFinisherChar);
uint16_t CenterXPrint(char *string, uint16_t InitialCoordinate, uint16_t LastCoordinate, FontDef_t Font);

#ifdef USER_DEBUG
volatile bool PauseFlag = true;
#endif

const PlotConfigs DefaultPlotSettings = {
		.PlotType = BothAxis,
		.SampleTime = DefaultSampleTime,
		.Resolution = DefaultResolution,
		.PrintLegends = true
};

PlotConfigs GlobalConfigs  = {
		.PlotType = DefaultPlotSettings.PlotType,
		.SampleTime = DefaultPlotSettings.SampleTime,
		.Resolution = DefaultPlotSettings.Resolution,
		.PrintLegends = DefaultPlotSettings.PrintLegends
};

const char Slots[5][7] = {"Slot 1", "Slot 2", "Slot 3", "Slot 5", "Slot 6"};
float Measure;
Rojo_BH1750 BH1750;
uint16_t IDR_Read;
uint8_t Config_buffer[2]; /*Solve here*/
bool comeFromMenu = false;

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_IWDG_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  HAL_IWDG_Init(&hiwdg);
  SSD1306_Init();
  Configs_init();
  //Initial Prints
#ifdef SHOW_LOADING
  SSD1306_GotoXY(7, 5);
  SSD1306_Puts("Loading", &Font_16x26, 1);
#endif
#ifdef ONE_SENSOR
  //Temporal asignation
  Sensor = _BH1750;
  //Temporal asignation
#endif

  ISR = None;
  //Version declaration
  SSD1306_GotoXY(7, 20);
  SSD1306_Puts("Firmware Version", &Font_7x10, 1);
  SSD1306_GotoXY(3, 37);
  SSD1306_Puts(VERSION, &Font_11x18, 1);
  SSD1306_UpdateScreen();
  HAL_IWDG_Refresh(&hiwdg);
  switch(Sensor)
  {
  	  case _BH1750:
  		  if(BH1750_Init(&BH1750, &hi2c2, Address_Low) != Rojo_OK)
  			  NoConnected_BH1750();
	  break;
	  case _TSL2561:
	  break;
  }
  //EEPROM Check & Configurations Read
#ifndef ECONOMIC_VERSION
  if(HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 0x0, 1, &Configs.Factory_Values, 1, 100) != HAL_OK)
	  Fatal_Error_EEPROM();
  if(Errors.EEPROM_Fatal || Configs.Factory_Values)
	  Flash_configs(); //Start by the FLASH configurations
  //Code here the backup EEPROM settings
  else if(!Configs.Factory_Values)
  {
	  if(HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 0x1, 1, Config_buffer, 2/*@TODO Change the neccesary buffer*/, 100) != HAL_OK)
		  Fatal_Error_EEPROM();
	  HAL_IWDG_Refresh(&hiwdg);
	  /*@TODO Check all the configurations*/
	  Config_buffer[0] = Configs.Mode;
	  Config_buffer[1] = Configs.Resolution;
  }
#endif
  //Final
  HAL_IWDG_Refresh(&hiwdg);
  HAL_TIM_Base_Start(&htim4);
  Timer_Delay_250ms(Seconds(0.5f));
#ifndef SHOW_LOADING
  Timer_Delay_250ms(Seconds(1.5f));
#endif
  //Final Clear
  SSD1306_Clear();
  SSD1306_UpdateScreen();
  //Starting the paused cycle handler
  HAL_TIM_Base_Start_IT(&htim3);
  while (1)
  {
	  //Check ISR's
	  switch(ISR)
	  {
	  	  case Menu:
	  		  ISR = None;
	  		  comeFromMenu = true;
	  		  MenuGUI();
	  	  break;
	  	  case MCU_Reset:
	  		  ISR = None;
	  		  MCU_Reset_Subrutine();
	  	  break;
	  	  default:
	  	  break;
	  }
	  //Check & Run the mode
#ifdef USER_PLOT_DEBUG
	  Configs.Mode = Plot;
#elif defined(USER_CONF_P_DEBUG)
	  Configs.Mode = Config_Plot;
#endif
	  switch(Configs.Mode)
	  {
	  	  case Continuous: //Basic Software mode
	  		  Continous_mode();
	  	  break;
	  	  case Hold: //Basic Software mode
	  		  Hold_mode();
	  	  break;
	  	  case Reset_Sensor: //Basic Software mode
	  		  Reset_sensor_mode();
		  break;
	  	  case Plot: //Basic Software mode
	  		  Plot_mode();
	  	  break;
	  	  case Config_Plot: //Basic Software mode
	  		  Config_plot_mode();
	  	  break;
	  	  case Select_Sensor: //IR Software mode
	  	  break;
	  	  case Select_Diode: //IR Software mode
	  	  break;
	  	  case Idle:
	  	  break;
	  }
	  HAL_IWDG_Refresh(&hiwdg);
	  //Parsed Loop
#ifdef USER_DEBUG
	  while(PauseFlag)
		  HAL_IWDG_Refresh(&hiwdg);
	  PauseFlag = true;
#else
	  HAL_SuspendTick();
	  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
	  HAL_ResumeTick();
#endif
  }
}

//Basic software modes
void Continous_mode(void)
{
	HAL_IWDG_Refresh(&hiwdg);
	if(Configs.Last_Mode != Continuous || comeFromMenu)
	{
		SSD1306_Clear();
		SSD1306_GotoXY(36, 8);
		SSD1306_Puts("Valor", &Font_11x18, 1);
		SSD1306_GotoXY(28, 53);
		SSD1306_Puts("Continuous", &Font_7x10, 1);
		SSD1306_UpdateScreen();
	}
	SensorRead();
	HAL_IWDG_Refresh(&hiwdg);
	Print_Measure(Measure, 14, 30);
	Configs.Last_Mode = Continuous;
	comeFromMenu = false;
}

void Hold_mode(void)
{
	if(Configs.Last_Mode != Hold || comeFromMenu)
	{
		SSD1306_Clear();
		SSD1306_GotoXY(36, 8);
		SSD1306_Puts("Valor", &Font_11x18, 1);
		SSD1306_GotoXY(43, 53);
		SSD1306_Puts("Hold", &Font_7x10, 1);
		SSD1306_UpdateScreen();
	}
	SensorRead();
	Print_Measure(Measure, 14, 30);
	wait_until_press(Ok);
	Configs.Last_Mode = Hold;
	comeFromMenu = false;
}


//@TODO Initial configurations done, print in sequence time, do first the config menu
void Plot_mode(void)
{
	static bool ChangedConfigs = false; //Checks if the user has pressed a button
	const uint16_t XAxis_High = 57; //Defines the high of the axis
	const uint16_t YScreenRes = 63; //Total screen pixel in y axis
	uint16_t YAxis_Offset;      //Defines the pixels at the right for the axis print
	uint16_t XAxis_Limit;       //Defines the right limit (near the "t")
	uint16_t YAxis_LimitUP;     //Defines the upper limit
	uint16_t HigherYcoordenate; //The higher coordinate that can be plotted
	uint16_t NumberOfChars;
	uint16_t YLimit;

	HAL_IWDG_Refresh(&hiwdg);
	if(Configs.Last_Mode != Plot || comeFromMenu || ChangedConfigs)
	{
		//Calculate the Y Axis offset
		NumberOfChars = CharsNumberFromInt(YAxisPosition.HigherRes, false);
		YAxis_Offset = (NumberOfChars * 7) + 5;
		YAxis_LimitUP = 0;
		XAxis_Limit = 128;
		SSD1306_Clear();
		//X Axis
		if(GlobalConfigs.PlotType == BothAxis)
		{
			if(GlobalConfigs.PrintLegends)
			{
				XAxis_Limit = 119;
				SSD1306_GotoXY(120, 53);
				SSD1306_Puts("t", &Font_7x10, 1);
			}
			SSD1306_DrawLine(0, XAxis_High, XAxis_Limit, XAxis_High, 1);
			//X Arrow
			SSD1306_DrawFilledTriangle(XAxis_Limit-5, XAxis_High-3, XAxis_Limit-5, XAxis_High+3, XAxis_Limit, XAxis_High, 1);
		}
		//Y Axis
		if(GlobalConfigs.PrintLegends)
		{
			YAxis_LimitUP = 11;
			SSD1306_GotoXY(YAxis_Offset - 7, 0);
			SSD1306_Puts("lx", &Font_7x10, 1);
		}
		SSD1306_DrawLine(YAxis_Offset, XAxis_High, YAxis_Offset, YAxis_LimitUP, 1);
		HigherYcoordenate = YAxis_LimitUP + 10;
		//Y Axis numeric legends -- Forced, not touched by the user
		if(!GlobalConfigs.PrintLegends) // Print all values
		{
			//Text prints
			sprintf(YAxisPosition.ThreeQuarterBuffer, "%d", (int) YAxisPosition.ThreeQuartersRes);
			sprintf(YAxisPosition.QuarterBuffer, "%d", (int) YAxisPosition.QuarterRes);
			SSD1306_GotoXY(0, (((YScreenRes - HigherYcoordenate) * 0.25) + HigherYcoordenate) - 5);
			SSD1306_Puts(YAxisPosition.ThreeQuarterBuffer, &Font_7x10, 1);
			SSD1306_GotoXY(0, (((YScreenRes - HigherYcoordenate) * 0.75) + HigherYcoordenate) - 5);
			SSD1306_Puts(YAxisPosition.QuarterBuffer, &Font_7x10, 1);
			//Line prints
			SSD1306_DrawLine(YAxis_Offset, (((YScreenRes - HigherYcoordenate) * 0.25) + HigherYcoordenate), (CharsNumberFromInt(YAxisPosition.ThreeQuartersRes, false) * 7) + 1, (((YScreenRes - HigherYcoordenate) * 0.25) + HigherYcoordenate), 1); //ThreeQuarter Line
			SSD1306_DrawLine(YAxis_Offset, (((YScreenRes - HigherYcoordenate) * 0.75) + HigherYcoordenate), (CharsNumberFromInt(YAxisPosition.QuarterRes, false) * 7) + 1, (((YScreenRes - HigherYcoordenate) * 0.75) + HigherYcoordenate), 1); //Quarter Line
			//Text prints
			sprintf(YAxisPosition.HigherBuffer, "%d", (int) YAxisPosition.HigherRes);
			sprintf(YAxisPosition.MiddleBuffer, "%d", (int) YAxisPosition.MiddleRes);
			SSD1306_GotoXY(0, HigherYcoordenate - 5);
			SSD1306_Puts(YAxisPosition.HigherBuffer, &Font_7x10, 1);
			SSD1306_GotoXY(0, (((YScreenRes - HigherYcoordenate) / 2) + HigherYcoordenate) - 5);
			SSD1306_Puts(YAxisPosition.MiddleBuffer, &Font_7x10, 1);
			///Line Prints
			SSD1306_DrawLine(YAxis_Offset, HigherYcoordenate, (CharsNumberFromInt(YAxisPosition.HigherRes, false) * 7) + 1, HigherYcoordenate, 1); //Higher Line
			SSD1306_DrawLine(YAxis_Offset, (((YScreenRes - HigherYcoordenate) / 2) + HigherYcoordenate), (CharsNumberFromInt(YAxisPosition.MiddleRes, false) * 7) + 1, (((YScreenRes - HigherYcoordenate) / 2) + HigherYcoordenate), 1); //Middle Line
		}
		else
		{
			//Text prints
			sprintf(YAxisPosition.HigherBuffer, "%d", (int) YAxisPosition.HigherRes);
			sprintf(YAxisPosition.MiddleBuffer, "%d", (int) YAxisPosition.MiddleRes);
			SSD1306_GotoXY(0, HigherYcoordenate - 5);
			SSD1306_Puts(YAxisPosition.HigherBuffer, &Font_7x10, 1);
			SSD1306_GotoXY(0, (((XAxis_High - HigherYcoordenate) / 2) + HigherYcoordenate) - 5);
			SSD1306_Puts(YAxisPosition.MiddleBuffer, &Font_7x10, 1);
			///Line Prints
			SSD1306_DrawLine(YAxis_Offset, HigherYcoordenate, (CharsNumberFromInt(YAxisPosition.HigherRes, false) * 7) + 1, HigherYcoordenate, 1); //Higher Line
			SSD1306_DrawLine(YAxis_Offset, (((XAxis_High - HigherYcoordenate) / 2) + HigherYcoordenate), (CharsNumberFromInt(YAxisPosition.MiddleRes, false) * 7) + 1, (((XAxis_High - HigherYcoordenate) / 2) + HigherYcoordenate), 1); //Middle Line
		}
		//Y Arrow
		SSD1306_DrawFilledTriangle(YAxis_Offset-3, YAxis_LimitUP+5, YAxis_Offset+3, YAxis_LimitUP+5, YAxis_Offset, YAxis_LimitUP, 1);
		HAL_IWDG_Refresh(&hiwdg);
		SSD1306_UpdateScreen();
		switch(GlobalConfigs.PlotType)
		{
			case BothAxis:
				YLimit = XAxis_High;
			break;
			case YAxis:
				YLimit = YScreenRes;
			break;
		}
		Print_OkToContinue(YAxis_Offset, YLimit);
	}

	//Do the magic :D

	Configs.Last_Mode = Plot;
	comeFromMenu = false;
	HAL_IWDG_Refresh(&hiwdg);
	//Read value and put a point
}

//Plot Functions
void Print_OkToContinue(uint16_t XOffset, uint16_t YLimit)
{
	const uint16_t CharsX = 56;
	const uint16_t XCoordinate = (((128-CharsX) + XOffset)/2);
	const uint16_t CharsY = 33;
	const uint16_t CharYDim = 10;
	const uint16_t YPixelStep = 1;
	const uint16_t YInitialCoordinate = (YLimit - CharsY) / 2;

	SSD1306_GotoXY(XCoordinate, YInitialCoordinate);
	SSD1306_Puts("Press OK", &Font_7x10, 1);
	SSD1306_GotoXY(XCoordinate, YInitialCoordinate + (CharYDim) + YPixelStep);
	SSD1306_Puts("to start", &Font_7x10, 1);
	SSD1306_GotoXY(XCoordinate, YInitialCoordinate + (CharYDim*2) + YPixelStep);
	SSD1306_Puts("the plot", &Font_7x10, 1);
	SSD1306_UpdateScreen();
	wait_until_press(Ok);
	SSD1306_GotoXY(XCoordinate, YInitialCoordinate);
	SSD1306_Puts("        ", &Font_7x10, 1);
	SSD1306_GotoXY(XCoordinate, YInitialCoordinate + (CharYDim) + YPixelStep);
	SSD1306_Puts("        ", &Font_7x10, 1);
	SSD1306_GotoXY(XCoordinate, YInitialCoordinate + (CharYDim*2) + YPixelStep);
	SSD1306_Puts("        ", &Font_7x10, 1);
	SSD1306_UpdateScreen();
}

//@TODO Solve cursor bugs, all the other stages
void Config_plot_mode(void)
{
	typedef enum ConfigStage
	{
		none,
		Selecting,
		Resolution,
		SampleTime,
		Graphic
	}ConfigStage;

	static PlotConfigs LocalBuffers = {
			.PlotType = DefaultPlotSettings.PlotType,
			.SampleTime = DefaultPlotSettings.SampleTime,
			.Resolution = DefaultPlotSettings.Resolution,
			.PrintLegends = DefaultPlotSettings.PrintLegends
	};

	struct GeneralBuffers
	{
		char *ResBuffer;
		char *SampleBuffer;
		char Units[3];
		const char ResolutionPrint[10];
		const char SamplePrint[10];
		const char GraphicPrint[10];

	}GeneralBuffers = {
			.Units = "ms",
			.ResolutionPrint = "Res",
			.SamplePrint = "Sample",
			.GraphicPrint = "Graphic"
	};

	const uint16_t XOffset = 10; //Minimum of 2
	const uint16_t ResY = 13;
	const uint16_t SampleY = 25;
	const uint16_t GraphicY = 37;
	static ConfigStage CurrentStage = Selecting;
	static ConfigStage Cursor = Resolution;
	static bool EnteredGraphic = false;
	static bool CursorMoved = false;
	uint32_t Past_IDR_Read = 0xFF;
	bool ReprintInitialPrint = false;
	bool NotReady = true;

	HAL_IWDG_Refresh(&hiwdg);
	if(Configs.Last_Mode != Config_Plot || comeFromMenu || ReprintInitialPrint)
	{
		SSD1306_Clear();
		LocalBuffers.PlotType = GlobalConfigs.PlotType;
		LocalBuffers.SampleTime = GlobalConfigs.SampleTime;
		LocalBuffers.Resolution = GlobalConfigs.Resolution;
		LocalBuffers.PrintLegends = GlobalConfigs.Resolution;
		GeneralBuffers.ResBuffer = (char *) calloc(CharsNumberFromInt(LocalBuffers.Resolution, false), sizeof(char));

		if(GeneralBuffers.ResBuffer == NULL)
		{
			//Send error message || Code error 0xAF
			SSD1306_Clear();
			SSD1306_GotoXY(CenterXPrint("Fatal Error, code: 0xAF", 0, 128, Font_11x18), 20);
			SSD1306_Puts("Fatal Error, code: 0xAF", &Font_11x18, 1);
			SSD1306_UpdateScreen();
			return;
		}
		else
		{
			sprintf(GeneralBuffers.ResBuffer, "%d", (int) LocalBuffers.Resolution);
		}

		GeneralBuffers.SampleBuffer = (char *) calloc(CharsNumberFromInt(LocalBuffers.SampleTime, false), sizeof(char));

		if(GeneralBuffers.SampleBuffer == NULL)
		{
			//Send error message || Code error 0xAA
			SSD1306_Clear();
			SSD1306_GotoXY(CenterXPrint("Fatal Error, code: 0xAA", 0, 128, Font_11x18), 20);
			SSD1306_Puts("Fatal Error, code: 0xAA", &Font_11x18, 1);
			SSD1306_UpdateScreen();
			return;
		}
		else
		{
			sprintf(GeneralBuffers.SampleBuffer, "%d", (int) LocalBuffers.SampleTime);
		}
		HAL_IWDG_Refresh(&hiwdg);
		//List of configurations
		SSD1306_GotoXY(XOffset, ResY);
		SSD1306_Puts((char *) GeneralBuffers.ResolutionPrint, &Font_7x10, 1);
		SSD1306_GotoXY(XOffset, SampleY);
		SSD1306_Puts((char *) GeneralBuffers.SamplePrint, &Font_7x10, 1);
		SSD1306_GotoXY(XOffset, GraphicY);
		SSD1306_Puts((char *) GeneralBuffers.GraphicPrint, &Font_7x10, 1);
		//Value Selected
		SSD1306_GotoXY(XOffset + (NumberOfCharsUsed((char *) GeneralBuffers.ResolutionPrint, false) * 7) + 5, 13);
		SSD1306_Puts(GeneralBuffers.ResBuffer, &Font_7x10, 1);
		SSD1306_GotoXY(XOffset + (NumberOfCharsUsed((char *) GeneralBuffers.SamplePrint, false) * 7) + 5, 25);
		SSD1306_Puts(GeneralBuffers.SampleBuffer, &Font_7x10, 1);
		SSD1306_UpdateScreen();
	}
	//Start the configuration
	HAL_IWDG_Refresh(&hiwdg);
	switch(CurrentStage)
	{
		case Selecting:
			do
			{
				HAL_IWDG_Refresh(&hiwdg);
				IDR_Read = (GPIOA -> IDR & ReadMask);
				if(Past_IDR_Read != IDR_Read)
				{
					switch(IDR_Read)
					{
						case Up:
							Cursor--;
							if(Cursor < Resolution)
								Cursor = Resolution;
							else
								CursorMoved = true;
						break;
						case Down:
							Cursor++;
							if(Cursor > Graphic)
								Cursor = Graphic;
							else
								CursorMoved = true;
						break;
						case Ok:
							CurrentStage = Cursor;
							if(Cursor == Graphic)
								EnteredGraphic = true;
							NotReady = false;
							if(Cursor == Graphic)
							{
							}
								//@TODO Select animation
						break;
						case Right:
							CurrentStage = Cursor;
							if(Cursor == Graphic)
								EnteredGraphic = true;
							NotReady = false;
							if(Cursor == Graphic)
							{

							}
						break;
						default:
						break;
					}
					//Printing cursor
					switch(Cursor)
					{
						case Resolution:
							if(CursorMoved)
							{
								//Erase Sample Rectangle
								SSD1306_DrawRectangle(XOffset - 2, SampleY - 1, (NumberOfCharsUsed((char *) GeneralBuffers.SamplePrint, false) * 7) + 1, 11, 0);
								CursorMoved = false;
							}
							else //Draw rectangle
								SSD1306_DrawRectangle(XOffset - 2, ResY - 3, (NumberOfCharsUsed((char *) GeneralBuffers.ResolutionPrint, false) * 7) + 3, 13, 1);
						break;
						case SampleTime:
							if(CursorMoved)
							{
								switch(IDR_Read)
								{
									case Up:
										//Erase Graphic
										SSD1306_DrawRectangle(XOffset - 2, GraphicY - 1, (NumberOfCharsUsed((char *) GeneralBuffers.GraphicPrint, false) * 7) + 1, 11, 0);
									break;
									case Down:
										//Erase Resolution
										SSD1306_DrawRectangle(XOffset - 2, ResY - 3, (NumberOfCharsUsed((char *) GeneralBuffers.ResolutionPrint, false) * 7) + 3, 13, 0);
									break;
								}
								CursorMoved = false;
							}

							else //Draw rectangle
								SSD1306_DrawRectangle(XOffset - 2, SampleY - 1, (NumberOfCharsUsed((char *) GeneralBuffers.SamplePrint, false) * 7) + 1, 11, 1);
						break;
						case Graphic:
							if(CursorMoved)
							{
								//Erase Sample
								SSD1306_DrawRectangle(XOffset - 2, SampleY - 1, (NumberOfCharsUsed((char *) GeneralBuffers.SamplePrint, false) * 7) + 1, 11, 0);
								CursorMoved = false;
							}

							else //Draw rectangle
								SSD1306_DrawRectangle(XOffset - 2, GraphicY - 1, (NumberOfCharsUsed((char *) GeneralBuffers.GraphicPrint, false) * 7) + 1, 11, 1);
						break;
						default:
						break;
					}
					SSD1306_UpdateScreen();
					Timer_Delay_50ms(1);
				}
				else
					HAL_IWDG_Refresh(&hiwdg);
				Past_IDR_Read = IDR_Read;
			}while(NotReady && ISR == None);
		break;
		case Resolution:
			//Erase Resolution
			SSD1306_DrawRectangle(XOffset - 2, ResY - 3, (NumberOfCharsUsed((char *) GeneralBuffers.ResolutionPrint, false) * 7) + 3, 13, 0);
			//Draw cursor on the number
			SSD1306_DrawRectangle(XOffset + (NumberOfCharsUsed((char *) GeneralBuffers.ResolutionPrint, false) * 7) - 2, ResY - 3, (NumberOfCharsUsed((char *) GeneralBuffers.ResBuffer, false) * 7) + 3, 13, 1);
			SSD1306_UpdateScreen();
		break;
		case SampleTime:
		break;
		case Graphic:
		break;
		default:
		break;
	}
	Configs.Last_Mode = Config_Plot;
	comeFromMenu = false;
	if(ISR == Menu)
	{
		free(GeneralBuffers.ResBuffer);
		free(GeneralBuffers.SampleBuffer);
	}
}

//Configuration plot functions
void Config_PlotSelectAnim(char *string, uint16_t CoordinateX, uint16_t CoordinateY)
{

}
//Configuration plot functions

//@TODO check error reset sensor mode
void Reset_sensor_mode(void)
{

	SSD1306_Clear();
	HAL_IWDG_Refresh(&hiwdg);
	switch(Sensor)
	{
		case _BH1750:
			if(BH1750_ReCalibrate(&BH1750) != Rojo_OK)
				Fatal_Error_BH1750();
		break;
		case _TSL2561:
		break;
	}
	SSD1306_GotoXY(29, 5);
	SSD1306_Puts("The sensor", &Font_7x10, 1);
	SSD1306_GotoXY(8, 17);
	SSD1306_Puts("has been reseted", &Font_7x10, 1);
	SSD1306_GotoXY(36, 29);
	SSD1306_Puts("Press OK", &Font_7x10, 1);
	SSD1306_GotoXY(25, 41);
	SSD1306_Puts("to continue", &Font_7x10, 1);
	SSD1306_UpdateScreen();
	wait_until_press(Ok);
	if(Configs.Last_Mode == Reset_Sensor)
		Configs.Last_Mode = Continuous;
	Configs.Mode = Configs.Last_Mode;
	Configs.Last_Mode = Reset_Sensor;
}

//@TODO All select sensor mode
void Select_sensor_mode(void);
//@TODO All select diode sensor mode
void Select_diode_mode(void);


void MenuGUI(void)
{
	bool Not_Filled = true;
	int16_t Mode_Displayed = Continuous;
	uint32_t Past_IDR_Read = 0xFF;
	const uint16_t animation_counts = 4;

	Timer_Delay_250ms(1);
	SSD1306_Clear();
	SSD1306_GotoXY(31, 5);
	SSD1306_Puts("Mode", &Font_16x26, 1);
	SSD1306_UpdateScreen();
	Mode_Displayed = Configs.Last_Mode;
	HAL_IWDG_Refresh(&hiwdg);
	do
	{
		HAL_IWDG_Refresh(&hiwdg);
		IDR_Read = (GPIOA -> IDR & ReadMask);
		//Displaying the selection
		if(Past_IDR_Read != IDR_Read)
		{
			switch(Mode_Displayed)
			{
				case Continuous:
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("            ", &Font_11x18, 1);
					SSD1306_GotoXY(8, 37);
					SSD1306_Puts("Continuous", &Font_11x18, 1);
				break;
				case Hold:
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("             ", &Font_11x18, 1);
					SSD1306_GotoXY(41, 37);
					SSD1306_Puts("Hold", &Font_11x18, 1);
				break;
#ifndef ECONOMIC_VERSION //Disabling the complete version modes
				case Plot:
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("             ", &Font_11x18, 1);
					SSD1306_GotoXY(41, 37);
					SSD1306_Puts("Plot", &Font_11x18, 1);
				break;
				case Config_Plot:
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("             ", &Font_11x18, 1);
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("Config Plot", &Font_11x18, 1);
				break;
				case Select_Sensor:
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("             ", &Font_11x18, 1);
					SSD1306_GotoXY(9, 37);
					SSD1306_Puts("Sel Sensor", &Font_11x18, 1);
				break;
#endif
				case Reset_Sensor:
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("             ", &Font_11x18, 1);
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("Reset Sense", &Font_11x18, 1);
				break;
				case Idle:
				break;
			}
			SSD1306_UpdateScreen();
			HAL_IWDG_Refresh(&hiwdg);
			//Reading for the selection
			switch(IDR_Read)
			{
				case Right:
					Mode_Displayed++;
#ifdef ECONOMIC_VERSION //Disabling the complete version modes
					if(Mode_Displayed == Plot)
						Mode_Displayed = Reset_Sensor;
#endif
					if(Mode_Displayed > Reset_Sensor)
						Mode_Displayed = Continuous;
				break;
				case Left:
					Mode_Displayed--;
#ifdef ECONOMIC_VERSION //Disabling the complete version modes
					if(Mode_Displayed >= Select_Sensor)
						Mode_Displayed = Hold;
#endif
					if(Mode_Displayed < Continuous)
						Mode_Displayed = Reset_Sensor;
				break;
				case Ok:
					Configs.Mode = Mode_Displayed;
					Not_Filled = false;
					HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, 0x1, 1, (uint8_t *) &Mode_Displayed, 1, 100);
					HAL_IWDG_Refresh(&hiwdg);
					for(uint16_t i = 0; i < animation_counts; i++)
					{
						switch(Mode_Displayed)
						{
							case Continuous:
								Select_animation("Continuous ", 8, 37);
							break;
							case Hold:
								Select_animation("Hold       ", 41, 37);
							break;
#ifndef ECONOMIC_VERSION //Disabling the complete version modes
							case Plot:
								Select_animation("Plot       ", 41, 37);
							break;
							case Config_Plot:
								Select_animation("Config Plot", 3, 37);
							break;
							case Select_Sensor:
								Select_animation("Sel Sensor ", 9, 37);
							break;
#endif
							case Reset_Sensor:
								Select_animation("Reset Sense", 3, 37);
							break;
							case Idle:
							break;
						}
					}
					Timer_Delay_250ms(1);
				break;
			}
		}
		else
			HAL_IWDG_Refresh(&hiwdg);
		Past_IDR_Read = IDR_Read;
	}while(Not_Filled && ISR != MCU_Reset);
	ISR = None;
}

//@TODO Code a fancy animation
void Select_animation(char String[], uint16_t x, uint16_t y)
{
	static uint32_t i = 0;
	extern const uint16_t animation_counts;
	HAL_IWDG_Refresh(&hiwdg);
	SSD1306_GotoXY(x, y);
	SSD1306_Puts(String, &Font_11x18, 1);
	SSD1306_UpdateScreen();
	//Timer_Delay_250ms(1);
	Timer_Delay_at_274PSC(30000, 1); //114.18ms
	SSD1306_GotoXY(3, 37);
	SSD1306_Puts("            ", &Font_11x18, 1);
	SSD1306_UpdateScreen();

	if(i == animation_counts)
	{

	}

}

//Error handlers
#ifndef ECONOMIC_VERSION
void Fatal_Error_EEPROM(void)
{
	if(!Errors.EEPROM_Fatal)
	{
		SSD1306_Clear();
		SSD1306_GotoXY(3, 18);
		SSD1306_Puts("Fatal Error: EEPROM", &Font_7x10, 1);
		SSD1306_GotoXY(6, 33);
		SSD1306_Puts("Press OK to continue", &Font_7x10, 1);
		SSD1306_UpdateScreen();
		HAL_IWDG_Refresh(&hiwdg);
		wait_until_press(Ok);
		Errors.EEPROM_Fatal = true;
		HAL_IWDG_Refresh(&hiwdg);
	}
}
#endif

void Fatal_Error_BH1750(void)
{
	if(!Errors.BH1750_Fatal)
	{
		SSD1306_Clear();
		SSD1306_GotoXY(3, 18);
		SSD1306_Puts("Fatal Error: EEPROM", &Font_7x10, 1);
		SSD1306_GotoXY(6, 33);
		SSD1306_Puts("Press OK to continue", &Font_7x10, 1);
		SSD1306_UpdateScreen();
		SSD1306_UpdateScreen();
		HAL_IWDG_Refresh(&hiwdg);
		wait_until_press(Ok);
		Errors.BH1750_Fatal = true;
		HAL_IWDG_Refresh(&hiwdg);
	}
}

//@TODO Bad prints, doesn't wait of the button ok
void NoConnected_BH1750(void)
{
	if(!Errors.BH1750_NoConn)
	{
		SSD1306_Clear();
		SSD1306_GotoXY(42, 10);
		SSD1306_Puts("BH1750", &Font_7x10, 1);
		SSD1306_GotoXY(21, 21);
		SSD1306_Puts("No Connected", &Font_7x10, 1);
		SSD1306_GotoXY(35, 36);
		SSD1306_Puts("Press OK", &Font_7x10, 1);
		SSD1306_GotoXY(25, 47);
		SSD1306_Puts("to continue", &Font_7x10, 1);
		SSD1306_UpdateScreen();
		//ISR = None;
		HAL_IWDG_Refresh(&hiwdg);
		wait_until_press(Ok);
		Errors.BH1750_NoConn = true;
		HAL_IWDG_Refresh(&hiwdg);
	}
}

//Auxiliar functions
//@TODO At Print_Measure print allways in the center, x left when big number
void Print_Measure(float Measure, uint16_t x, uint16_t y)
{
	char Integer_part[5];
	char Fraccional_part[3];
	uint32_t Integer_measure;
	uint32_t Fraccional_measure;

	Integer_measure = (uint32_t) Measure;
	Integer_measure = (uint32_t) Measure;
	Fraccional_measure = (uint32_t) ((Measure - Integer_measure) * 100);
	sprintf(Integer_part, "%d", (int)Integer_measure);
	sprintf(Fraccional_part, "%d", (int)Fraccional_measure);
	HAL_IWDG_Refresh(&hiwdg);

	SSD1306_GotoXY(x, y);
	SSD1306_Puts("         ", &Font_11x18, 1);
	SSD1306_GotoXY(x, y);
	SSD1306_Puts(Integer_part, &Font_11x18, 1);
	SSD1306_Putc('.', &Font_11x18, 1);
	SSD1306_Puts(Fraccional_part, &Font_11x18, 1);
	SSD1306_Puts("lx", &Font_11x18, 1);
	HAL_IWDG_Refresh(&hiwdg);
	SSD1306_UpdateScreen();
}

void wait_until_press(Buttons Button)
{
	do{
		IDR_Read = (GPIOA -> IDR & ReadMask);
		HAL_IWDG_Refresh(&hiwdg);
	}while(IDR_Read != Button && ISR == None);
}

void Errors_init()
{
	bool *Pointer = &Errors.EEPROM_Fatal;
	for(uint16_t i = 0; i < sizeof(Errors); i++)
	{
		*Pointer = false;
		Pointer++;
		if(i == sizeof(Errors)/2)
			HAL_IWDG_Refresh(&hiwdg);
	}
}

void Timer_Delay_250ms(uint16_t Value)
{
	Timer_Delay_at_274PSC(EndOfCounts250ms, Value);
}

void Timer_Delay_50ms(uint16_t Value)
{
	Timer_Delay_at_274PSC(EndOfCounts50ms, Value);
}

void Timer_Delay_at_274PSC(uint16_t Counts, uint16_t Overflows) //Period of 0.000003806
{
	if(Overflows == 0)
		Overflows++;
	bool Time_not_reached = true;
	uint32_t i = 0;
	__HAL_TIM_SET_COUNTER(&htim4, 0);
	do{
		HAL_IWDG_Refresh(&hiwdg);
		if(__HAL_TIM_GET_COUNTER(&htim4) == Counts)
		{
			__HAL_TIM_SET_COUNTER(&htim4, 0);
			i++;
		}
		if(i == Overflows)
			Time_not_reached = false;
	}while(Time_not_reached);
}

void MCU_Reset_Subrutine(void)
{
	SSD1306_Clear();
	SSD1306_GotoXY(23, 17);
	SSD1306_Puts("Reset", &Font_16x26, 1);
	SSD1306_UpdateScreen();
	Timer_Delay_250ms(Seconds(1.5f));
	NVIC_SystemReset(); //Reset de MCU
}

void SensorRead(void)
{
	switch(Sensor)
	{
		case _BH1750:
			if(BH1750_Read(&BH1750, &Measure) != Rojo_OK) //Saving the value into a global
				NoConnected_BH1750();
		break;
		case _TSL2561:
		break;
	}
}

uint16_t CenterXPrint(char *string, uint16_t InitialCoordinate, uint16_t LastCoordinate, FontDef_t Font)
{
	uint16_t Chars = NumberOfCharsUsed(string, 0);

	Chars *= Font.FontWidth;

	return (((LastCoordinate - Chars) + InitialCoordinate) / 2);
}

uint16_t CharsNumberFromInt(uint32_t Number, uint16_t CountStringFinisher)
{
    uint16_t NumberOfChars = 0;

    while (Number > 0)
    {
        Number /= 10;
        NumberOfChars++;
    }
    if(CountStringFinisher)
        NumberOfChars++;
    return NumberOfChars;
}

void CharNumberFromFloat(float Number, uint16_t DecimalsToConsider, uint16_t CountStringFinisher, uint16_t *NumberOfIntegers, uint16_t *NumberOfDecimals)
{
	uint32_t IntegerPart, DecimalPart;
	uint16_t Multiplier = 1;
	uint16_t Integers = 0, Decimals = 0;
	for(uint16_t i = 0; i > DecimalsToConsider; i++)
	{
		Multiplier *= 10;
	}
	IntegerPart = (uint32_t) Number;
	DecimalPart = (Number - IntegerPart) * Multiplier;
	while(IntegerPart > 0)
	{
		IntegerPart /= 10;
		Integers++;
	}
	if(CountStringFinisher)
		Integers++;
	while(DecimalPart > 0)
	{
		DecimalPart /= 10;
		Decimals++;
	}
	if(CountStringFinisher)
		Decimals++;

	*NumberOfIntegers = Integers;
	*NumberOfDecimals = Decimals;
}

uint16_t NumberOfCharsUsed(char *String, uint16_t CountStringFinisher)
{
    uint16_t NumChars = 0;
    while(*String != 0)
    {
        NumChars++;
        String++;
    }
    if(CountStringFinisher)
        NumChars++;
    return NumChars;
}


//Configurations
void Configs_init(void)
{
	Configs.Factory_Values = 0;
	Configs.Last_Mode = Idle;
	Configs.Mode = Continuous;
	Configs.Resolution = Medium_Res;
}

//@TODO Flash configurations
void Flash_configs(void)
{

}

//ISR Handlers
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == GPIO_PIN_0)
	{
		if(ISR == None)
			ISR = Menu;
	}
	if(GPIO_Pin == GPIO_PIN_1)
		ISR = MCU_Reset;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
#ifdef USER_DEBUG
	//Breaks the while in the main function
	PauseFlag = false;
#endif
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_ErrorCallback could be implemented in the user file
   */
}

void Borrame(void)
{
	HAL_IWDG_Refresh(&hiwdg);
	IDR_Read = (GPIOA -> IDR & ReadMask);
	switch(IDR_Read)
	{
		case Up:
		break;
		case Down:
		break;
		case Right:
		break;
		case Left:
		break;
		case Ok:
		break;
	}
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 400000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
  hiwdg.Init.Reload = 4000;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 10;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65454;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 274;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : Arriba_Pin Abajo_Pin Derecha_Pin Izquierda_Pin
                           Ok_Pin WP_Pin */
  GPIO_InitStruct.Pin = Arriba_Pin|Abajo_Pin|Derecha_Pin|Izquierda_Pin
                          |Ok_Pin|WP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : Menu_IT_Pin Reset_IT_Pin */
  GPIO_InitStruct.Pin = Menu_IT_Pin|Reset_IT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
