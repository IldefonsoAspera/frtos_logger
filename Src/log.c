
#include "log.h"

#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "main.h"
#include "cmsis_os.h"
#include "vcp.h"



#define LOG_ARRAY_N_ELEM(x)     (sizeof(x)/sizeof((x)[0]))


#if LOG_SUPPORT_ANSI_COLOR
#define LOG_ANSI_PREFIX         "\x1B["
#define LOG_ANSI_SUFFIX         'm'

static char ansi_colors[_LOG_COLOR_LEN][2] = {
    {'0', ' '},
    {'3', '0'},
    {'3', '1'},
    {'3', '2'},
    {'3', '3'},
    {'3', '4'},
    {'3', '5'},
    {'3', '6'},
    {'3', '7'},
};
#endif


typedef struct log_fifo_item_s
{
    uint32_t           data;
    uint16_t           str_len;
    enum log_data_type type;
#if LOG_SUPPORT_ANSI_COLOR
    enum log_color     color;
#endif
} log_fifo_item_t;


typedef struct log_fifo_s
{
    log_fifo_item_t buffer[LOG_INPUT_FIFO_N_ELEM];
    uint32_t wrIdx;
    uint32_t rdIdx;
    uint32_t nItems;
} log_fifo_t;



static log_fifo_t logFifo;



static inline void log_fifo_put(log_fifo_item_t *pItem, log_fifo_t *pFifo)
{
    uint32_t primask_bit;

    primask_bit = __get_PRIMASK();
    __disable_irq();

    if(pFifo->nItems < LOG_ARRAY_N_ELEM(pFifo->buffer))
    {
        pFifo->buffer[pFifo->wrIdx++] = *pItem;
        pFifo->wrIdx &= LOG_INPUT_FIFO_N_ELEM - 1;
        pFifo->nItems++;
    }

    __set_PRIMASK(primask_bit);
}


static inline bool log_fifo_get(log_fifo_item_t *pItem, log_fifo_t *pFifo)
{
    bool retVal = false;
    uint32_t primask_bit;

    primask_bit = __get_PRIMASK();
    __disable_irq();

    if(pFifo->nItems)
    {
        *pItem = pFifo->buffer[pFifo->rdIdx++];
        pFifo->rdIdx &= LOG_INPUT_FIFO_N_ELEM - 1;
        pFifo->nItems--;
        retVal = true;
    }

    __set_PRIMASK(primask_bit);
    return retVal;
}


static void log_fifo_reset(log_fifo_t *pFifo)
{
    pFifo->rdIdx  = 0;
    pFifo->wrIdx  = 0;
    pFifo->nItems = 0;
}


static inline uint8_t process_number_decimal(uint32_t number, char *output)
{
    uint32_t divider = 1000000000UL;
    uint8_t i;

    for(i=0; i<10; i++)
    {
        output[i] = 0x30 + number/divider;
        number %= divider;
        divider /= 10;
    }

    i = 0;
    while(i < 10 && output[i] == 0x30)
        i++;

    if(i == 10)     // Case when all digits are 0 should print one digit ('0')
        return 1;
    else            // Return number of digits to print without leading zeroes
        return 10-i;
}


static inline void process_number_hex(uint32_t number, char *output, uint8_t n_digits)
{
    char hexVals[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    int8_t i;

    for(i = sizeof(number)*2 - 1; i > -1; i--)
    {
        output[i] = hexVals[number & 0x0F];
        number >>= 4;
    }
}


#if LOG_SUPPORT_ANSI_COLOR
static void set_color(enum log_color color)
{
    if(color != LOG_COLOR_NONE)
    {
        char str[5] = LOG_ANSI_PREFIX;

        str[2] = ansi_colors[color][0];
        if(color != LOG_COLOR_DEFAULT)
        {
            str[3] = ansi_colors[color][1];
            str[4] = LOG_ANSI_SUFFIX;
            vcp_send(str, 5);
        }
        else
        {
            str[3] = LOG_ANSI_SUFFIX;
            vcp_send(str, 4);
        }
    }
}
#endif


static void proc_string(char *string, uint32_t length)
{
    vcp_send(string, length);
}


static void proc_uint_dec(uint32_t number)
{
    char output[10];
    uint8_t n_digits;

    n_digits = process_number_decimal(number, output);
    proc_string(&output[10 - n_digits], n_digits);
}


static void proc_hex(uint32_t number, uint8_t n_digits)
{
    char output[sizeof(number)*2];

    process_number_hex(number, output, n_digits);
    proc_string(&output[8 - n_digits], n_digits);
}


static void proc_sint_dec(int32_t number)
{
    char output[11];
    bool is_negative = number < 0;
    uint8_t n_digits;

    if(is_negative)
        number = -number;

    n_digits = process_number_decimal(number, &output[1]);

    if(is_negative)
        output[10 - n_digits++] = '-';

    proc_string(&output[11 - n_digits], n_digits);
}


void _log_var(uint32_t number, enum log_data_type type, enum log_color color)
{
    log_fifo_item_t item = {.type = type, .data = number};

#if LOG_SUPPORT_ANSI_COLOR
        item.color = color;
#endif

    log_fifo_put(&item, &logFifo);
}


void _log_str(char *string, uint32_t length, enum log_color color)
{
    log_fifo_item_t item = {.type = LOG_STRING, .data = (uint32_t)string, .str_len = length};

#if LOG_SUPPORT_ANSI_COLOR
        item.color = color;
#endif

    log_fifo_put(&item, &logFifo);
}


void _log_char(char chr, enum log_color color)
{
    log_fifo_item_t item = {.type = LOG_CHAR, .data = chr};

#if LOG_SUPPORT_ANSI_COLOR
        item.color = color;
#endif

    log_fifo_put(&item, &logFifo);
}


void log_flush(void)
{
    log_fifo_item_t item;

    while(log_fifo_get(&item, &logFifo))
    {
#if LOG_SUPPORT_ANSI_COLOR
        set_color(item.color);
#endif
        switch(item.type)
        {
        case LOG_STRING:
            proc_string((char*)item.data, item.str_len);
            break;
        case LOG_UINT_DEC:
            proc_uint_dec(item.data);
            break;
        case LOG_INT_DEC:
            proc_sint_dec((int32_t)item.data);
            break;
        case LOG_HEX_2:
            proc_hex(item.data, 2);
            break;
        case LOG_HEX_4:
            proc_hex(item.data, 4);
            break;
        case LOG_HEX_8:
            proc_hex(item.data, 8);
            break;
        case LOG_CHAR:
            proc_string((char*)&item.data, 1);
        }
    }
}


void log_thread(void const * argument)
{

    while(1)
    {
        log_flush();
        osDelay(LOG_DELAY_LOOPS_MS);
    }
}


void log_init(void)
{
    static_assert(!(LOG_INPUT_FIFO_N_ELEM & (LOG_INPUT_FIFO_N_ELEM - 1)), "Log input queue must be power of 2");
    log_fifo_reset(&logFifo);
}