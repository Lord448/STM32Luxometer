/**
 *  EEPROM Memory Map
 *  Memory regions
 *  0x0000 - 0x0002 Menu Configurations
 *  Variables
 *	//Menu Configurations
 *	0x0000: Variable that contains if the system is in Factory Values : 8 bits
 *	0x0001: Variable Mode : 8 bits
 *	0x0002: Variable Resolutions : 8 bits
 *
 *	Version 0.0.3
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
#define CountsOfSelectAnim 0xFFFF //@274PSC:
#define SizeOfSlotsArray 5
#define Seconds(x) x*4 //Only valid for the Timer_Delay_250ms

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
TIM_HandleTypeDef htim4;
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

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_IWDG_Init(void);
static void MX_TIM4_Init(void);
void Continous_mode(void);
void Hold_mode(void);
void Plot_mode(void);
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
void Timer_Delay_at_274PSC(uint16_t Counts, uint16_t Overflows); //Period of 0.000003806
void SensorRead(void);
void Errors_init();
void Configs_init(void);

const char Slots[5][7] = {"Slot 1", "Slot 2", "Slot 3", "Slot 5", "Slot 6"};
float Measure;
Rojo_BH1750 BH1750;
uint32_t IDR_Read;
uint8_t Config_buffer[2]; /*Solve here*/

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_IWDG_Init();
  MX_TIM4_Init();
  HAL_IWDG_Init(&hiwdg);
  SSD1306_Init();
  Configs_init();
  //Initial Prints
  //SSD1306_GotoXY(7, 5);
  //SSD1306_Puts("Loading", &Font_16x26, 1);

  //Temporal asignation
  Sensor = _BH1750;
  //Temporal asignation

  SSD1306_GotoXY(3, 37);
  SSD1306_Puts("Version 0.3", &Font_11x18, 1);
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
  if(HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 0x0, 1, &Configs.Factory_Values, 1, 100) != HAL_OK)
	  Fatal_Error_EEPROM();
  if(Errors.EEPROM_Fatal || Configs.Factory_Values)
	  Flash_configs(); //Start by the FLASH configurations
  else if(!Configs.Factory_Values)
  {
	  if(HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 0x1, 1, Config_buffer, 2/*@TODO Change the neccesary buffer*/, 100) != HAL_OK)
		  Fatal_Error_EEPROM();
	  HAL_IWDG_Refresh(&hiwdg);
	  /*@TODO Check all the configurations*/
	  Config_buffer[0] = Configs.Mode;
	  Config_buffer[1] = Configs.Resolution;
  }
  //Final
  HAL_IWDG_Refresh(&hiwdg);
  HAL_TIM_Base_Start(&htim4);
  ISR = None;
  Timer_Delay_250ms(Seconds(1.5f));
  //Final Clear
  SSD1306_Clear();
  SSD1306_UpdateScreen();
  while (1)
  {
	  //Check ISR's
	  switch(ISR)
	  {
	  	  case Menu:
	  		  ISR = None;
	  		  MenuGUI();
	  		  //Configs.Last_Mode = Idle;
	  	  break;
	  	  case MCU_Reset:
	  		  ISR = None;
	  		  MCU_Reset_Subrutine();
	  	  break;
	  	  default:
	  	  break;
	  }
	  //Check & Run the mode
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
	  	  break;
	  	  case Select_Sensor: //IR Software mode
	  	  break;
	  	  case Select_Diode: //IR Software mode
	  	  break;
	  	  case Idle:
	  	  break;
	  }
	  HAL_IWDG_Refresh(&hiwdg);
  }
}

//Basic software modes
void Continous_mode(void)
{
	HAL_IWDG_Refresh(&hiwdg);
	if(Configs.Last_Mode != Continuous)
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
}

void Hold_mode(void)
{
	if(Configs.Last_Mode != Hold)
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
}

//@TODO all plot mode
void Plot_mode(void)
{
	SSD1306_Clear();
	HAL_IWDG_Refresh(&hiwdg);
	SSD1306_GotoXY(14, 18);
	SSD1306_Puts("Mode not ready", &Font_7x10, 1);
	SSD1306_GotoXY(6, 33);
	SSD1306_Puts("Press OK to continue", &Font_7x10, 1);
	SSD1306_UpdateScreen();
	wait_until_press(Ok);
	Configs.Mode = Continuous;
}

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
	Configs.Mode = Configs.Last_Mode;
	Configs.Last_Mode = Reset_Sensor;
}

//@TODO All select sensor mode
void Select_sensor_mode(void);
//@TODO All select diode sensor mode
void Select_diode_mode(void);
//@TODO Flash configurations
void Flash_configs(void)
{

}

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
				case Plot:
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("             ", &Font_11x18, 1);
					SSD1306_GotoXY(41, 37);
					SSD1306_Puts("Plot", &Font_11x18, 1);
				break;
				case Select_Sensor:
					SSD1306_GotoXY(3, 37);
					SSD1306_Puts("             ", &Font_11x18, 1);
					SSD1306_GotoXY(9, 37);
					SSD1306_Puts("Sel Sensor", &Font_11x18, 1);
				break;
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
					if(Mode_Displayed > Reset_Sensor)
						Mode_Displayed = Continuous;
				break;
				case Left:
					Mode_Displayed--;
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
								Select_animation("Continuous", 8, 37);
							break;
							case Hold:
								Select_animation("Hold      ", 41, 37);
							break;
							case Plot:
								Select_animation("Plot      ", 41, 37);
							break;
							case Select_Sensor:
								Select_animation("Sel Sensor", 9, 37);
							break;
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
	Timer_Delay_at_274PSC(30000, 1);
	SSD1306_GotoXY(3, 37);
	SSD1306_Puts("            ", &Font_11x18, 1);
	SSD1306_UpdateScreen();

	if(i == animation_counts)
	{

	}

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

//OLED Prints
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

void Fatal_Error_BH1750(void)
{
	if(!Errors.BH1750_Fatal)
	{
		SSD1306_Clear();
		SSD1306_GotoXY(3, 18);
		SSD1306_Puts("Fatal Error: BH1750", &Font_7x10, 1);
		SSD1306_GotoXY(6, 33);
		SSD1306_Puts("Press OK to continue", &Font_7x10, 1);
		SSD1306_UpdateScreen();
		HAL_IWDG_Refresh(&hiwdg);
		wait_until_press(Ok);
		Errors.BH1750_Fatal = true;
		HAL_IWDG_Refresh(&hiwdg);
	}
}

//@TODO error check in NoConnected sensor error function
void NoConnected_BH1750(void)
{
	if(!Errors.BH1750_NoConn)
	{
		SSD1306_Clear();
		SSD1306_GotoXY(3, 18);
		SSD1306_Puts("BH1750 No Connected", &Font_7x10, 1);
		SSD1306_GotoXY(6, 33);
		SSD1306_Puts("Press OK to continue", &Font_7x10, 1);
		SSD1306_UpdateScreen();
		//ISR = None;
		HAL_IWDG_Refresh(&hiwdg);
		wait_until_press(Ok);
		Errors.BH1750_NoConn = true;
		HAL_IWDG_Refresh(&hiwdg);
	}
}

//@TODO At Print_Measure print allways in the center
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

void SensorRead(void)
{
	switch(Sensor)
	{
		case _BH1750:
			if(BH1750_Read(&BH1750, &Measure) != Rojo_OK)
				NoConnected_BH1750();
		break;
		case _TSL2561:
		break;
	}
}

void Configs_init(void)
{
	Configs.Factory_Values = 0;
	Configs.Last_Mode = Idle;
	Configs.Mode = Continuous;
	Configs.Resolution = Medium_Res;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == GPIO_PIN_0)
		ISR = Menu;
	if(GPIO_Pin == GPIO_PIN_1)
		ISR = MCU_Reset;
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
