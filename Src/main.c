/* 
 * This file is part of the SaeWave RemoteSwitch (USB-CDC-CMSIS) 
 * distribution (https://github.com/saewave/STM32F103-USB-CDC-CMSIS).
 * Copyright (c) 2017 Samoilov Alexey.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "stm32f1xx.h"
#include "usblib.h"
#include "dwt.h"

USBLIB_WByte _LineState;

int main(void)
{
    DWT_Init();

    // ===== GPIO init =====
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    // PB9 PB10 dbg signal out
    GPIOB->CRH |= GPIO_CRH_MODE10 | GPIO_CRH_MODE9;   // mode OUT 50MHz
    GPIOB->CRH &= ~(GPIO_CRH_CNF10 | GPIO_CRH_CNF9);  // Push-pull

    // PB12 - LED. Output PP
    GPIOB->CRH |= GPIO_CRH_MODE11_0;
    GPIOB->CRH &= ~GPIO_CRH_CNF11;
    GPIOB->CRH |= GPIO_CRH_MODE12_0;
    GPIOB->CRH &= ~GPIO_CRH_CNF12;

    // PB13 - USB Enable. Output PP
    GPIOB->CRH |= GPIO_CRH_MODE13_0;
    GPIOB->CRH &= ~GPIO_CRH_CNF13;
    
    // =========== TIM1 ==========
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    TIM1->PSC = 2000 - 1;
    TIM1->ARR = 36000 - 1;
    TIM1->DIER |= TIM_DIER_UIE;
    TIM1->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
    NVIC_SetPriority(TIM1_UP_IRQn, 15);
    NVIC_EnableIRQ(TIM1_UP_IRQn);

    GPIOB->ODR &= ~GPIO_ODR_ODR13; // USB disable
    for (int i = 0; i < 1000000; i++) {
        __NOP();
    };

    USBLIB_Init();
    GPIOB->ODR |= GPIO_ODR_ODR13; // USB enable

#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        GPIOB->ODR ^= GPIO_ODR_ODR11;
        DWT_Delay_ms(500);
        USBLIB_Transmit((uint16_t *) "USBLIB_Transmit((uint16_t *) USBLIB_Transmit((uint16_t *) USBLIB_Transmit((uint16_t *)\r\n", 88);
    }
}

void HardFault_Handler() {
    while(1);
}

void TIM1_UP_IRQHandler() {
    TIM1->SR &= ~TIM_SR_UIF;
    GPIOB->ODR ^= GPIO_ODR_ODR12;

    if ((_LineState.L & 0x01) && USBLIB_ReadyToTransmit(2)) {      // App connected to the virtual port
        USBLIB_Transmit((uint16_t *) "Welcome to the club!\r\n", 22);
    }
}

void uUSBLIB_DataReceivedHandler(uint16_t *Data, uint16_t Length)
{
    USBLIB_Transmit(Data, Length);
}

void uUSBLIB_LineStateHandler(USBLIB_WByte LineState)
{
//    if (LineState.L)      // App connected to the virtual port
        _LineState = LineState;
}
