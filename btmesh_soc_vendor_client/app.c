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
#include "sl_simple_timer.h"
#include "sl_btmesh_data_logging_client.h"
#include "sl_btmesh_data_logging_config.h"

/// Advertising Provisioning Bearer
#define PB_ADV                         0x1
/// GATT Provisioning Bearer
#define PB_GATT                        0x2

#define APP_LED_BLINKING_TIMEOUT       250

#define LOG_SAMPLE_RATE                1500
#define LOG_PERIOD                     5000

static sl_simple_timer_t app_led_blinking_timer;
static bool on_booting = true;

/// Buffer for the Log received
sl_data_log_data_t log_data_arr[SL_BTMESH_DATA_LOG_BUFF_SIZE_CFG_VAL];

sl_data_log_recv_t log_data = {
    .source_addr = 0,
    .dest_addr = 0,
    .index = 0,
    .data = log_data_arr
};

bool log_received_flag = false;

/***************************************************************************//**
 * Print the received log
 *********************0********************************************************/
void print_log(void);
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

	app_log("App init\r\n");
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

	static bool btn_state_old = false;
	if (on_booting == false) {
		// Handle button pressed
		bool state = sl_simple_button_get_state(&sl_button_btn0);
		if (state != btn_state_old) {
			btn_state_old = state;
			// uint32_t sample = LOG_SAMPLE_RATE;
			uint32_t period = LOG_PERIOD;

			if (state == SL_SIMPLE_BUTTON_PRESSED) {
				sl_status_t sc;

				sc = sl_btmesh_data_log_client_set_period(&period);
				app_assert(sc == SL_STATUS_OK, "[E: 0x%04x] Failed to set period\n", (int )sc);
			}
		}
	}

	sl_status_t sc;
	if(log_received_flag)
	{
		print_log();

		sc = sl_btmesh_data_log_client_reset_log();
		app_assert(sc == SL_STATUS_OK, "[E: 0x%04x] Failed to reset log\n",
				(int )sc);
		log_received_flag = false;
	}
}

bool handle_reset_conditions(void)
{
  if(sl_simple_button_get_state(&sl_button_btn0) == SL_SIMPLE_BUTTON_PRESSED)
  {
      sl_btmesh_initiate_full_reset();

      app_log("Factory\r\n");
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
      app_log("Node init\r\n");
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
void sl_btmesh_on_event(sl_btmesh_msg_t *evt) {
	sl_status_t sc;
	switch (SL_BT_MSG_ID(evt->header)) {
	case sl_btmesh_evt_node_initialized_id:
		sc = sl_btmesh_data_log_client_init(&log_data);
		app_assert(sc == SL_STATUS_OK, "[E: 0x%04x] Failed to Init Log\n",
				(int )sc);

		if (!evt->data.evt_node_initialized.provisioned) {
			// The Node is now initialized,
			// start unprovisioned Beaconing using PB-ADV and PB-GATT Bearers
			sc = sl_btmesh_node_start_unprov_beaconing(PB_ADV | PB_GATT);
			app_assert_status_f(sc,
					"Failed to start unprovisioned beaconing\n");
		}

		on_booting = false;
		app_log("Vendor model init\r\n");
		break;
	case sl_btmesh_evt_vendor_model_receive_id:
		sc = sl_btmesh_data_log_on_client_receive_event(evt);
		app_assert(sc == SL_STATUS_OK, "[E: 0x%04x] Failed to handle event!\n", (int )sc);
		app_log("Vendor model received\r\n");
		break;
	case sl_btmesh_evt_node_reset_id:
		app_log("Node reset\r\n");
		sl_btmesh_initiate_node_reset();
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


/***************************************************************************//**
 * Log receive completed Callbacks
 ******************************************************************************/
void sl_btmesh_data_log_client_recv_complete_callback(void)
{
  log_received_flag = true;
  app_log("Log received complete\r\n");
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

void print_log(void)
{
  uint16_t count;
  app_log("Received Len: %d\r\n", log_data.index);
  app_log("Received Log: [ ");
  for(count = 0; count < log_data.index; count++){
      app_log("%d ", log_data.data[count]);
  }
  app_log("]\r\n");
}

static void app_led_blinking_timer_cb(sl_simple_timer_t *handle, void *data)
{
	(void)data;
	(void)handle;

	sl_simple_led_toggle(sl_led_led0.context);
}
