/*         ______   ___    ___
 *        /\  _  \ /\_ \  /\_ \
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      QNX mouse driver.
 *
 *      By Angelo Mottola.
 *
 *      Based on Unix/X11 version by Michael Bukin.
 *
 *      See readme.txt for copyright information.
 */


#include "allegro.h"
#include "allegro/aintern.h"
#include "allegro/aintqnx.h"


int qnx_mouse_warped = FALSE;

static int mouse_minx = 0;
static int mouse_miny = 0;
static int mouse_maxx = 319;
static int mouse_maxy = 199;

static int mymickey_x = 0;
static int mymickey_y = 0;



/* qnx_mouse_handler:
 *  Mouse "interrupt" handler for mickey-mode driver.
 */
void qnx_mouse_handler(int x, int y, int z, int buttons)
{
   _mouse_b = buttons;

   mymickey_x += x;
   mymickey_y += y;

   _mouse_x += x;
   _mouse_y += y;
   _mouse_z += z;
   
   if ((_mouse_x < mouse_minx) || (_mouse_x > mouse_maxx)
       || (_mouse_y < mouse_miny) || (_mouse_y > mouse_maxy)) {
      _mouse_x = MID(mouse_minx, _mouse_x, mouse_maxx);
      _mouse_y = MID(mouse_miny, _mouse_y, mouse_maxy);
   }

   _handle_mouse_input();
}



/* qnx_mouse_init:
 *  Initializes the mickey-mode driver.
 */
int qnx_mouse_init(void)
{
   return 3;
}



/* qnx_mouse_exit:
 *  Shuts down the mickey-mode driver.
 */
void qnx_mouse_exit(void)
{
}



/* qnx_mouse_position:
 *  Sets the position of the mickey-mode mouse.
 */
void qnx_mouse_position(int x, int y)
{
   short mx = 0, my = 0;

   pthread_mutex_lock(&qnx_events_mutex);
   
   _mouse_x = x;
   _mouse_y = y;
   
   if (ph_window_context) {
      PtGetAbsPosition(ph_window, &mx, &my);
   }
   
   PhMoveCursorAbs(PhInputGroup(NULL), x + mx, y + my);
   
   mymickey_x = mymickey_y = 0;

   qnx_mouse_warped = TRUE;
   
   pthread_mutex_unlock(&qnx_events_mutex);
}



/* qnx_mouse_set_range:
 *  Sets the range of the mickey-mode mouse.
 */
void qnx_mouse_set_range(int x1, int y1, int x2, int y2)
{
   mouse_minx = x1;
   mouse_miny = y1;
   mouse_maxx = x2;
   mouse_maxy = y2;

   DISABLE();

   _mouse_x = MID(mouse_minx, _mouse_x, mouse_maxx);
   _mouse_y = MID(mouse_miny, _mouse_y, mouse_maxy);

   ENABLE();
}



/* qnx_mouse_set_speed:
 *  Sets the speed of the mickey-mode mouse.
 */
void qnx_mouse_set_speed(int xspeed, int yspeed)
{
   /* Use xset utility with "m" option.  */
}



/* qnx_mouse_get_mickeys:
 *  Reads the mickey-mode count.
 */
void qnx_mouse_get_mickeys(int *mickeyx, int *mickeyy)
{
   int temp_x = mymickey_x;
   int temp_y = mymickey_y;

   mymickey_x -= temp_x;
   mymickey_y -= temp_y;

   *mickeyx = temp_x;
   *mickeyy = temp_y;

//   _xwin_set_warped_mouse_mode(TRUE);
}

