/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include "tcpip/tcpip.h"
#include <stdio.h>              /* snprintf - DumpMem() line formatting */
#include <stdlib.h>             /* strtoul - cmd_mem_dump() argument parsing */
#include <string.h>             /* strcmp - TelnetAuthenticationHandler() credential check */
#include "system/console/sys_console.h"
#include "system/command/sys_command.h"
#include "config/default/library/tcpip/telnet.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback functions.
*/

/* initialization.c declares TCPIP_STACK_InitCallback as extern and hands its
   address to TCPIP_STACK_Init()->TCPIP_STACK_Initialize(), but never defines
   it; the tcpip_manager expects *ppStackInit to stay valid for the lifetime
   of the stack init, so it must point to storage with static duration. */
extern const TCPIP_NETWORK_CONFIG TCPIP_HOSTS_CONFIGURATION[];
extern const size_t TCPIP_HOSTS_CONFIGURATION_SIZE;
extern const TCPIP_STACK_MODULE_CONFIG TCPIP_STACK_MODULE_CONFIG_TBL[];
extern const size_t TCPIP_STACK_MODULE_CONFIG_TBL_SIZE;

int TCPIP_STACK_InitCallback(const struct TCPIP_STACK_INIT** ppStackInit)
{
    static TCPIP_STACK_INIT s_tcpipStackInit;

    s_tcpipStackInit.pNetConf   = TCPIP_HOSTS_CONFIGURATION;
    s_tcpipStackInit.nNets      = TCPIP_HOSTS_CONFIGURATION_SIZE;
    s_tcpipStackInit.pModConfig = TCPIP_STACK_MODULE_CONFIG_TBL;
    s_tcpipStackInit.nModules   = TCPIP_STACK_MODULE_CONFIG_TBL_SIZE;
    s_tcpipStackInit.initCback  = NULL;

    *ppStackInit = &s_tcpipStackInit;
    return 0;
}

/* Ported from the sister project (t1s_100baset_bridge/firmware/src/app.c):
   a hardcoded user/password check, registered with the Telnet server so an
   unauthenticated client cannot get a shell on the bridge over the network. */
bool TelnetAuthenticationHandler(const char* user, const char* password, const TCPIP_TELNET_CONN_INFO* pInfo, const void* hParam)
{
    (void)pInfo;
    (void)hParam;

    /* Temporary diagnostic: show exactly what was received, so stray
       characters (e.g. a trailing '\r' the terminal client left in) are
       visible instead of just "Declined". Remove once login works. */
    SYS_CONSOLE_PRINT("Telnet auth attempt: user=\"%s\" password=\"%s\"\n\r", user, password);

    if ((strcmp(user, "admin") == 0) && (strcmp(password, "password") == 0)) {
        SYS_CONSOLE_PRINT("Telnet Access Authenticated\n\r");
        return true;
    } else {
        SYS_CONSOLE_PRINT("Telnet Access Declined\n\r");
        return false;
    }
}

const void* TelnetHandlerParam;

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


/* TODO:  Add any necessary local functions.
*/

/* Ported from the sister project (t1s_100baset_bridge/firmware/src/app.c) to
   read live config/state (structs, driver descriptors, register-backed
   variables) straight out of RAM over the CLI instead of guessing from source.
   One SYS_CONSOLE_PRINT() call per line instead of one per byte, and a wait
   for free ring-buffer space before each line: SYS_CONSOLE_PRINT() is
   fire-and-forget (its write_t() return value is discarded, see
   sys_console.c), so a tight burst of small prints silently loses data once
   the SERCOM TX ring buffer fills faster than 115200 baud can drain it. */
static void DumpMem(uint32_t addr, uint32_t count)
{
    uint8_t *puc = (uint8_t *) addr;
    uint32_t ix;

    for (ix = 0; ix < count; ix += 16u) {
        uint32_t lineBytes = (count - ix > 16u) ? 16u : (count - ix);
        char line[96];
        char ascii[17];
        int pos;
        uint32_t j;

        pos = snprintf(line, sizeof(line), "%08x: ", (unsigned int)(addr + ix));
        for (j = 0; j < 16u; j++) {
            if (j < lineBytes) {
                uint8_t b = puc[ix + j];
                pos += snprintf(line + pos, sizeof(line) - (size_t)pos, " %02x", b);
                ascii[j] = ((b > 31u) && (b < 127u)) ? (char)b : '.';
            } else {
                pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "   ");
            }
        }
        ascii[lineBytes] = '\0';
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "   %s\n\r", ascii);

        while (SYS_CONSOLE_WriteFreeBufferCountGet(SYS_CONSOLE_DEFAULT_INSTANCE) < (ssize_t)pos) {
            /* wait for the SERCOM TX interrupt to drain the ring buffer */
        }
        SYS_CONSOLE_PRINT("%s", line);
    }
}

/* CLI command: dump <address_hex> <count> */
static void cmd_mem_dump(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    (void)pCmdIO;

    if (argc != 3) {
        SYS_CONSOLE_PRINT("Usage: dump <address_hex> <count>\n\r");
        SYS_CONSOLE_PRINT("Example: dump 0x20000000 64\n\r");
        return;
    }

    uint32_t addr  = strtoul(argv[1], NULL, 0);
    uint32_t count = strtoul(argv[2], NULL, 0);

    if (count == 0u) {
        SYS_CONSOLE_PRINT("Count must be > 0\n\r");
        return;
    }

    SYS_CONSOLE_PRINT("Memory dump: 0x%08X  %u bytes\n\r", (unsigned int)addr, (unsigned int)count);
    DumpMem(addr, count);
}

/* CLI command: peek <address_hex> [size=1|2|4, default 4] - read a single value */
static void cmd_mem_peek(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    (void)pCmdIO;

    if (argc < 2 || argc > 3) {
        SYS_CONSOLE_PRINT("Usage: peek <address_hex> [size=1|2|4]\n\r");
        SYS_CONSOLE_PRINT("Example: peek 0x42000000 4\n\r");
        return;
    }

    uint32_t addr = strtoul(argv[1], NULL, 0);
    uint32_t size = (argc == 3) ? strtoul(argv[2], NULL, 0) : 4u;

    switch (size) {
        case 1u:
            SYS_CONSOLE_PRINT("0x%08X: 0x%02X\n\r", (unsigned int)addr, (unsigned int)*(volatile uint8_t*)addr);
            break;
        case 2u:
            SYS_CONSOLE_PRINT("0x%08X: 0x%04X\n\r", (unsigned int)addr, (unsigned int)*(volatile uint16_t*)addr);
            break;
        case 4u:
            SYS_CONSOLE_PRINT("0x%08X: 0x%08X\n\r", (unsigned int)addr, (unsigned int)*(volatile uint32_t*)addr);
            break;
        default:
            SYS_CONSOLE_PRINT("Size must be 1, 2, or 4\n\r");
            break;
    }
}

/* CLI command: poke <address_hex> <value_hex> [size=1|2|4, default 4] - write a single value */
static void cmd_mem_poke(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    (void)pCmdIO;

    if (argc < 3 || argc > 4) {
        SYS_CONSOLE_PRINT("Usage: poke <address_hex> <value_hex> [size=1|2|4]\n\r");
        SYS_CONSOLE_PRINT("Example: poke 0x42000000 0x12345678 4\n\r");
        return;
    }

    uint32_t addr  = strtoul(argv[1], NULL, 0);
    uint32_t value = strtoul(argv[2], NULL, 0);
    uint32_t size  = (argc == 4) ? strtoul(argv[3], NULL, 0) : 4u;

    switch (size) {
        case 1u:
            *(volatile uint8_t*)addr = (uint8_t)value;
            SYS_CONSOLE_PRINT("0x%08X <= 0x%02X\n\r", (unsigned int)addr, (unsigned int)(uint8_t)value);
            break;
        case 2u:
            *(volatile uint16_t*)addr = (uint16_t)value;
            SYS_CONSOLE_PRINT("0x%08X <= 0x%04X\n\r", (unsigned int)addr, (unsigned int)(uint16_t)value);
            break;
        case 4u:
            *(volatile uint32_t*)addr = value;
            SYS_CONSOLE_PRINT("0x%08X <= 0x%08X\n\r", (unsigned int)addr, (unsigned int)value);
            break;
        default:
            SYS_CONSOLE_PRINT("Size must be 1, 2, or 4\n\r");
            break;
    }
}

static const SYS_CMD_DESCRIPTOR s_cmdTbl[] = {
    {"dump", (SYS_CMD_FNC) cmd_mem_dump, ": dump memory (dump <addr_hex> <count>)"},
    {"peek", (SYS_CMD_FNC) cmd_mem_peek, ": read a single value (peek <addr_hex> [size=1|2|4])"},
    {"poke", (SYS_CMD_FNC) cmd_mem_poke, ": write a single value (poke <addr_hex> <value_hex> [size=1|2|4])"},
};

static bool Command_Init(void)
{
    return SYS_CMD_ADDGRP(s_cmdTbl, sizeof(s_cmdTbl) / sizeof(*s_cmdTbl), "Test", ": Test Commands");
}


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;

    /* Temporary diagnostic: confirm the registration itself succeeded
       (a NULL return means the handler slot was already occupied).
       Remove once Telnet login is confirmed working. */
    TCPIP_TELNET_HANDLE telnetAuthHandle = TCPIP_TELNET_AuthenticationRegister(TelnetAuthenticationHandler, &TelnetHandlerParam);
    SYS_CONSOLE_PRINT("Telnet auth handler registration: %s\n\r", (telnetAuthHandle != NULL) ? "OK" : "FAILED (slot already taken)");

    Command_Init();

    /* TODO: Initialize your application's state machine and other
     * parameters.
     */
}


/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks ( void )
{

    /* Check the application's current state. */
    switch ( appData.state )
    {
        /* Application's initial state. */
        case APP_STATE_INIT:
        {
            bool appInitialized = true;


            if (appInitialized)
            {

                appData.state = APP_STATE_SERVICE_TASKS;
            }
            break;
        }

        case APP_STATE_SERVICE_TASKS:
        {

            break;
        }

        /* TODO: implement your application state machine.*/


        /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */
