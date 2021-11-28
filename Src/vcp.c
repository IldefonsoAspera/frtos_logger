/*
 * vcp.c
 *
 *  Created on: Nov 28, 2021
 *      Author: Ilde
 */


#include "vcp.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "main.h"


UART_HandleTypeDef*  mp_huart = NULL;

static uint8_t inputStreamBuffer[VCP_INPUT_BUFFER_SIZE];
StaticStreamBuffer_t inputStreamCb;
StreamBufferHandle_t inputStream;



void vcp_th(void const * argument)
{

    while(1)
    {
        uint8_t rxBuffer[16];
        uint32_t nChars;

        nChars = xStreamBufferReceive(inputStream, rxBuffer, sizeof(rxBuffer), HAL_MAX_DELAY);
        HAL_UART_Transmit(mp_huart, rxBuffer, nChars, HAL_MAX_DELAY);
        HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    }
}


void vcp_send(void* p_data, uint32_t length)
{
    xStreamBufferSend(inputStream, p_data, length, 0);
}


void vcp_init(UART_HandleTypeDef *p_huart)
{
    mp_huart = p_huart;
    inputStream = xStreamBufferCreateStatic(sizeof(inputStreamBuffer), 1, inputStreamBuffer, &inputStreamCb);
}