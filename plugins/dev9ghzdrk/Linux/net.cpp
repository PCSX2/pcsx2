/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "net.h"
#include "../DEV9.h"

#include <pthread.h>
#include <unistd.h>

NetAdapter* nif;
pthread_t rx_thread;

volatile bool RxRunning=false;

void *NetRxThread(void *arg)
{
    NetPacket tmp;
    while(RxRunning)
    {
        while(rx_fifo_can_rx() && nif->recv(&tmp))
        {
            rx_process(&tmp);
        }

    }

    return 0;
}

void tx_put(NetPacket* pkt)
{
    nif->send(pkt);
    //pkt must be copied if its not processed by here, since it can be allocated on the callers stack
}

void InitNet(NetAdapter* ad)
{
    nif=ad;
    RxRunning=true;

       pthread_attr_t thAttr;
       int policy = 0;
       int max_prio_for_policy = 0;


       rx_thread = pthread_create(&rx_thread, NULL, NetRxThread, NULL);
       pthread_attr_init(&thAttr);
       pthread_attr_getschedpolicy(&thAttr, &policy);
       max_prio_for_policy = sched_get_priority_max(policy);


       pthread_setschedprio(rx_thread, max_prio_for_policy);
       pthread_attr_destroy(&thAttr);
}

void TermNet()
{
    if(RxRunning)
    {
        RxRunning = false;
        emu_printf("Waiting for RX-net thread to terminate..");
            pthread_join(rx_thread,NULL);
        emu_printf(".done\n");

        delete nif;
    }
}
