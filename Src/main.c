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
    SystemCoreClock = 72000000;
    // HSE = 8 MHz
    /* ============ 72 MHz ============= */
    RCC->CFGR &= ~RCC_CFGR_SW; // Change System Clock to HSI
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0x00) {
        __NOP();
    };
    RCC->CR &= ~RCC_CR_PLLON; // Disable Pll
    while ((RCC->CR & RCC_CR_PLLRDY)) {
        __NOP();
    };
    RCC->CFGR &= ~0x3C0000;
    RCC->CFGR |= RCC_CFGR_PLLMULL9; // Pll Mul: x9 - 72MHz, x6 - 48MHz
//    RCC->CFGR |= RCC_CFGR_USBPRE;   // USBPRE: 0 - x1.5, 1 - x1
    RCC->CFGR |= RCC_CFGR_PLLSRC;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLON)) {
        __NOP();
    };
    RCC->CFGR |= RCC_CFGR_SW_1; // Change System Clock to PLL
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_1) {
        __NOP();
    };

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    // PB9 PB10 dbg signal out
    GPIOB->CRH |= GPIO_CRH_MODE10 | GPIO_CRH_MODE9;   // mode OUT 50MHz
    GPIOB->CRH &= ~(GPIO_CRH_CNF10 | GPIO_CRH_CNF9);  // Push-pull

    GPIOB->ODR ^= GPIO_ODR_ODR9;
    DWT_Delay_ms(10);
    GPIOB->ODR ^= GPIO_ODR_ODR9;

    /* PB12 - LED. Output PP */
    GPIOB->CRH |= GPIO_CRH_MODE11_0;
    GPIOB->CRH &= ~GPIO_CRH_CNF11;
    GPIOB->CRH |= GPIO_CRH_MODE12_0;
    GPIOB->CRH &= ~GPIO_CRH_CNF12;

    /* PB13 - USB EN. Output PP */
    GPIOB->CRH |= GPIO_CRH_MODE13_0;
    GPIOB->CRH &= ~GPIO_CRH_CNF13;
    
    /* =========== TIM1 ========== */
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    TIM1->PSC = 2000 - 1;
    TIM1->ARR = 36000 - 1;
    TIM1->DIER |= TIM_DIER_UIE;
    TIM1->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
    NVIC_SetPriority(TIM1_UP_IRQn, 15);
    NVIC_EnableIRQ(TIM1_UP_IRQn);

    GPIOB->ODR &= ~GPIO_ODR_ODR13; //LOW
    for (int i = 0; i < 1000000; i++) {
        __NOP();
    };

    DWT_Init();
    USBLIB_Init();
    GPIOB->ODR |= GPIO_ODR_ODR13; //UP

#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        GPIOB->ODR ^= GPIO_ODR_ODR11;
        DWT_Delay_ms(500);
    }
}

void HardFault_Handler() {
    while(1);
}

void TIM1_UP_IRQHandler() {
    TIM1->SR &= ~TIM_SR_UIF;
    GPIOB->ODR ^= GPIO_ODR_ODR12;

//    if (_LineState.L) {      //App connected to the virtual port
//        USBLIB_Transmit((uint16_t *)"Welcome to the club!\r\n", 22);
//    } else {
//        USBLIB_Transmit((uint16_t *)"Bye bye!\r\n", 10);
//    }
}

void uUSBLIB_DataReceivedHandler(uint16_t *Data, uint16_t Length)
{
    USBLIB_Transmit(Data, Length);
}

void uUSBLIB_LineStateHandler(USBLIB_WByte LineState)
{
    if (LineState.L) {      //App connected to the virtual port
        _LineState = LineState;
        TIM1->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
    }
}
