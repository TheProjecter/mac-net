/************************************************
 * file: main.c
 * date: 2010-05-21
 * description: main file for the ad-hoc network (RoboNet)
 ************************************************/

#include "hal.h"
#include "mac.h"
#include "trans.h"

void main()
{
	// Initialize RobotNet stack
	hal_init();
	mac_init();
	trans_init();

	// Main loop
	while (1)
	{
		// Keep stack rolling
		mac_FSM();

		// Deal with newly frame?
		if ( trans_frm_avail() )
		{
			trans_frm_parse();
		}
	}
}

