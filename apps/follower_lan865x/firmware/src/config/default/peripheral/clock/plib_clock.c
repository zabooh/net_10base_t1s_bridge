/*******************************************************************************
 CLOCK PLIB

  Company:
    Microchip Technology Inc.

  File Name:
    plib_clock.c

  Summary:
    CLOCK PLIB Implementation File.

  Description:
    None

*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

#include "plib_clock.h"
#include "device.h"

/* --- HAND-PATCH on generated code --------------------------------------------
 * Follower since 2026-08-12, bridge since 2026-08-22.  This file is byte-identical
 * in BOTH projects; whoever changes it in one changes it in the other too.
 * Why: the MCU time base was hanging off the open-loop DFLL48M - no XOSC, no
 * DFLL closed loop, and OSCCTRL_Initialize()/DFLL_Initialize() were empty.
 * Measured +601 ppm and +783 ppm respectively against the master wall clock, with
 * ~180 ppm drift over 20 minutes. That makes SYS_TIME unusable as a frequency
 * reference, and PTP_TIMEBASE_PLAN.md Phase A unmeasurable (test_results.md, Phase A).
 *
 * What: XOSC0 in external-clock mode (XTALEN=0) as the DPLL0 reference.
 *
 * XIN0 carries 50 MHz, NOT 12 MHz. Measured, not assumed:
 * GCLK generator 3 from XOSC0 -> TC2 (32-bit, GCLK channel 26, independent of
 * TC0/SYS_TIME) -> 1 100 457 527 counts in 21,945 s = 50,147 MHz. So it is
 * the DSC1001CI2-050.0000, the RMII reference clock. An initial attempt with
 * the 12 MHz assumption (DIV=5) did not let the DPLL lock and the board did not
 * boot.
 *
 * Calculation: DIV=9  -> 50 MHz / (2*(9+1)) = 2,5 MHz reference (range 32 kHz to
 *           3,2 MHz), LDR=47 -> 2,5 MHz * 48 = 120 MHz exact, without LDRFRAC.
 *           GCLK0 = 120 MHz and GCLK1 = 60 MHz remain unchanged as a result.
 *
 * BRIDGE: PA14 is XIN0 AND the GMAC's RMII reference clock.  Datasheet 6.2.1
 * says oscillators are "not mapped to the normal PORT functions" - so the
 * concern was that an enabled XOSC0 would steal the GMAC's clock and
 * eth1 would die.  Measured on the running board (_probe_xosc0_gmac.py): XOSC0 on,
 * XOSCRDY0 set (STATUS 0x00010100 -> 0x00010101), eth1 responded
 * consistently 4/4, and 6/6 after reverting it.  Both coexist on PA14.
 * On the follower this doesn't show up anyway - there the GMAC is compiled in,
 * but without an active interface.
 *
 * MCC: plib_clock.c IS generated territory and is tracked in the bridge's
 * hash map - so this patch is a deviation there.  That was a problem until
 * 2026-08-22 (a "Generate Code" would have silently removed it), and
 * no longer is: the bridge is leaving MCC, so the route previously recommended
 * here via the Clock Configurator no longer applies.  It would have been costly anyway -
 * MCC generates UNBOUNDED wait loops, and the bounded guards below and
 * the DFLL fallback would have been lost as a result.  An oscillator that fails
 * to start up then hangs the boot, instead of falling back to the old path.
 *
 * VERIFICATION VALUES after flashing (both boards):
 *   dump 0x40001014 -> 02 00 00 00   XOSCCTRL[0]: ENABLE, ONDEMAND cleared
 *   dump 0x40001038 -> 40 00 09 00   DPLLCTRLB:  REFCLK=2 (XOSC0), DIV=9
 * -------------------------------------------------------------------------- */
static bool clk_xosc0_ready = false;

static void OSCCTRL_Initialize(void)
{
    uint32_t guard;

    /* ENABLE, XTALEN=0 (external clock), ONDEMAND=0 (always running), STARTUP=0:
     * an already-applied clock needs no oscillator startup time. Too large a
     * STARTUP value apparently kept XOSCRDY0 from appearing on the first probe attempt. */
    OSCCTRL_REGS->OSCCTRL_XOSCCTRL[0] = OSCCTRL_XOSCCTRL_ENABLE_Msk;

    for (guard = 0u; guard < 1000000u; guard++)
    {
        if ((OSCCTRL_REGS->OSCCTRL_STATUS & OSCCTRL_STATUS_XOSCRDY0_Msk) != 0u)
        {
            clk_xosc0_ready = true;
            break;
        }
    }

    if (!clk_xosc0_ready)
    {
        OSCCTRL_REGS->OSCCTRL_XOSCCTRL[0] = OSCCTRL_XOSCCTRL_RESETVALUE;
    }
}

static void OSC32KCTRL_Initialize(void)
{

    OSC32KCTRL_REGS->OSC32KCTRL_RTCCTRL = OSC32KCTRL_RTCCTRL_RTCSEL(0U);
}

static void FDPLL0_Initialize(void)
{
    GCLK_REGS->GCLK_PCHCTRL[1] = GCLK_PCHCTRL_GEN(0x2U)  | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[1] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }

    /****************** DPLL0 Initialization  *********************************/

    /* Attempt 1: XOSC0 as the reference. The lock wait loop is BOUNDED - on
     * the first run of this patch (with the wrong 12 MHz assumption) the
     * DPLL did not lock, and the unbounded wait loop killed the board. A
     * wrong clock must cost at most the fallback to the DFLL path. */
    if (clk_xosc0_ready)
    {
        uint32_t guard;

        OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLB = OSCCTRL_DPLLCTRLB_FILTER(0U) | OSCCTRL_DPLLCTRLB_LTIME(0x0U)
                                                | OSCCTRL_DPLLCTRLB_REFCLK(OSCCTRL_DPLLCTRLB_REFCLK_XOSC0_Val)
                                                | OSCCTRL_DPLLCTRLB_DIV(9U) ;

        OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLRATIO = OSCCTRL_DPLLRATIO_LDRFRAC(0U) | OSCCTRL_DPLLRATIO_LDR(47U);

        while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk)
        {
            /* Waiting for the synchronization */
        }

        OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLA = OSCCTRL_DPLLCTRLA_ENABLE_Msk   ;

        while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
        {
            /* Waiting for the DPLL enable synchronization */
        }

        for (guard = 0u; guard < 1000000u; guard++)
        {
            if ((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSTATUS & (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk))
                    == (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk))
            {
                break;
            }
        }

        if (guard >= 1000000u)
        {
            /* No lock: disable the DPLL, give up on XOSC0, take the DFLL path
             * below instead. The board then boots with the poor but
             * functioning time base - instead of not booting at all. */
            OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLA = 0U;
            while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
            {
                /* Waiting for the DPLL disable synchronization */
            }
            OSCCTRL_REGS->OSCCTRL_XOSCCTRL[0] = OSCCTRL_XOSCCTRL_RESETVALUE;
            clk_xosc0_ready = false;
        }
    }

    /* Attempt 2 / fallback: the originally generated path, the reference is
     * GCLK channel 1 from generator 2 (DFLL48M/48 = 1 MHz), LDR=119 -> 120 MHz. */
    if (!clk_xosc0_ready)
    {
        /* Configure DPLL    */
        OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLB = OSCCTRL_DPLLCTRLB_FILTER(0U) | OSCCTRL_DPLLCTRLB_LTIME(0x0U)| OSCCTRL_DPLLCTRLB_REFCLK(0U) ;


        OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLRATIO = OSCCTRL_DPLLRATIO_LDRFRAC(0U) | OSCCTRL_DPLLRATIO_LDR(119U);

        while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk)
        {
            /* Waiting for the synchronization */
        }

        /* Enable DPLL */
        OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLA = OSCCTRL_DPLLCTRLA_ENABLE_Msk   ;

        while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
        {
            /* Waiting for the DPLL enable synchronization */
        }

        while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSTATUS & (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk)) !=
                    (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk))
        {
            /* Waiting for the Ready state */
        }
    }
}


static void DFLL_Initialize(void)
{
}


static void GCLK0_Initialize(void)
{

    /* selection of the CPU clock Division */
    MCLK_REGS->MCLK_CPUDIV = MCLK_CPUDIV_DIV(0x01U);

    while((MCLK_REGS->MCLK_INTFLAG & MCLK_INTFLAG_CKRDY_Msk) != MCLK_INTFLAG_CKRDY_Msk)
    {
        /* Wait for the Main Clock to be Ready */
    }
    GCLK_REGS->GCLK_GENCTRL[0] = GCLK_GENCTRL_DIV(1U) | GCLK_GENCTRL_SRC(7U) | GCLK_GENCTRL_GENEN_Msk;

    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL_GCLK0) == GCLK_SYNCBUSY_GENCTRL_GCLK0)
    {
        /* wait for the Generator 0 synchronization */
    }
}

static void GCLK1_Initialize(void)
{
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_DIV(2U) | GCLK_GENCTRL_SRC(7U) | GCLK_GENCTRL_GENEN_Msk;

    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL_GCLK1) == GCLK_SYNCBUSY_GENCTRL_GCLK1)
    {
        /* wait for the Generator 1 synchronization */
    }
}

static void GCLK2_Initialize(void)
{
    GCLK_REGS->GCLK_GENCTRL[2] = GCLK_GENCTRL_DIV(48U) | GCLK_GENCTRL_SRC(6U) | GCLK_GENCTRL_GENEN_Msk;

    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL_GCLK2) == GCLK_SYNCBUSY_GENCTRL_GCLK2)
    {
        /* wait for the Generator 2 synchronization */
    }
}

void CLOCK_Initialize (void)
{
    /* MISRAC 2012 deviation block start */
    /* MISRA C-2012 Rule 2.2 deviated in this file.  Deviation record ID - H3_MISRAC_2012_R_2_2_DR_2 */

    /* Function to Initialize the Oscillators */
    OSCCTRL_Initialize();

    /* Function to Initialize the 32KHz Oscillators */
    OSC32KCTRL_Initialize();

    DFLL_Initialize();
    GCLK2_Initialize();
    FDPLL0_Initialize();
    GCLK0_Initialize();
    GCLK1_Initialize();

    /* MISRAC 2012 deviation block end */

    /* Selection of the Generator and write Lock for SERCOM0_CORE */
    GCLK_REGS->GCLK_PCHCTRL[7] = GCLK_PCHCTRL_GEN(0x1U)  | GCLK_PCHCTRL_CHEN_Msk;

    while ((GCLK_REGS->GCLK_PCHCTRL[7] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }
    /* Selection of the Generator and write Lock for SERCOM1_CORE */
    GCLK_REGS->GCLK_PCHCTRL[8] = GCLK_PCHCTRL_GEN(0x1U)  | GCLK_PCHCTRL_CHEN_Msk;

    while ((GCLK_REGS->GCLK_PCHCTRL[8] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }
    /* Selection of the Generator and write Lock for TC0 TC1 */
    GCLK_REGS->GCLK_PCHCTRL[9] = GCLK_PCHCTRL_GEN(0x1U)  | GCLK_PCHCTRL_CHEN_Msk;

    while ((GCLK_REGS->GCLK_PCHCTRL[9] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }

    /* Configure the AHB Bridge Clocks */
    MCLK_REGS->MCLK_AHBMASK = 0xffffffU;

    /* Configure the APBA Bridge Clocks */
    MCLK_REGS->MCLK_APBAMASK = 0x77ffU;


}
