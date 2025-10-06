/*
 * Copyright (c) 2025 Pierre Haessig
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1
 */

/**
 * @brief  Application for DC/DC conversion open loop (fixed, adjustable duty cycle)
 *         using one leg of the three phase OwnVerter board.
 *
 * @author Pierre Haessig <pierre.haessig@centralesupelec.fr>
 */

/* Future ideas: 
- variable frequency
- use output pin for sending event sync signal to scope */

/* --------------OWNTECH APIs---------------------------------- */
#include "TaskAPI.h"
#include "ShieldAPI.h"
#include "SpinAPI.h"

#include "zephyr/console/console.h"

// OwnVerter leg to be used: power including measurements
#define LEG LEG1 // Leg to use
#define V_LOW V1_LOW // Leg voltage to measure
#define I_LOW I1_LOW // Leg current to measure

/* --------------SETUP AND LOOP FUNCTIONS DECLARATION------------------- */

/* Set up of hardware and software at board startup */
void setup_routine();

/* Interaction with the user on the serial monitor (slow background task) */
void user_interface_task();
/* Displaying board status messages on the serial monitor (slow background task) */
void status_display_task();
/* Power converter control (critical periodic task) */
void control_task();

/* -------------- VARIABLES DECLARATIONS------------------- */

static const float32_t T_control = 100e-6F; // Control task period (s)
static const uint32_t T_control_micro = (uint32_t)(T_control * 1.e6F); // Control task period (integer number of µs)

/* BOARD POWER CONVERSION STATE VARIABLES */

static bool power_enable = false; // Power conversion state of the leg (PWM activation state)
float32_t duty_cycle = 0.5; // PWM duty cycle
float32_t duty_cycle_increment = 0.05; // PWM duty cycle up or down increment

/* Possible modes for the OwnTech board */
enum serial_interface_menu_mode
{
	IDLE_MODE = 0,
	POWER_MODE
};

uint8_t mode = IDLE_MODE; // Currently user-requested mode

/* COMMUNICATION AND MEASUREMENT VARIABLES */

uint8_t received_serial_char; // Temporary storage for serial monitor character reception

/* Measurement variables */

static float32_t V_low; // Low-side leg voltage
static float32_t I_low; // Low-side leg current
static float32_t I_high; // High-side voltage (DC bus)
static float32_t V_high; // High-side current (DC bus current to the legs)

static float meas_data; // Temporary storage for measured value


/* -------------- SETUP FUNCTION -------------------------------*/

/**
 * Setup routine, called at board startup.
 * It is used to initialize the board (spin microcontroller and power shield)
 * and the application (set tasks).
 */
void setup_routine()
{
	spin.led.turnOn(); // Blink LED at board startup

	/* Set the high switch convention for all legs */
	shield.power.initBuck(ALL);

	/* Setup all the measurements */
	shield.sensors.enableDefaultOwnverterSensors();

	/* Declare tasks */
	uint32_t app_task_number = task.createBackground(status_display_task);
	uint32_t com_task_number = task.createBackground(user_interface_task);
	task.createCritical(control_task, T_control_micro);

	/* Start tasks */
	task.startBackground(app_task_number);
	task.startBackground(com_task_number);
	task.startCritical();
}

/* --------------LOOP FUNCTIONS (TASKS) ------------------------------- */

/**
 * User interface task, running in a loop in the background.
 * It allows controlling the application through the serial monitor.
 *
 * It waits for the user to press a key to select an action.
 * In particular, 'h' displays the help menu.
 */
void user_interface_task()
{
	received_serial_char = console_getchar();
	switch (received_serial_char) {
	case 'h':
		/* ----------SERIAL INTERFACE MENU----------------------- */

		printk(" ________________________________________ \n"
				"|     ------- MENU ---------             |\n"
				"|     press i : idle mode                |\n"
				"|     press p : power mode               |\n"
				"|     press u : duty cycle UP            |\n"
				"|     press j : duty cycle DOWN          |\n"
				"|________________________________________|\n\n");

		/* ------------------------------------------------------ */
		break;
	case 'i':
		printk("Idle mode request\n");
		mode = IDLE_MODE;
		break;
	case 'p':
		printk("Power mode request (duty %.2f) \n", (double) duty_cycle);
		mode = POWER_MODE;
		break;
	case 'u':
		duty_cycle += duty_cycle_increment;
		printk("Duty cycle UP (%.2f) \n", (double) duty_cycle);
		break;
	case 'j':
		duty_cycle -= duty_cycle_increment;
		printk("Duty cycle DOWN (%.2f) \n", (double) duty_cycle);
		break;
	default:
		break;
	}
}

/**
 * Board status display task, called pseudo-periodically.
 * It displays board measurements on the serial monitor
 * 
 * It also sets the board LED (blinking when POWER_MODE).
 */
void status_display_task()
{
	if (mode == IDLE_MODE) {
		spin.led.turnOn(); // Constantly ON led when IDLE
		printk("IDL: ");
		printk("Vl %5.2f V, ", (double) V_low);
		printk("Il %4.2f A | ", (double) I_low);
		printk("Vh %5.2f V, ", (double) V_high);
		printk("Ih %4.2f A, ", (double) I_high);
		printk("\n");

	} else if (mode == POWER_MODE) {
		spin.led.toggle(); // Blinking LED when POWER
		printk("POW: ");
		printk("d %3.0f%%, ", (double) (duty_cycle*100));
		printk("Vl %5.2f V, ", (double) V_low); // remark: on Ownverter, low side measurement when power is on is not usable
		printk("Il %4.2f A | ", (double) I_low);
		printk("Vh %5.2f V, ", (double) V_high);
		printk("Ih %4.2f A, ", (double) I_high);
		printk("\n");
	}
	task.suspendBackgroundMs(200);
}

/**
 * This is the code loop of the critical task.
 * It is executed every T_control seconds (100 µs by default).
 * 
 * Actions:
 * - measure voltage and currents
 * - control the power converter leg (ON/OFF state and duty cycle)
 */
void control_task()
{
	/* Retrieve sensor values */
	meas_data = shield.sensors.getLatestValue(I_LOW);
	if (meas_data != NO_VALUE) {
		I_low = meas_data;
	}

	meas_data = shield.sensors.getLatestValue(V_LOW);
	if (meas_data != NO_VALUE) {
		V_low = meas_data;
	}

	meas_data = shield.sensors.getLatestValue(I_HIGH);
	if (meas_data != NO_VALUE) {
		I_high = meas_data;
	}

	meas_data = shield.sensors.getLatestValue(V_HIGH);
	if (meas_data != NO_VALUE) {
		V_high = meas_data;
	}

	if (mode == IDLE_MODE) {
		if (power_enable == true) {
			shield.power.stop(ALL);
		}
		power_enable = false;
	} else if (mode == POWER_MODE) {
		/* Set leg duty cycle */
		shield.power.setDutyCycle(LEG, duty_cycle);
		/* Set POWER ON */
		if (!power_enable) {
			power_enable = true;
			shield.power.start(LEG);
		}
	}
}

/**
 * Main function of the application.
 * This function is generic and does not need editing.
 */
int main(void)
{
	setup_routine();
	return 0;
}

