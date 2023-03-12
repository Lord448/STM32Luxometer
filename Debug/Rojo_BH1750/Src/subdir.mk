################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/home/lord448/Escritorio/Libraries/STM32/Rojo_BH1750/Src/Rojo_BH1750.c 

OBJS += \
./Rojo_BH1750/Src/Rojo_BH1750.o 

C_DEPS += \
./Rojo_BH1750/Src/Rojo_BH1750.d 


# Each subdirectory must supply rules for building sources it contributes
Rojo_BH1750/Src/Rojo_BH1750.o: /home/lord448/Escritorio/Libraries/STM32/Rojo_BH1750/Src/Rojo_BH1750.c Rojo_BH1750/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I/home/lord448/Escritorio/Libraries/STM32/OLED/Inc -I/home/lord448/Escritorio/Libraries/STM32/Rojo_BH1750/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Rojo_BH1750-2f-Src

clean-Rojo_BH1750-2f-Src:
	-$(RM) ./Rojo_BH1750/Src/Rojo_BH1750.d ./Rojo_BH1750/Src/Rojo_BH1750.o ./Rojo_BH1750/Src/Rojo_BH1750.su

.PHONY: clean-Rojo_BH1750-2f-Src

