/*
 * Copyright (c) 2018 ???
 * Copyright (c) 2018 balika011
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "touch.h"

/*#include "../gfx/gfx.h"
extern gfx_ctxt_t gfx_ctxt;
extern gfx_con_t gfx_con;
#define DPRINTF(...) gfx_printf(&gfx_con, __VA_ARGS__)*/
#define DPRINTF(...)

struct touch_event last_event;

static int touch_command(u8 cmd)
{
	int err = i2c_send_byte(I2C_3, 0x49, cmd, 0);
	if (err < 0)
		return err;
		
	// TODO: Check for completion in event loop
	msleep(10);
	return 0;
}

static void touch_process_contact_event(struct touch_event *event)
{
	event->x = (event->raw[2] << 4) | ((event->raw[3] & 0x0f));
	event->y = (event->raw[3] << 4) | ((event->raw[3] & 0xf0) >> 1);
	
	DPRINTF("x = %d    y = %d    \n", event->x, event->y);
}

static void touch_parse_event(struct touch_event *event) 
{
	event->type = event->raw[0];
	//gfx_con_setpos(&gfx_con, 0, 0);
	DPRINTF("Event = 0x%02x\n", event->type);

	switch (event->type) 
	{
	case STMFTS_EV_NO_EVENT:
	case STMFTS_EV_CONTROLLER_READY:
	case STMFTS_EV_SLEEP_OUT_CONTROLLER_READY:
	case STMFTS_EV_STATUS:
		return;

	case STMFTS_EV_MULTI_TOUCH_ENTER:
	case STMFTS_EV_MULTI_TOUCH_MOTION:
		touch_process_contact_event(event);
		break;
	}
}

static void touch_poll(struct touch_event *event)
{
	i2c_recv_buf_small(event->raw, 4, I2C_3, 0x49, STMFTS_LATEST_EVENT);
	touch_parse_event(event);
}

struct touch_event touch_wait()
{
	struct touch_event event;
	do 
	{
		touch_poll(&event);
	} while(event.type != STMFTS_EV_MULTI_TOUCH_ENTER);

	return event;
}

int touch_power_on() 
{
	int err;

	/*
	 * The datasheet does not specify the power on time, but considering
	 * that the reset time is < 10ms, I sleep 20ms to be sure
	 */
	msleep(20);
	
	err = touch_command(STMFTS_SYSTEM_RESET);
	if (err < 0)
		return err;

	err = touch_command(STMFTS_SLEEP_OUT);
	if (err < 0)
		return err;

	err = touch_command(STMFTS_MS_CX_TUNING);
	if (err < 0)
		return err;

	err = touch_command(STMFTS_SS_CX_TUNING);
	if (err < 0)
		return err;

	err = touch_command(STMFTS_FULL_FORCE_CALIBRATION);
	if (err < 0)
		return err;

	err = touch_command(STMFTS_MS_MT_SENSE_ON);
	if (err < 0)
		return err;

	return 1;
}