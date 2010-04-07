/*
 * file: main.c
 * date: 2009-11-03
 * description: main file for the ad-hoc network (RoboNet)
 */



#include "hal.h"
#include "trans.h"
#include "nwk.h"



void main()
{
	// init
	hal_init();
	nwk_init();
	trans_init();

	while (1)
	{
		nwk_main();

		if ( trans_frm_avail() )
		{
			trans_frm_parse();
		}

	}

}

