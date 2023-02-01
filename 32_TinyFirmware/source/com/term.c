/**
 *--------------------------------------------------------------------\n
 *          HSLU T&A Hochschule Luzern Technik+Architektur            \n
 *--------------------------------------------------------------------\n
 *
 * \brief         terminal program
 * \file
 * \author        Christian Jost, christian.jost@hslu.ch
 * \date          19.03.2018
 *
 *--------------------------------------------------------------------
 */
#include "platform.h"
#include "term.h"
#include "uart.h"
#include "string.h"
#include "util.h"

#define TERM_UART0      0
#define TERM_UART1      0
#define TERM_UART2      1
#define TERM_LPUART0    0

typedef enum
{
  uart0 = 1,
  uart1 = 2,
  uart2 = 4,
  lpuart0 = 8,
  uartAll = (uart0 | uart1 | uart2 |lpuart0)
} tUart;

static tUart destination;
static tCommandLineHandler *head = NULL;

void termRegisterCommandLineHandler(tCommandLineHandler *clh, char* cmd, char *cmdDesc, cmdHandler h)
{
  clh->next = head;
  head = clh;

  strncpy(clh->cmd, cmd, sizeof(clh->cmd));
  strcat(clh->cmd, " ");
  strncpy(clh->cmdDesc, cmdDesc, sizeof(clh->cmdDesc));
  clh->cmdHandler = h;
}

/**
 * Writes a character to the uart
 * The output is sent to the uart which received the last command
 *
 * @param[in] ch
 *   the character to send
 */
void termWriteChar(char ch)
{
//  if (TGT_IS_MCCAR)
//  {
    // check if uart0 is enabled
    #if (UART0_EN && TERM_UART0)
      if (destination & uart0) uart0WriteChar(ch);
    #endif

    // check if lpuart0 is enabled
    #if (LPUART0_EN && TERM_LPUART0)
      if (destination & lpuart0) lpuart0WriteChar(ch);
    #endif
//  }

//  if (TGT_IS_TINYK22)
//  {
    // check if uart1 is enabled
    #if (UART1_EN && TERM_UART1)
      if (destination & uart1) uart1WriteChar(ch);
    #endif

    // check if uart2 is enabled
    #if (UART2_EN && TERM_UART2)
      if (destination & uart2) uart2WriteChar(ch);
    #endif
//  }
}

/**
 * Writes a null terminated string in the send buffer. If the
 * string is null, the function returns immediately.
 *
 * @param[in] str
 *   the null terminated string to send
 */
void termWrite(const char *str)
{
  if (str == NULL) return;
  while (*str != '\0') termWriteChar(*str++);
}

void termWriteNum32s(int32_t value)
{
  char str[sizeof("-2147483648")];
  utilNum32sToStr(str, sizeof(str), value);
  termWrite(str);
}

void termWriteNum16s(int16_t value)
{
  char str[sizeof("-32768")];
  utilNum16sToStr(str, sizeof(str), value);
  termWrite(str);
}

/**
 * Writes a null terminated string in the send buffer. If the
 * string is null, only a new new line character is sent.
 *
 * @param[in] str
 *   the null terminated string to send
 */
void termWriteLine(const char *str)
{
  termWrite(str);
  termWriteChar(NEW_LINE);
}

/**
 * Writes a substring in the send buffer.
 *
 * @param[in] str
 *   the null terminated string to send
 * @param[in] start
 *   the start position of the substring to send
 * @param[in] length
 *   the number of characters to send
 */
void termWriteSubString(char *str, uint8_t start, uint8_t length)
{
  str += start;
  while(length--) termWriteChar(*str++);
}

/**
 * Parses one command line and executes the appropriate command.
 *
 * @param[in] cmd
 *   the null terminated string to process
 */
void termParseCommand(char *cmd)
{
  uint8_t subCmdLength = 0;
  tError result = EC_INVALID_CMD;
  tCommandLineHandler *clh = head;
  char buf[16];

  if (strcmp(cmd, "help") == 0)
  {
    termWriteLine(NULL);
    termWriteLine(">>> TinyK22 terminal ready <<<");
    termWriteLine("valid commands are:");
    while(clh != NULL)
    {
      termWrite("  ");
      termWrite(clh->cmd);
      termWrite("-> ");
      termWriteLine(clh->cmdDesc);
      clh = clh->next;
    }
    result = EC_SUCCESS;
  }
  else
  {
    while(clh != NULL)
    {
      if (strncmp(cmd, clh->cmd, strlen(clh->cmd)) == 0)
      {
        subCmdLength = strlen(clh->cmd);
        result = clh->cmdHandler(cmd + strlen(clh->cmd));
        break;
      }
      clh = clh->next;
    }
  }

  if (result != EC_SUCCESS)
  {
    termWriteLine(NULL);
    termWrite(cmd);
    termWriteLine(": command not found!");
    termWrite("Error Code: ");
    buf[0] = '\0';
    utilNum16uToStr(buf, sizeof(buf), result);
    termWriteLine(buf);
    termWrite("Enter '");
    termWriteSubString(cmd, 0, subCmdLength);
    termWriteLine("help' for a list of valid commands.");
  }
}

/**
 * @brief Terminal do work
 *
 * This function reads a line from the uart and calls the
 * termParseCommand function to process the command.
 */
void termDoWork(void)
{
  char cmd[512];
//  if (TGT_IS_MCCAR)
//  {
    #if (UART0_EN && TERM_UART0)
      if (uart0HasLineReceived())
      {
        destination = uart0;
        uart0ReadLine(cmd, sizeof(cmd));            // process uart data from debug interface
        termParseCommand(cmd);
      }
    #endif

    #if (LPUART0_EN && TERM_LPUART0)
      if (lpuart0HasLineReceived())                 // process uart data from bluetooth interface
      {
        destination = lpuart0;
        lpuart0ReadLine(cmd, sizeof(cmd));
        termParseCommand(cmd);
      }
    #endif
//  }

//  if (TGT_IS_TINYK22)
//  {
    #if (UART1_EN && TERM_UART1)
      if (uart1HasLineReceived())                   // process uart data from debug interface
      {
        destination = uart1;
        uart1ReadLine(cmd, sizeof(cmd));
        termParseCommand(cmd);
      }
    #endif
    #if (UART2_EN && TERM_UART2)
      if (uart2HasLineReceived())                   // process uart data from debug interface
      {
        destination = uart2;
        uart2ReadLine(cmd, sizeof(cmd));
        if (strlen(cmd))
        {
          termParseCommand(cmd);
        }
      }
    #endif
//  }
}

/**
 * Initialize the terminal with the desired baudrate.
 *
 * @param[in] baudrate
 *   the desired baudrate (for example: 9600, 19200, 57600...)
 */
void termInit(uint32_t baudrate)
{
  // Send the first message to all uarts
  destination = uartAll;

  // initialize uart0 only if the uart is enabled
  #if (UART0_EN  && TERM_UART0)
    uart0Init(baudrate);
  #endif

  // initialize lpuart0 only if the uart is enabled
  #if (LPUART0_EN && TERM_LPUART0)
    lpuart0Init(baudrate);
  #endif

  // initialize uart1 only if the uart is enabled
  #if (UART1_EN && TERM_UART1)
    uart1Init(baudrate);
  #endif

  // initialize uart1 only if the uart is enabled
  #if (UART2_EN && TERM_UART2)
    uart2Init(baudrate);
  #endif
}
