/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 CTCaer
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

#include "tui.h"
#include "../utils/btn.h"
#include "../config/config.h"
#include "../power/max17050.h"
#include "../utils/util.h"
#include "../utils/touch.h"

#ifdef MENU_LOGO_ENABLE
extern u8 *Kc_MENU_LOGO;
#define X_MENU_LOGO       119
#define Y_MENU_LOGO        57
#define X_POS_MENU_LOGO   (con->gfx_ctxt->width - 143)
#define Y_POS_MENU_LOGO   (con->gfx_ctxt->height - 200)
#endif //MENU_LOGO_ENABLE

extern hekate_config h_cfg;

void tui_sbar(gfx_con_t *con, bool force_update)
{
	u32 cx, cy;

	u32 timePassed = get_tmr_s() - h_cfg.sbar_time_keeping;
	if (!force_update)
		if (timePassed < 5)
			return;

	u8 prevFontSize = con->fntsz;
	con->fntsz = 16;
	h_cfg.sbar_time_keeping = get_tmr_s();

	u32 battPercent = 0;
	int battVoltCurr = 0;

	gfx_con_getpos(con, &cx, &cy);
	gfx_con_setpos(con, 0,  con->gfx_ctxt->height - 20);

	max17050_get_property(MAX17050_RepSOC, (int *)&battPercent);
	max17050_get_property(MAX17050_VCELL, &battVoltCurr);

	gfx_clear_partial_grey(con->gfx_ctxt, 0x30, con->gfx_ctxt->height - 24, 24);
	gfx_printf(con, "%K%k Battery: %d.%d%% (%d mV) - Charge:", 0xFF303030, 0xFF888888,
		(battPercent >> 8) & 0xFF, (battPercent & 0xFF) / 26, battVoltCurr);

	max17050_get_property(MAX17050_AvgCurrent, &battVoltCurr);

	if (battVoltCurr >= 0)
		gfx_printf(con, " %k+%d mA     %k%K\n",
			0xFF008800, battVoltCurr / 1000, 0xFFCCCCCC, 0xFF1B1B1B);
	else
		gfx_printf(con, " %k-%d mA     %k%K\n",
			0xFF880000, (~battVoltCurr) / 1000, 0xFFCCCCCC, 0xFF1B1B1B);
	con->fntsz = prevFontSize;
	gfx_con_setpos(con, cx, cy);
}

void tui_pbar(gfx_con_t *con, int x, int y, u32 val, u32 fgcol, u32 bgcol)
{
	u32 cx, cy;
	if (val > 200)
		val = 200;

	gfx_con_getpos(con, &cx, &cy);

	gfx_con_setpos(con, x, y);

	gfx_printf(con, "%k[%3d%%]%k", fgcol, val, 0xFFCCCCCC);

	x += 7 * con->fntsz;
	
	for (int i = 0; i < (con->fntsz >> 3) * 6; i++)
	{
		gfx_line(con->gfx_ctxt, x, y + i + 1, x + 3 * val, y + i + 1, fgcol);
		gfx_line(con->gfx_ctxt, x + 3 * val, y + i + 1, x + 3 * 100, y + i + 1, bgcol);
	}

	gfx_con_setpos(con, cx, cy);

	// Update status bar.
	tui_sbar(con, false);
}

void *tui_do_menu(gfx_con_t *con, menu_t *menu)
{
	int idx = 0, prev_idx = 0, cnt = 0x7FFFFFFF;

	gfx_clear_partial_grey(con->gfx_ctxt, 0x1B, 0, con->gfx_ctxt->height - 24);
	tui_sbar(con, true);

#ifdef MENU_LOGO_ENABLE
	gfx_set_rect_rgb(con->gfx_ctxt, Kc_MENU_LOGO,
		X_MENU_LOGO, Y_MENU_LOGO, X_POS_MENU_LOGO, Y_POS_MENU_LOGO);
#endif //MENU_LOGO_ENABLE

	while (1)
	{
		gfx_con_setcol(con, 0xFFCCCCCC, 1, 0xFF1B1B1B);
		gfx_con_setpos(con, menu->x, menu->y);
		gfx_printf(con, "%k%s\n\n", 0xFF0099EE, menu->caption);

		// Skip caption or seperator lines selection.
		while (menu->ents[idx].type == MENT_CAPTION ||
			menu->ents[idx].type == MENT_CHGLINE)
		{
			if (prev_idx <= idx || (!idx && prev_idx == cnt - 1))
			{
				idx++;
				if (idx > (cnt - 1))
				{
					idx = 0;
					prev_idx = 0;
				}
			}
			else
			{
				idx--;
				if (idx < 0)
				{
					idx = cnt - 1;
					prev_idx = cnt;
				}
			}
		}
		prev_idx = idx;

		// Draw the menu.
		for (cnt = 0; menu->ents[cnt].type != MENT_END; cnt++)
		{
			if (cnt == idx)
				gfx_con_setcol(con, 0xFF1B1B1B, 1, 0xFFCCCCCC);
			else
				gfx_con_setcol(con, 0xFFCCCCCC, 1, 0xFF1B1B1B);
			if (menu->ents[cnt].type == MENT_CAPTION)
				gfx_printf(con, "%k %s", menu->ents[cnt].color, menu->ents[cnt].caption);
			else if (menu->ents[cnt].type != MENT_CHGLINE)
				gfx_printf(con, " %s", menu->ents[cnt].caption);
			if(menu->ents[cnt].type == MENT_MENU)
				gfx_printf(con, "%k...", 0xFF0099EE);
			gfx_printf(con, " \n");
		}
		gfx_con_setcol(con, 0xFFCCCCCC, 1, 0xFF1B1B1B);
		gfx_putc(con, '\n');

		// Print help and battery status.
		gfx_con_getpos(con, &con->savedx,  &con->savedy);

		struct button
		{
			u32 min_x;
			u32 min_y;
			u32 max_x;
			u32 max_y;
		};
		
		struct button up;
		up.min_x = 30;
		up.min_y = con->gfx_ctxt->height - 24 - 210;
		up.max_x = 110;
		up.max_y = con->gfx_ctxt->height - 24 - 130;
		
		struct button down;
		down.min_x = 30;
		down.min_y = con->gfx_ctxt->height - 24 - 110;
		down.max_x = 110;
		down.max_y = con->gfx_ctxt->height - 24 - 30;
		
		struct button select;
		select.min_x = con->gfx_ctxt->width - 110;
		select.min_y = con->gfx_ctxt->height - 24 - 110;
		select.max_x = con->gfx_ctxt->width - 30;
		select.max_y = con->gfx_ctxt->height - 24 - 30;
		
		struct button back;
		back.min_x = con->gfx_ctxt->width - 210;
		back.min_y = con->gfx_ctxt->height - 24 - 110;
		back.max_x = con->gfx_ctxt->width - 130;
		back.max_y = con->gfx_ctxt->height - 24 - 30;
		
		gfx_line(con->gfx_ctxt, up.min_x, up.min_y, up.max_x, up.min_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, up.min_x, up.min_y, up.min_x, up.max_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, up.max_x, up.min_y, up.max_x, up.max_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, up.min_x, up.max_y, up.max_x, up.max_y, 0xFF555555);
		
		gfx_line(con->gfx_ctxt, down.min_x, down.min_y, down.max_x, down.min_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, down.min_x, down.min_y, down.min_x, down.max_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, down.max_x, down.min_y, down.max_x, down.max_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, down.min_x, down.max_y, down.max_x, down.max_y, 0xFF555555);
		
		gfx_line(con->gfx_ctxt, select.min_x, select.min_y, select.max_x, select.min_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, select.min_x, select.min_y, select.min_x, select.max_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, select.max_x, select.min_y, select.max_x, select.max_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, select.min_x, select.max_y, select.max_x, select.max_y, 0xFF555555);
		
		gfx_line(con->gfx_ctxt, back.min_x, back.min_y, back.max_x, back.min_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, back.min_x, back.min_y, back.min_x, back.max_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, back.max_x, back.min_y, back.max_x, back.max_y, 0xFF555555);
		gfx_line(con->gfx_ctxt, back.min_x, back.max_y, back.max_x, back.max_y, 0xFF555555);
		
		con->fillbg = false;
		gfx_con_setpos(con, up.min_x + (up.max_x - up.min_x) / 2 - 13, up.min_y + (up.max_y - up.min_y - 16) / 2);
		gfx_putc(con, '/');
		gfx_con_setpos(con, up.min_x + (up.max_x - up.min_x) / 2 - 3, up.min_y + (up.max_y - up.min_y - 16) / 2);
		gfx_putc(con, '\\');
		
		gfx_con_setpos(con, down.min_x + (down.max_x - down.min_x) / 2 - 13, down.min_y + (down.max_y - down.min_y - 16) / 2);
		gfx_putc(con, '\\');
		gfx_con_setpos(con, down.min_x + (down.max_x - down.min_x) / 2 - 3, down.min_y + (down.max_y - down.min_y - 16) / 2);
		gfx_putc(con, '/');
		
		gfx_con_setpos(con, select.min_x + (select.max_x - select.min_x - 16) / 2, select.min_y + (select.max_y - select.min_y) / 2 - 13);
		gfx_putc(con, '\\');
		gfx_con_setpos(con, select.min_x + (select.max_x - select.min_x - 16) / 2, select.min_y + (select.max_y - select.min_y) / 2 - 3);
		gfx_putc(con, '/');
		
		gfx_con_setpos(con, back.min_x + (back.max_x - back.min_x - 16) / 2, back.min_y + (back.max_y - back.min_y) / 2 - 13);
		gfx_putc(con, '/');
		gfx_con_setpos(con, back.min_x + (back.max_x - back.min_x - 16) / 2, back.min_y + (back.max_y - back.min_y) / 2 - 3);
		gfx_putc(con, '\\');

		struct touch_event event = touch_wait();
		if (event.y >= up.min_y && event.y <= up.max_y && event.x >= up.min_x && event.x <= up.max_x)
		{
			if (idx > 0)
				idx--;
			else
				idx = cnt - 1;
		}
		else if (event.y >= down.min_y && event.y <= down.max_y && event.x >= down.min_x && event.x <= down.max_x)
		{
			if (idx < (cnt - 1))
				idx++;
			else
				idx = 0;
		}
		else if (event.y >= select.min_y && event.y <= select.max_y && event.x >= select.min_x && event.x <= select.max_x)
		{
			ment_t *ent = &menu->ents[idx];
			switch (ent->type)
			{
			case MENT_HANDLER:
				ent->handler(ent->data);
				break;
			case MENT_MENU:
				return tui_do_menu(con, ent->menu);
				break;
			case MENT_CHOICE:
				return ent->data;
				break;
			case MENT_HDLR_RE:
				ent->handler(ent->data);
				return NULL;
			default:
				break;
			}
			con->fntsz = 16;
			gfx_clear_partial_grey(con->gfx_ctxt, 0x1B, 0, con->gfx_ctxt->height - 24);
#ifdef MENU_LOGO_ENABLE
			gfx_set_rect_rgb(con->gfx_ctxt, Kc_MENU_LOGO,
				X_MENU_LOGO, Y_MENU_LOGO, X_POS_MENU_LOGO, Y_POS_MENU_LOGO);
#endif //MENU_LOGO_ENABLE
		}
		else if (event.y >= back.min_y && event.y <= back.max_y && event.x >= back.min_x && event.x <= back.max_x)
		{
			return NULL;
		}
		tui_sbar(con, false);
	}

	return NULL;
}
