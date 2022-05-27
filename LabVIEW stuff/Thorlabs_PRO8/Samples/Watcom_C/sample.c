/****************************************************************************

   Thorlabs PRO800/PRO8000 Instrument Driver Sample Application

   Source file    sample.c

   Date:          Apr-26-2010
   Software-Nr:   N/A
   Version:       1.0
   Copyright:     Copyright(c) 2010, Thorlabs GmbH (www.thorlabs.com)
   Author(s):     Michael Biebl (mbiebl@thorlabs.com)

   Disclaimer:

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA


   NOTE: You may need to set your compiler include search path to the
         VXIPNP include directory.
         This is typically 'C:\VXIPNP\WinNT\include' or
         'C:\Program Files\IVI Foundation\VISA\WinNT\include'

****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Set your compiler include search path to the VXIPNP\WinNt\include directory
#include "visa.h"
#include "PRO8.h"

#ifdef _CVI_
   #define EXIT_SUCCESS 0
   #define EXIT_FAILURE 1
#endif

/*===========================================================================
 Macros
===========================================================================*/
#define COMM_TIMEOUT             5000    // communications timeout in ms


/*===========================================================================
 Prototypes
===========================================================================*/
void error_exit(ViSession handle, ViStatus err);
void waitKeypress(void);
ViStatus find_instruments(ViString findPattern, ViChar **resource);
ViStatus get_device_id(ViSession handle);
ViStatus get_slot_config(ViSession instrHdl);

/*===========================================================================
 Functions
===========================================================================*/
int main(int argc, char **argv)
{
   ViStatus    err;
   ViChar      *rscPtr;
   ViSession   instrHdl = VI_NULL;

   printf("-----------------------------------------------------------\n");
   printf(" Thorlabs PRO800/PRO8000 Series instrument driver sample \n");
   printf("-----------------------------------------------------------\n\n");

   // Parameter checking / resource scanning
   if(argc < 2)
   {
      // Find resources
      err = find_instruments(PRO8_FIND_PATTERN, &rscPtr);
      if(err) error_exit(instrHdl, err);  // something went wrong
      if(!rscPtr) exit(EXIT_SUCCESS);     // none found
   }
   else
   {
      // Got resource in command line
      rscPtr = argv[1];
   }

   // Open session to instrument
   printf("Opening session to '%s' ...\n\n", rscPtr);
   err = Pro8_init(rscPtr, VI_ON, VI_ON, &instrHdl);
   if(err) error_exit(instrHdl, err);  // can not open session to instrument

   // Get instrument info
   err = get_device_id(instrHdl);
   if(err) error_exit(instrHdl, err);

   // Get slot config
   err = get_slot_config(instrHdl);
   if(err) error_exit(instrHdl, err);

   // Close session to instrument
   Pro8_close (instrHdl);

   waitKeypress();
   return (EXIT_SUCCESS);
}


/*---------------------------------------------------------------------------
 Read out device ID and print it to screen
---------------------------------------------------------------------------*/
ViStatus get_device_id(ViSession instrHdl)
{
   ViStatus err;
   ViChar   nameBuf[256];
   ViChar   fwRevBuf[256];
   ViChar   drvRevBuf[256];

   err = Pro8_identificationQuery (instrHdl, VI_NULL, nameBuf, VI_NULL, fwRevBuf);
   if(err) return(err);
   printf("Instrument:    %s\n", nameBuf);
   printf("Firmware:      %s\n", fwRevBuf);

   err = Pro8_revisionQuery (instrHdl, drvRevBuf, VI_NULL);
   if(err) return(err);
   printf("Driver:        %s\n\n", drvRevBuf);

   return(VI_SUCCESS);
}


/*---------------------------------------------------------------------------
 Read out slot configuration
---------------------------------------------------------------------------*/
ViStatus get_slot_config(ViSession instrHdl)
{
   ViStatus err;
   int      i;
   ViChar   idBuf[256];
   ViInt16  type[8];

   err = Pro8_GetSlotConfiguration (instrHdl, &type[0], VI_NULL, &type[1], VI_NULL, &type[2], VI_NULL, &type[3], VI_NULL, &type[4], VI_NULL, &type[5], VI_NULL, &type[6], VI_NULL, &type[7], VI_NULL);
   if(err) return(err);

   for(i = 0; i < PRO8_NUM_SLOTS; i++)
   {
      if(type[i] != 0)
      {
         err = Pro8_SetSlot (instrHdl, i +1);
         if(err) return(err);
         
         err = Pro8_SlotDataQuery (instrHdl, VI_NULL, VI_NULL, idBuf, VI_NULL, VI_NULL, VI_NULL, VI_NULL, VI_NULL, VI_NULL, VI_NULL, VI_NULL, VI_NULL, VI_NULL);
         if(err) return(err);

         printf("Slot %d:        %s\n", i +1, idBuf);
      }
      else
      {
         printf("Slot %d:        empty\n", i +1);
      }
   }
   printf("\n");
   return(VI_SUCCESS);
}


/*---------------------------------------------------------------------------
  Find Instruments
---------------------------------------------------------------------------*/
ViStatus find_instruments(ViString findPattern, ViChar **resource)
{
   ViStatus       err;
   ViSession      resMgr, instr;
   ViFindList     findList;
   ViUInt32       findCnt, cnt;
   ViChar         *rscBuf, *rscPtr;
   static ViChar  rscStr[VI_FIND_BUFLEN];
   ViChar         name[256], sernr[256];
   int            i, done;

   printf("Scanning for instruments ...\n");

   if((err = viOpenDefaultRM(&resMgr))) return(err);
   switch((err = viFindRsrc(resMgr, findPattern, &findList, &findCnt, rscStr)))
   {
      case VI_SUCCESS:
         break;

      case VI_ERROR_RSRC_NFOUND:
         viClose(resMgr);
         printf("No matching instruments found\n\n");
         return (err);

      default:
         viClose(resMgr);
         return (err);
   }

   if(findCnt < 2)
   {
      // Found only one matching instrument - return this
      *resource = rscStr;
      viClose(findList);
      viClose(resMgr);
      return (VI_SUCCESS);
   }

   // Found multiple instruments - get resource strings of all instruments into buffer
   if((rscBuf = malloc(findCnt * VI_FIND_BUFLEN)) == NULL) return (VI_ERROR_SYSTEM_ERROR);
   rscPtr = rscBuf;
   strncpy(rscBuf, rscStr, VI_FIND_BUFLEN); // copy first found instrument resource string
   rscPtr += VI_FIND_BUFLEN;

   for(cnt = 1; cnt < findCnt; cnt++)
   {
      if((err = viFindNext (findList, rscPtr)))
      {
         free(rscBuf);
         return(err);
      }
      rscPtr += VI_FIND_BUFLEN;
   }
   viClose(findList);

   // Display selection
   done = 0;
   do
   {
      printf("Found %d matching instruments:\n\n", findCnt);
      rscPtr = rscBuf;

      for(cnt = 0; cnt < findCnt; cnt++)
      {
         // Open resource
         if((err = viOpen (resMgr, rscPtr, VI_NULL, COMM_TIMEOUT, &instr)) != 0)
         {
            viClose(resMgr);
            free(rscBuf);
            return(err);

         }
         // Get attribute data
         viGetAttribute(instr,VI_ATTR_MODEL_NAME,      name);
         viGetAttribute(instr,VI_ATTR_USB_SERIAL_NUM,  sernr);
         // Closing
         viClose(instr);

         // Print out
         printf("% d: %s \tS/N:%s\n", cnt+1, name, sernr);
         rscPtr += VI_FIND_BUFLEN;
      }

      printf("\nPlease select: ");
      while((i = getchar()) == EOF);
      i -= '0';
      fflush(stdin);
      printf("\n");
      if((i < 1) || (i > cnt))
      {
         printf("Invalid selection\n\n");
      }
      else
      {
         done = 1;
      }
   }
   while(!done);

   // Copy resource string to static buffer
   strncpy(rscStr, rscBuf + ((i-1) * VI_FIND_BUFLEN), VI_FIND_BUFLEN);
   *resource = rscStr;

   // Cleanup
   viClose(resMgr);
   free(rscBuf);
   return (VI_SUCCESS);
}


/*---------------------------------------------------------------------------
  Exit with error message
---------------------------------------------------------------------------*/
void error_exit(ViSession instrHdl, ViStatus err)
{
   ViChar   buf[1024];

   // Get error description and print out error
   Pro8_errorMessage (instrHdl, err, buf);
   fprintf(stderr, "ERROR: %s\n", buf);

   // close session to instrument if open
   if(instrHdl != VI_NULL)
   {
      Pro8_close(instrHdl);
   }

   // exit program
   waitKeypress();
   exit (EXIT_FAILURE);
}


/*---------------------------------------------------------------------------
  Print keypress message and wait
---------------------------------------------------------------------------*/
void waitKeypress(void)
{
   printf("Press <ENTER> to exit\n");
   while(getchar() == EOF);
}


/****************************************************************************
  End of Source file
****************************************************************************/
