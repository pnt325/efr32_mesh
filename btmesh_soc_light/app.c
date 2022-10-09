/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_status.h"
#include "app.h"

#include "sl_btmesh_api.h"
#include "sl_bt_api.h"
#include "sl_button.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"
#include "sl_btmesh_factory_reset.h"
#include "sl_simple_led_instances.h"
#include "sl_btmesh_lighting_level_transition_handler.h"
#include "sl_simple_timer.h"

/// Advertising Provisioning Bearer
#define PB_ADV                         0x1
/// GATT Provisioning Bearer
#define PB_GATT                        0x2

#define APP_LED_BLINKING_TIMEOUT       250

static sl_simple_timer_t app_led_blinking_timer;
static bool on_booting = true;

static void app_led_blinking_timer_cb(sl_simple_timer_t *handle, void *data);


/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////

	if(on_booting == false)
	{
		// Handle button pressed

	}
}

bool handle_reset_conditions(void)
{
  if(sl_simple_button_get_state(&sl_button_btn0) == SL_SIMPLE_BUTTON_PRESSED)
  {
      sl_btmesh_initiate_full_reset();
      return false;
  }
  return true;
}

void handle_boot_event(void)
{
  sl_status_t sc;
  if(handle_reset_conditions())
    {
      sc = sl_btmesh_node_init();
      app_assert_status(sc);
    }
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(struct sl_bt_msg *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
    	handle_boot_event();
      break;
    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/**************************************************************************//**
 * Bluetooth Mesh stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth Mesh stack.
 *****************************************************************************/
void sl_btmesh_on_event(sl_btmesh_msg_t *evt)
{
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_btmesh_evt_node_initialized_id:
      if (!evt->data.evt_node_initialized.provisioned) {
        // The Node is now initialized,
        // start unprovisioned Beaconing using PB-ADV and PB-GATT Bearers
        sc = sl_btmesh_node_start_unprov_beaconing(PB_ADV | PB_GATT);
        app_assert_status_f(sc, "Failed to start unprovisioned beaconing\n");
      }

      on_booting = false;
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}


void sl_btmesh_lighting_level_pwm_cb(uint16_t pwm)
{
	if(pwm > 1)
	{
		sl_simple_led_turn_on(sl_led_led0.context);
	}
	else
	{
		sl_simple_led_turn_off(sl_led_led0.context);
	}
}

void sl_btmesh_on_node_provisioned(uint16_t address, uint32_t iv_index)
{
	// Provision finish
	(void) address;
	(void)iv_index;
	sl_status_t sc = sl_simple_timer_stop(&app_led_blinking_timer);
	app_assert_status_f(sc, "Provisioning finish\r\n");
}

void sl_btmesh_on_node_provisioning_started(uint16_t result)
{
	// Provision start
	// start timer
	(void)result;
	sl_status_t sc = sl_simple_timer_start(&app_led_blinking_timer,
											APP_LED_BLINKING_TIMEOUT,
											app_led_blinking_timer_cb,
											NULL,
											true);
	app_assert_status_f(sc, "Provisioning start\r\n");
}

void sl_btmesh_on_node_provisioning_failed(uint16_t result)
{
	// Provision failed
	(void)result;

	sl_status_t sc = sl_simple_timer_stop(&app_led_blinking_timer);
	app_assert_status_f(sc, "Provisioning failed callback\r\n");
}

static void app_led_blinking_timer_cb(sl_simple_timer_t *handle, void *data)
{
	(void)data;
	(void)handle;

	sl_simple_led_toggle(sl_led_led0.context);
}
