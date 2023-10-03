################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../SSD1306_Prints/SSD1396_Prints.c 

OBJS += \
./SSD1306_Prints/SSD1396_Prints.o 

C_DEPS += \
./SSD1306_Prints/SSD1396_Prints.d 


# Each subdirectory must supply rules for building sources it contributes
SSD1306_Prints/%.o SSD1306_Prints/%.su: ../SSD1306_Prints/%.c SSD1306_Prints/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I/home/lord448/Escritorio/Libraries/STM32/OLED/Inc -I/home/lord448/Escritorio/Libraries/STM32/Rojo_BH1750/Inc -I"/home/lord448/Escritorio/STM32 Workspaces/Luxometro/Luxometro/SSD1306_Prints" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-SSD1306_Prints

clean-SSD1306_Prints:
	-$(RM) ./SSD1306_Prints/SSD1396_Prints.d ./SSD1306_Prints/SSD1396_Prints.o ./SSD1306_Prints/SSD1396_Prints.su

.PHONY: clean-SSD1306_Prints

