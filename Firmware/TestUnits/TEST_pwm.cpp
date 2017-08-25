#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "lpc_types.h"
#include "chip-defs.h"
#include "sct_18xx_43xx.h"
#include "sct_pwm_18xx_43xx.h"
#include "scu_18xx_43xx.h"


#define SCT_PWM            LPC_SCT

#define SCT_PWM_PIN_OUT    4        /* COUT4 Generate square wave */
#define SCT_PWM_PIN_LED    5        /* COUT5 [index 2] Controls LED */

#define SCT_PWM_OUT        1        /* Index of OUT PWM */
#define SCT_PWM_LED        2        /* Index of LED PWM */
#define SCT_PWM_RATE   10000        /* PWM frequency 10 KHz */

/* Systick timer tick rate, to change duty cycle */
#define TICKRATE_HZ     1000        /* 1 ms Tick rate */


/*
TODO need a table to look up if pin is a valid pwm
PWM pin mappings, these pins can be used for PWM output, given the specific function

Func1
-----
P2_7  CTOUT_1
P2_8  CTOUT_0
P2_9  CTOUT_3
P2_10 CTOUT_2
P2_11 CTOUT_5
P2_12 CTOUT_4

P4_1  CTOUT_1
P4_2  CTOUT_0
P4_3  CTOUT_3
P4_4  CTOUT_2
P4_5  CTOUT_5
P4_6  CTOUT_4

P6_5  CTOUT_6
P6_12 CTOUT_7

P7_0  CTOUT_14
P7_1  CTOUT_15
P7_4  CTOUT_13
P7_5  CTOUT_12
P7_6  CTOUT_11
P7_7  CTOUT_8

PA_4  CTOUT_9

PB_0  CTOUT_10

PD_0  CTOUT_15
PD_2  CTOUT_7
PD_3  CTOUT_6
PD_4  CTOUT_8
PD_5  CTOUT_9
PD_6  CTOUT_10
PD_9  CTOUT_13

PE_5  CTOUT_3
PE_6  CTOUT_2
PE_7  CTOUT_5
PE_8  CTOUT_4

PE_11  CTOUT_12
PE_12  CTOUT_11
PE_13  CTOUT_14
PE_15  CTOUT_0

Func2
-----

P1_7  CTOUT_13
P1_8  CTOUT_12
P1_9  CTOUT_11
P1_10 CTOUT_14
P1_11 CTOUT_15

PF_9 CTOUT_1

Func5
-----

PB_1 CTOUT_6
PB_2 CTOUT_7
PB_3 CTOUT_8

Func6
-----

PD_11  CTOUT_14
PD_12  CTOUT_10
PD_13  CTOUT_13
PD_14  CTOUT_11
PD_15  CTOUT_8
PD_16  CTOUT_12

*/


REGISTER_TEST(PWMTest, basic)
{
    /* Initialize the SCT as PWM and set frequency */
    Chip_SCTPWM_Init(SCT_PWM);
    Chip_SCTPWM_SetRate(SCT_PWM, SCT_PWM_RATE);

    /* SCT_OUT5 on P2.11 mapped to FUNC1: LED2 */
    Chip_SCU_PinMuxSet(0x2, 11, (SCU_MODE_INACT | SCU_MODE_FUNC1));
    /* SCT_OUT4 on P2.12 mapped to FUNC1: Oscilloscope input */
    Chip_SCU_PinMuxSet(0x2, 12, (SCU_MODE_INACT | SCU_MODE_FUNC1));

    /* Use SCT0_OUT1 pin */
    Chip_SCTPWM_SetOutPin(SCT_PWM, SCT_PWM_OUT, SCT_PWM_PIN_OUT);
    Chip_SCTPWM_SetOutPin(SCT_PWM, SCT_PWM_LED, SCT_PWM_PIN_LED);

    /* Start with 50% duty cycle */
    Chip_SCTPWM_SetDutyCycle(SCT_PWM, SCT_PWM_OUT, Chip_SCTPWM_PercentageToTicks(SCT_PWM, 50));
    Chip_SCTPWM_SetDutyCycle(SCT_PWM, SCT_PWM_LED, 50);
    Chip_SCTPWM_Start(SCT_PWM);
}

