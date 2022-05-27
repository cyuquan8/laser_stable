/****************************************************************************

   Thorlabs PRO800/8000 series VXIpnp instrument driver

   FOR DETAILED DESCRIPTION OF THE DRIVER FUNCTIONS SEE THE ONLINE HELP FILE
   AND THE PROGRAMMERS REFERENCE MANUAL.

   Copyright:  Copyright(c) 1999-2010, Thorlabs (www.thorlabs.com)
   Author:     Michael Biebl (mbiebl@thorlabs.com)

   Disclaimer:

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


   Source file

   Date:          Jun-28-2010
   Built with:    NI LabWindows/CVI 9.1.0
   Software-Nr:   09.178.xxx
   Version:       2.5.3

   Changelog:     see 'readme.rtf'

****************************************************************************/


#include <visa.h>
#include <string.h>
#include <utility.h>
#include <formatio.h>
#include "pro8.h"

#define PRO8_DRIVER_REVISION           "2.5.3"              // Instrument driver revision
#define BUFFER_SIZE                    512L                 // File I/O buffer size
#define CMD_BUF_SIZE                   128                  // Buffer size of the command format buffer

#define PRO8_ID_RESPONSE_COMPANY_A     "PROFILE"            // The device has to respond this string as the company name
#define PRO8_ID_RESPONSE_COMPANY_B     "THORLABS"           // The device has to respond this string as the company name
#define PRO8_ID_RESPONSE_COMPANY_C     "THORLABS INC"       // The device has to respond this string as the company name
#define PRO8_ID_RESPONSE_COMPANY_D     "THORLABS GMBH"      // The device has to respond this string as the company name

#define PRO8_ID_RESPONSE_PRO8xxx       "PRO8xxx"            // The device has to respond one of this strings as the device name
#define PRO8_ID_RESPONSE_PRO8000       "PRO8000"
#define PRO8_ID_RESPONSE_PRO8000_4     "PRO8000-4"
#define PRO8_ID_RESPONSE_PRO800        "PRO800 "

#define PRO8_ERR_NOT_AVAILABLE         "not available"      // Text in functon error query if device is in Answer Value Mode
#define PRO8_POLL_STB_LOOP_COUNTER     100                  // Max number of loops in function Pro8_WaitForFinBit(...)
#define PRO8_POLL_STB_LOOP_DELAY       0.05                 // Delay (seconds) in function Pro8_WaitForFinBit()
#define PRO8_GET_ERROR_LOOP_COUNTER    50                   // Max number of loops in function Pro8_GetInstrumentError(...)


//===========================================================================
// Pro8_stringValPair is used in the unigpb_errorMessage function
//===========================================================================
typedef struct  Pro8_stringValPair
{
   ViStatus stringVal;
   ViString stringName;
}  Pro8_tStringValPair;


//===========================================================================
// UTILITY ROUTINE DECLARATIONS (Non-Exportable Functions)
//===========================================================================
ViBoolean Pro8_invalidViBooleanRange (ViBoolean val);
ViBoolean Pro8_invalidViInt16Range (ViInt16 val, ViInt16 min, ViInt16 max);
ViBoolean Pro8_invalidViInt32Range (ViInt32 val, ViInt32 min, ViInt32 max);
ViBoolean Pro8_invalidViUInt8Range (ViUInt8 val, ViUInt8 min, ViUInt8 max);
ViBoolean Pro8_invalidViUInt16Range (ViUInt16 val, ViUInt16 min, ViUInt16 max);
ViBoolean Pro8_invalidViUInt32Range (ViUInt32 val, ViUInt32 min, ViUInt32 max);
ViBoolean Pro8_invalidViReal32Range (ViReal32 val, ViReal32 min, ViReal32 max);
ViBoolean Pro8_invalidViReal64Range (ViReal64 val, ViReal64 min, ViReal64 max);
ViStatus Pro8_initCleanUp (ViSession openRMSession, ViPSession openInstrSession, ViStatus currentStatus);
ViStatus Pro8_WaitForFinBit (ViSession instrumentHandle, ViUInt16 *stb);
ViStatus Pro8_GetInstrumentError (ViSession instrumentHandle);


//===========================================================================
// USER-CALLABLE FUNCTIONS (Exportable Functions)
//===========================================================================
//===========================================================================
// GENERAL FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Initialize
// Purpose:  This function opens the instrument, queries the instrument
//           for its ID, and initializes the instrument to a known state.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_init (ViRsrc resourceName, ViBoolean IDQuery, ViBoolean resetDevice, ViPSession instrSession)
{
   ViStatus    status      = VI_SUCCESS;
   ViSession   rmSession   = 0;
   ViUInt32    retCnt      = 0;
   ViBoolean   match       = VI_FALSE;
   ViChar      companyBuf[BUFFER_SIZE],
               deviceBuf[BUFFER_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViBooleanRange (IDQuery))     return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViBooleanRange (resetDevice)) return VI_ERROR_PARAMETER3;
   //Open instrument session
   if ((status = viOpenDefaultRM (&rmSession)) < 0) return status;
   if ((status = viOpen (rmSession, resourceName, VI_NULL, VI_NULL, instrSession)) < 0)
   {
      viClose (rmSession);
      return status;
   }
   //Configure VISA Formatted I/O
   if ((status = viSetBuf (*instrSession, VI_READ_BUF|VI_WRITE_BUF, 4000)) < 0)                      return Pro8_initCleanUp (rmSession, instrSession, status);
   if ((status = viSetAttribute (*instrSession, VI_ATTR_WR_BUF_OPER_MODE, VI_FLUSH_ON_ACCESS)) < 0)  return Pro8_initCleanUp (rmSession, instrSession, status);
   if ((status = viSetAttribute (*instrSession, VI_ATTR_RD_BUF_OPER_MODE, VI_FLUSH_ON_ACCESS)) < 0)  return Pro8_initCleanUp (rmSession, instrSession, status);
   //Identification Query
   if (IDQuery)
   {
      if ((status = viWrite (*instrSession, "*IDN?", 5, &retCnt)) < 0)                                  return Pro8_initCleanUp (rmSession, instrSession, status);
      if ((status = viScanf (*instrSession, "%[^,], %[^,], %*[^,], %*[^\n]", companyBuf, deviceBuf))<0) return Pro8_initCleanUp (rmSession, instrSession, status);
      //Does Company match?
      if ( (CompareStrings (companyBuf, 0, PRO8_ID_RESPONSE_COMPANY_A, 0, 0) != 0) &&
           (CompareStrings (companyBuf, 0, PRO8_ID_RESPONSE_COMPANY_B, 0, 0) != 0) &&
           (CompareStrings (companyBuf, 0, PRO8_ID_RESPONSE_COMPANY_C, 0, 0) != 0) &&
           (CompareStrings (companyBuf, 0, PRO8_ID_RESPONSE_COMPANY_D, 0, 0) != 0) ) return Pro8_initCleanUp (rmSession, instrSession, VI_ERROR_FAIL_ID_QUERY);

      //Does Device match?
      if (CompareStrings (deviceBuf, 0, PRO8_ID_RESPONSE_PRO800 ,   0, 0) == 0)   match = VI_TRUE;
      if (CompareStrings (deviceBuf, 0, PRO8_ID_RESPONSE_PRO8000,   0, 0) == 0)   match = VI_TRUE;
      if (CompareStrings (deviceBuf, 0, PRO8_ID_RESPONSE_PRO8xxx,   0, 0) == 0)   match = VI_TRUE;
      if (CompareStrings (deviceBuf, 0, PRO8_ID_RESPONSE_PRO8000_4, 0, 0) == 0)   match = VI_TRUE;
      //Error
      if (match == VI_FALSE)  return Pro8_initCleanUp (rmSession, instrSession, VI_ERROR_FAIL_ID_QUERY);
   }
   //Remove existing instrument errors from the instruments error queue
   status = Pro8_GetInstrumentError(*instrSession);
   if ((status >= VI_INSTR_ERROR_OFFSET) && (status <= VI_INSTR_ERROR_MAXIMUM)) status = VI_SUCCESS;
   else return Pro8_initCleanUp (rmSession, instrSession, status);
   //Reset instrument
   if (resetDevice)
   {
      if ((status = Pro8_reset (*instrSession)) < 0)  return Pro8_initCleanUp (rmSession, instrSession, status);
   }
   //Answer Mode
   if ((status = Pro8_SetAnswerMode (*instrSession, 0 /* full mode */)) < 0) return Pro8_initCleanUp (rmSession, instrSession, status);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Slot
// Purpose:  This function sets the active slot
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SetSlot (ViSession instrumentHandle, ViInt16 slot)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (slot, 1, 8)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":SLOT %d", slot);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Slot
// Purpose:  This function returns the active slot
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetSlot (ViSession instrumentHandle, ViInt16 *slot)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":SLOT?", 6, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], slot)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Port
// Purpose:  This function sets the active port
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SetPort (ViSession instrumentHandle, ViInt16 port)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (port, 1, 8)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":PORT %d", port);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Port
// Purpose:  This function returns the active port
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetPort (ViSession instrumentHandle, ViInt16 *port)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":PORT?", 6, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], port)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Device Error Summary Register
// Purpose:  This function returns the Device Error Summary Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetDESR (ViSession instrumentHandle, ViInt32 *deviceErrorSummaryRegister)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %ld", "%ld"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":STAT:DESR?", 11, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], deviceErrorSummaryRegister)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Device Error Summary Enable Register
// Purpose:  This function sets the Device Error Summary Enable Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SetDESE (ViSession instrumentHandle, ViInt32 devErrorEventSumReg)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt32Range (devErrorEventSumReg, 0, 255)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":STAT:DESE %d", devErrorEventSumReg);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Device Error Summary Enable Register
// Purpose:  This function returns the Device Error Summary Enable Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetDESE (ViSession instrumentHandle, ViInt32 *devErrorSumEnableRegister)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %ld", "%ld"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":STAT:DESE?", 11, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], devErrorSumEnableRegister)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Device Error Condition Register
// Purpose:  This function returns the Device Error Condition Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetDEC (ViSession instrumentHandle, ViInt16 slot, ViInt32 *deviceErrorConditionRegister)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %ld", "%ld"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (slot, 1, 8)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":STAT:DEC%d?", slot);
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], deviceErrorConditionRegister)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Device Error Event Register
// Purpose:  This function returns the Device Error Event Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetDEE (ViSession instrumentHandle, ViInt16 slot, ViInt32 *deviceErrorEventRegister)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %ld", "%ld"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (slot, 1, 8)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":STAT:DEE%d?", slot);
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], deviceErrorEventRegister)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Device Error Summary Enable Register
// Purpose:  This function sets the Device Error Summary Enable Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SetEDE (ViSession instrumentHandle, ViInt16 slot, ViInt32 devErrorEventEnableReg)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (slot, 1, 8)) return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViInt32Range (devErrorEventEnableReg, 0, 65535)) return VI_ERROR_PARAMETER3;
   //Formatting
   Fmt (buffer, ":STAT:EDE%d %d", slot, devErrorEventEnableReg);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Device Error Event Register
// Purpose:  This function returns the Device Error Event Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetEDE (ViSession instrumentHandle, ViInt16 slot, ViInt32 *devErrorEventEnableRegister)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %ld", "%ld"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (slot, 1, 8)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":STAT:EDE%d?", slot);
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], devErrorEventEnableRegister)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Block Function Condition Register
// Purpose:  This function returns the Block Function Condition Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetBFC (ViSession instrumentHandle, ViInt32 *blockFunctionCondRegister)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %ld", "%ld"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":STAT:BFC?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], blockFunctionCondRegister)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Block Function Event Register
// Purpose:  This function returns the Block Function Event Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetBFR (ViSession instrumentHandle, ViInt32 *blockFunctionEventRegister)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %ld", "%ld"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":STAT:BFR?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], blockFunctionEventRegister)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Block Function Enable Register
// Purpose:  This function sets the Block Function Enable Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SetBFE (ViSession instrumentHandle, ViInt32 blockFunctionEnableRegister)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt32Range (blockFunctionEnableRegister, 0, 255)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":STAT:BFE %d", blockFunctionEnableRegister);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Block Function Enable Register
// Purpose:  This function returns the Block Function Enable Register
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetBFE (ViSession instrumentHandle, ViInt32 *blockFunctionEnableRegister)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %ld", "%ld"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":STAT:BFE?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], blockFunctionEnableRegister)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Write To Instrument
// Purpose:  This function writes a command string to the instrument
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_writeInstrData (ViSession instrumentHandle, ViString writeBuffer)
{
   ViStatus status = VI_SUCCESS;

   //Writing
   if ((status = viPrintf (instrumentHandle, "%s", writeBuffer)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Read Instrument Buffer
// Purpose:  This function reads the output buffer of the instrument.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_readInstrData (ViSession instrumentHandle, ViInt32 numberBytesToRead, ViChar _VI_FAR readBuffer[], ViInt32 *numBytesRead)
{
   ViStatus status = VI_SUCCESS;
   *numBytesRead   = 0L;

   //Reading
   if ((status = viRead (instrumentHandle, readBuffer, numberBytesToRead, numBytesRead)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Wait for FIN Bit (Bit0 in Status Byte)
// Purpose:  This function waits for the FIN (finished) Bit and returns
//           the status byte.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WaitForFinishedBit (ViSession instrumentHandle)
{
   ViUInt16 stb;

   return Pro8_WaitForFinBit (instrumentHandle, &stb);
}

//---------------------------------------------------------------------------
// Function: Reset
// Purpose:  This function resets the instrument.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_reset (ViSession instrumentHandle)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;

   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, "*RST", 4, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Self-Test
// Purpose:  This function executes the instrument self-test and returns
//           the result.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_selfTest (ViSession instrumentHandle, ViInt16 *selfTestResult)
{
   ViUInt32 retCnt = 0;
   ViStatus status = VI_SUCCESS;
   ViUInt16 stb;

   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, "*TST?", 5, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, "%d", selfTestResult)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Error Query
// Purpose:  This function queries the instrument error queue, and returns
//           the result.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_errorQuery (ViSession instrumentHandle, ViInt32 *errorCode, ViChar _VI_FAR errorMessage[])
{
   ViUInt32  retCnt = 0;
   ViStatus  status = VI_SUCCESS;
   ViUInt16  stb;
   ViBoolean answer;
   ViChar    msg[1024];

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing
   if ((status = viWrite (instrumentHandle, ":SYST:ERR?", 10, &retCnt)) < 0) return status;
   //Poll STB
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   //Reading
   if (answer)
   {
      if ((status = viScanf (instrumentHandle, "%ld", errorCode)) < 0) return status;
      if (errorMessage) CopyString (errorMessage, 0, PRO8_ERR_NOT_AVAILABLE, 0, -1);
   }
   else
   {
      if ((status = viScanf (instrumentHandle, "%ld, \"%[^\"]", errorCode, msg)) < 0) return status;
      if (errorMessage) CopyString (errorMessage, 0, msg, 0, -1);
   }
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Error Message
// Purpose:  This function translates the error return value from the
//           instrument driver into a user-readable string.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_errorMessage (ViSession instrumentHandle, ViStatus statusCode, ViChar _VI_FAR message[])
{
   ViStatus status = VI_SUCCESS;
   ViInt16 i;

   static Pro8_tStringValPair statusDescArray[] =
   {
      //Instrument Driver errors and warnings
      {VI_WARN_NSUP_ID_QUERY,                "WARNING: ID Query not supported"                                       },
      {VI_WARN_NSUP_RESET,                   "WARNING: Reset not supported"                                          },
      {VI_WARN_NSUP_SELF_TEST,               "WARNING: Self-test not supported"                                      },
      {VI_WARN_NSUP_ERROR_QUERY,             "WARNING: Error Query not supported"                                    },
      {VI_WARN_NSUP_REV_QUERY,               "WARNING: Revision Query not supported"                                 },
      {VI_ERROR_PARAMETER1,                  "ERROR: Parameter 1 out of range"                                       },
      {VI_ERROR_PARAMETER2,                  "ERROR: Parameter 2 out of range"                                       },
      {VI_ERROR_PARAMETER3,                  "ERROR: Parameter 3 out of range"                                       },
      {VI_ERROR_PARAMETER4,                  "ERROR: Parameter 4 out of range"                                       },
      {VI_ERROR_PARAMETER5,                  "ERROR: Parameter 5 out of range"                                       },
      {VI_ERROR_PARAMETER6,                  "ERROR: Parameter 6 out of range"                                       },
      {VI_ERROR_PARAMETER7,                  "ERROR: Parameter 7 out of range"                                       },
      {VI_ERROR_PARAMETER8,                  "ERROR: Parameter 8 out of range"                                       },
      {VI_ERROR_FAIL_ID_QUERY,               "ERROR: Identification query failed"                                    },
      {VI_ERROR_INV_RESPONSE,                "ERROR: Interpreting instrument response"                               },
      {VI_ERROR_INSTR_FILE_OPEN,             "ERROR: Opening the specified file"                                     },
      {VI_ERROR_INSTR_FILE_WRITE,            "ERROR: Writing to the specified file"                                  },
      {VI_ERROR_INSTR_INTERPRETING_RESPONSE, "ERROR: Interpreting the instrument's response"                         },
      {VI_ERROR_POLL_FIN_BIT,                "ERROR: Polling FIN Bit"                                                },
      {VI_ERROR_GET_INSTR_ERROR,             "ERROR: Query Instrument Error Queue"                                   },
      //Instrument specific error codes
      {PRO8_ERR_NO_ERROR,                    "ERROR: No Error"                                                       },
      {PRO8_ERR_UNKNOWN_COMMAND,             "ERROR: Unknown command"                                                },
      {PRO8_ERR_INVALID_CHARACTER,           "ERROR: Invalid character"                                              },
      {PRO8_ERR_INVALID_NUMERIC,             "ERROR: Invalid numeric parameter"                                      },
      {PRO8_ERR_INVALID_TEXT,                "ERROR: Invalid text parameter"                                         },
      {PRO8_ERR_MISSING_PARAMETER,           "ERROR: Missing parameter"                                              },
      {PRO8_ERR_INVALID_SEPARATOR,           "ERROR: Invalid separator"                                              },
      {PRO8_ERR_EMPTY_SLOT,                  "ERROR: Empty slot"                                                     },
      {PRO8_ERR_SET_PARAMETER,               "ERROR: Parameter can not be set"                                       },
      {PRO8_ERR_WRONG_COMPOUND,              "ERROR: Wrong compound"                                                 },
      {PRO8_ERR_UNKNOWN_COMPOUND,            "ERROR: Unknown compound"                                               },
      {PRO8_ERR_WRONG_PARAMETER,             "ERROR: Wrong parameter"                                                },
      {PRO8_ERR_WRONG_CMD_FOR_PORT,          "ERROR: Wrong command for the selected port"                            },
      {PRO8_ERR_PARSER_OVERFLOW,             "ERROR: Parser buffer overflow"                                         },
      {PRO8_ERR_OUT_OF_RANGE,                "ERROR: Data out of range"                                              },
      {PRO8_ERR_HARDWARE,                    "ERROR: Hardware error"                                                 },
      {PRO8_ERR_SOFTWARE,                    "ERROR: Software error"                                                 },
      {PRO8_ERR_NOT_IMPLEMENTED,             "ERROR: Not implemented jet"                                            },
      {PRO8_ERR_UPDATE_REQUIRED,             "ERROR: Update required"                                                },
      {PRO8_ERR_ELCH_SET_INCOMPLETE,         "ERROR: ELCH set value initialisation not complete"                     },
      {PRO8_ERR_ELCH_READ_INCOMPLETE,        "ERROR: ELCH read value(s) initialisation not complete"                 },
      {PRO8_ERR_EE_WRITE_FAILED,             "ERROR: EEPROM write failed"                                            },
      {PRO8_ERR_EE_READ_FAILED,              "ERROR: EEPROM read back failed"                                        },
      {PRO8_ERR_EE_COMPARE_FAILED,           "ERROR: EEPROM compare failed"                                          },
      {PRO8_ERR_QUEUE_OVERFLOW,              "ERROR: Too many errors"                                                },
      {PRO8_ERR_QUERY_INTERRUPTED,           "ERROR: Query interrupted"                                              },
      {PRO8_ERR_QUERY_UNTERMINATED,          "ERROR: Query unterminated"                                             },
      {PRO8_ERR_GBPI_RECEIVE_OVERFLOW,       "ERROR: IEEE receive buffer overflow"                                   },
      {PRO8_ERR_SERIAL_RECEIVE_OVERFLOW,     "ERROR: Serial receive buffer overflow"                                 },
      {PRO8_ERR_SERIAL_SEND_TIMEOUT,         "ERROR: Serial send timeout occoured"                                   },
      //LDC module specific errors
      {PRO8_ERR_LDC_INTERLOCK,               "ERROR: Interlock is open"                                              },
      {PRO8_ERR_LDC_OPEN,                    "ERROR: Open circuit"                                                   },
      {PRO8_ERR_LDC_OTP,                     "ERROR: Over temperature"                                               },
      {PRO8_ERR_LDC_VCC,                     "ERROR: Internal power failure"                                         },
      {PRO8_ERR_LDC_NO_LDPOL_WHILE_ON,       "ERROR: No LD polarity change during laser on"                          },
      {PRO8_ERR_LDC_NO_PDPOL_WHILE_ON,       "ERROR: No PD polarity change during laser on"                          },
      {PRO8_ERR_LDC_NO_ILD_WHILE_CP,         "ERROR: No setting of ILD during constant power mode"                   },
      {PRO8_ERR_LDC_NO_IMD_WHILE_CC,         "ERROR: No setting of IMD during constant current mode"                 },
      {PRO8_ERR_LDC_NO_WIN_WHILE_LDON,       "ERROR: Attempt to activate TWIN durin laser on"                        },
      {PRO8_ERR_LDC_WIN,                     "ERROR: Attempt to switch on laser while temperature is out of window"  },
      {PRO8_ERR_LDC_NO_TEC_PRESENT,          "ERROR: Attempt to activate Twin although there is no TEC in the system"},
      {PRO8_ERR_LDC_NO_PDPOL_WHILE_BIAS,     "ERROR: No PD polarity change during bias on"                           },
      {PRO8_ERR_LDC_NO_CALPD_WHILE_LDON,     "ERROR: No calibrating of PD during laser on"                           },
      {PRO8_ERR_LDC_NO_TEC_IN_SLOT,          "ERROR: No TEC in this slot"                                            },
      {PRO8_ERR_LDC_NO_MODE_WHILE_LDON,      "ERROR: No mode change during laser on"                                 },
      {PRO8_ERR_LDC_NO_BIAS_WHILE_ON_CP,     "ERROR: No bias change during laser on in constant power mode"          },
      //MLC module specific errors
      {PRO8_ERR_MLC_INTERLOCK,               "ERROR: Interlock is open"                                              },
      {PRO8_ERR_MLC_OTP,                     "ERROR: Over temperature"                                               },
      {PRO8_ERR_MLC_VCC,                     "ERROR: Internal power failure"                                         },
      {PRO8_ERR_MLC_NO_ILD_WHILE_CP,         "ERROR: No setting of ILD during constant power mode"                   },
      {PRO8_ERR_MLC_NO_IMD_WHILE_CC,         "ERROR: No setting of IMD during constant current mode"                 },
      {PRO8_ERR_MLC_NO_MODE_WHILE_LDON,      "ERROR: No mode change during laser on"                                 },
      {PRO8_ERR_MLC_NO_RANGE_WHILE_LDON,     "ERROR: No range change during laser on"                                },
      {PRO8_ERR_MLC_WIN,                     "ERROR: Attempt to switch on laser while temperature is out of window"  },
      {PRO8_ERR_MLC_NO_TEC_PRESENT,          "ERROR: Attempt to activate Twin although there is no TEC in the system"},
      {PRO8_ERR_MLC_NO_WIN_WHILE_LDON,       "ERROR: Attempt to activate TWIN durin laser on"                        },
      {PRO8_ERR_MLC_NO_TEC_IN_SLOT,          "ERROR: No TEC in this slot"                                            },
      //TEC module specific errors
      {PRO8_ERR_TEC_OTP,                     "ERROR: Over temperature"                                               },
      {PRO8_ERR_TEC_NO_SENSOR,               "ERROR: Wrong or no sensor"                                             },
      {PRO8_ERR_TEC_NO_CALTR_WHILE_TECON,    "ERROR: No calibrating of thermistor during TEC on"                     },
      {PRO8_ERR_TEC_WRONG_CMD_SENSOR,        "ERROR: Wrong command for this sensor"                                  },
      {PRO8_ERR_TEC_NO_SENSOR_WHILE_TECON,   "ERROR: No sensor change during TEC on allowed"                         },
      {PRO8_ERR_TEC_COMMAND_NOT_VALID,       "ERROR: Command not valid for this module"                              },
      //ITC module specific errors
      {PRO8_ERR_ITC_INTERLOCK,               "ERROR: Interlock is open"                                              },
      {PRO8_ERR_ITC_OPEN,                    "ERROR: Open circuit"                                                   },
      {PRO8_ERR_ITC_OTP,                     "ERROR: Over temperature"                                               },
      {PRO8_ERR_ITC_VCC,                     "ERROR: Internal power failure"                                         },
      {PRO8_ERR_ITC_NO_CALTR_WHILE_TECON,    "ERROR: No calibrating of thermistor during TEC on"                     },
      {PRO8_ERR_ITC_NO_CALPD_WHILE_LDON,     "ERROR: No calibrating of PD during laser on"                           },
      {PRO8_ERR_ITC_NO_LDPOL_WHILE_ON,       "ERROR: No LD polarity change during laser on"                          },
      {PRO8_ERR_ITC_NO_PDPOL_WHILE_ON,       "ERROR: No PD polarity change during laser on"                          },
      {PRO8_ERR_ITC_NO_ILD_WHILE_CP,         "ERROR: No setting of ILD during constant power mode"                   },
      {PRO8_ERR_ITC_NO_IMD_WHILE_CC,         "ERROR: No setting of IMD during constant current mode"                 },
      {PRO8_ERR_ITC_NO_MODE_WHILE_LDON,      "ERROR: No mode change during laser on"                                 },
      {PRO8_ERR_ITC_NO_SENSOR,               "ERROR: Wrong or no sensor"                                             },
      {PRO8_ERR_ITC_WRONG_CMD_SENSOR,        "ERROR: Wrong command for this sensor"                                  },
      {PRO8_ERR_ITC_NO_SENSOR_WHILE_TECON,   "ERROR: No sensor change during TEC on allowed"                         },
      {PRO8_ERR_ITC_WIN,                     "ERROR: Attempt to switch on laser while temperature is out of window"  },
      {PRO8_ERR_ITC_NO_WIN_WHILE_LDON,       "ERROR: Attempt to activate TWIN durin laser on"                        },
      //PDA module specific errors
      {PRO8_ERR_PDA_NO_BIAS_WHILE_NEGATIVE,  "ERROR: Attempt to switch on BIAS while photo current is negative"      },
      {PRO8_ERR_PDA_NO_PDPOL_WHILE_BIAS,     "ERROR: Attempt to change polarity while BIAS is on"                    },
      {PRO8_ERR_PDA_NO_RANGE_WHILE_BIAS,     "ERROR: Attempt to change measurement range while BIAS is on"           },
      {PRO8_ERR_PDA_NO_UFWD_WHILE_BIAS,      "ERROR: Attempt to measure Ufwd while BIAS is on"                       },
      {PRO8_ERR_PDA_UFWD_ACCURACY,           "ERROR: Ufwd measurement failed - accuracy error"                       },
      {PRO8_ERR_PDA_UFWD_OVERFLOW,           "ERROR: Ufwd measurement failed - voltage overflow"                     },
      //LS/WDM module specific Error Codes
      {PRO8_ERR_WDM_OTP,                     "ERROR: Over temperature"                                               },
      {PRO8_ERR_WDM_VCC,                     "ERROR: Internal power failure"                                         },
      {PRO8_ERR_WDM_WIN,                     "ERROR: Temperature is out of window"                                   },
      {PRO8_ERR_WDM_SHUTTER,                 "ERROR: Shutter error - fiber missing"                                  },
      {PRO8_ERR_WDM_INVALID_CMD,             "ERROR: Command not valid for this module"                              },
      {PRO8_ERR_WDM_SERVICE_MODE_1,          "ERROR: Service-Mode Error 1"                                           },
      {PRO8_ERR_WDM_SERVICE_MODE_2,          "ERROR: Service-Mode Error 2"                                           },
      {PRO8_ERR_WDM_POWER_CHANGE_MOD,        "ERROR: No power change during modulation on allowed"                   },
      {PRO8_ERR_WDM_CALIBRATION,             "ERROR: Calibration required (module is not calibrated)"                },
      {PRO8_ERR_WDM_MOD_TYPE_CHANGE_MOD,     "ERROR: No change of LF-modulation type while LF-modulation is on"      },
      {PRO8_ERR_WDM_NO_MOD_AMPLITUDE,        "ERROR: No LF-modulation possible - amplitude too high"                 },
      {PRO8_ERR_WDM_NOT_IN_SERVICE_MODE,     "ERROR: Command not valid in service mode"                              },
      {PRO8_ERR_WDM_ONLY_IN_SERVICE_MODE,    "ERROR: Command only valid in service mode"                             },
      {PRO8_ERR_WDM_NO_POWER_TUNE,           "ERROR: No output power tune for this module"                           },
      //Additional module specific errors
      {VI_NULL, VI_NULL}
   };
   //Search
   status = viStatusDesc (instrumentHandle, statusCode, message);
   if (status == VI_WARN_UNKNOWN_STATUS)
   {
      for (i=0; statusDescArray[i].stringName; i++)
      {
         if (statusDescArray[i].stringVal == statusCode)
         {
            CopyString (message, 0, statusDescArray[i].stringName, 0, -1);
            return (VI_SUCCESS);
         }
      }
      Fmt (message, "Unknown Error 0x%x[w8p0]", statusCode);
      return (VI_WARN_UNKNOWN_STATUS);
   }
   //Ready
   status = VI_SUCCESS;
   return status;
}

//---------------------------------------------------------------------------
// Function: Identification Query
// Purpose:  This function returns the instrument identification strings.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_identificationQuery (ViSession instrumentHandle, ViChar _VI_FAR manufacturerName[], ViChar _VI_FAR instrumentName[], ViChar _VI_FAR instrumentSerialNumber[], ViChar _VI_FAR firmwareRevision[])
{
   ViUInt32 retCnt = 0;
   ViStatus status = VI_SUCCESS;
   ViUInt16 stb;
   ViChar   manuf[256], name[256], sn[256], rev[256];

   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, "*IDN?", 5, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, "%[^,], %[^,], %[^,], %[^\n]", manuf, name, sn, rev)) < 0) return status;
   //Ready
   if (manufacturerName)         CopyString (manufacturerName, 0, manuf, 0, -1);
   if (instrumentName)           CopyString (instrumentName, 0, name, 0, -1);
   if (instrumentSerialNumber)   CopyString (instrumentSerialNumber, 0, sn, 0, -1);
   if (firmwareRevision)         CopyString (firmwareRevision, 0, rev, 0, -1);

   
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Slot Configuration
// Purpose:  This function returns the type and subtype codes of all slots.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetSlotConfiguration (ViSession instrumentHandle, ViPInt16 type1, ViPInt16 sub1, ViPInt16 type2, ViPInt16 sub2, ViPInt16 type3, ViPInt16 sub3, ViPInt16 type4, ViPInt16 sub4,
                     ViPInt16 type5, ViPInt16 sub5, ViPInt16 type6, ViPInt16 sub6, ViPInt16 type7, ViPInt16 sub7, ViPInt16 type8, ViPInt16 sub8)
{
   ViUInt32  retCnt = 0;
   ViStatus  status = VI_SUCCESS;
   ViUInt16  stb;
   ViInt16   t0, s0, t1, s1, t2, s2, t3, s3, t4, s4, t5, s5, t6, s6, t7, s7;
   ViBoolean answer;
   ViString  format[] = {"%*s %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":CONFIG:PLUG?", 13, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], &t0, &s0, &t1, &s1, &t2, &s2, &t3, &s3, &t4, &s4, &t5, &s5, &t6, &s6, &t7, &s7)) < 0) return status;

   if(type1) *type1 = t0;
   if(type2) *type2 = t1;
   if(type3) *type3 = t2;
   if(type4) *type4 = t3;
   if(type5) *type5 = t4;
   if(type6) *type6 = t5;
   if(type7) *type7 = t6;
   if(type8) *type8 = t7;
   
   if(sub1) *sub1 = s0;
   if(sub2) *sub2 = s1;
   if(sub3) *sub3 = s2;
   if(sub4) *sub4 = s3;
   if(sub5) *sub5 = s4;
   if(sub6) *sub6 = s5;
   if(sub7) *sub7 = s6;
   if(sub8) *sub8 = s7;
   
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Slot Data Query
// Purpose:  This function returns the slot data.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SlotDataQuery (ViSession instrumentHandle, ViPInt16 type, ViPInt16 sub, ViChar _VI_FAR ident[], ViPInt16 opt1, ViPInt16 opt2,
                     ViPInt16 opt3, ViPInt16 opt4, ViPInt16 opt5, ViPInt16 opt6, ViPInt16 opt7, ViPInt16 opt8, ViPInt16 opt9, ViPInt16 opt10)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViInt16   o[10], t, s;
   ViChar    id[256];
   ViBoolean answer;
   ViString  format1[] = {"%*s %d", "%d"};
   ViString  format2[] = {"%*s %d", "%d"};
   ViString  format3[] = {"%*s \"%[^\"]", "\"%[^\"]"};
   ViString  format4[] = {"%*s %d,%d,%d,%d,%d,%d,%d,%d,%d,%d", "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:ID?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format1[answer], &t)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:SUB?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format2[answer], &s)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:TXT?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format3[answer], id)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:OPT?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format4[answer], &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7], &o[8], &o[9])) < 0) return status;
   //Ready
   if(type)    *type  = t;
   if(sub)     *sub   = s;
   if(ident)   strcpy(ident, id);
   if(opt1)    *opt1  = o[0];
   if(opt2)    *opt2  = o[1];
   if(opt3)    *opt3  = o[2];
   if(opt4)    *opt4  = o[3];
   if(opt5)    *opt5  = o[4];
   if(opt6)    *opt6  = o[5];
   if(opt7)    *opt7  = o[6];
   if(opt8)    *opt8  = o[7];
   if(opt9)    *opt9  = o[8];
   if(opt10)   *opt10 = o[9];

   return status;
}

//---------------------------------------------------------------------------
// Function: Slot Data Query Extended
// Purpose:  This function returns the slot data in extended version.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SlotDataQueryEx (ViSession instrumentHandle, ViPInt16 type, ViPInt16 sub, ViChar _VI_FAR ident[], ViChar _VI_FAR sernr[],
                     ViPInt16 opt1, ViPInt16 opt2, ViPInt16 opt3, ViPInt16 opt4, ViPInt16 opt5, ViPInt16 opt6, ViPInt16 opt7, ViPInt16 opt8, ViPInt16 opt9, ViPInt16 opt10)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViInt16   o[10], t, s;
   ViChar    id[256], sn[256];
   ViBoolean answer;
   ViString  format1[] = {"%*s %d", "%d"};
   ViString  format2[] = {"%*s %d", "%d"};
   ViString  format3[] = {"%*s \"%[^\"]", "\"%[^\"]"};
   ViString  format4[] = {"%*s %s", "%s"};
   ViString  format5[] = {"%*s %d,%d,%d,%d,%d,%d,%d,%d,%d,%d", "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:ID?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format1[answer], &t)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:SUB?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format2[answer], &s)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:TXT?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format3[answer], id)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:SN?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format4[answer], sn)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TYPE:OPT?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format5[answer], &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7], &o[8], &o[9])) < 0) return status;
   //Ready
   if(type)    *type  = t;
   if(sub)     *sub   = s;
   if(ident)   strcpy(ident, id);
   if(sernr)   strcpy(sernr, sn);
   if(opt1)    *opt1  = o[0];
   if(opt2)    *opt2  = o[1];
   if(opt3)    *opt3  = o[2];
   if(opt4)    *opt4  = o[3];
   if(opt5)    *opt5  = o[4];
   if(opt6)    *opt6  = o[5];
   if(opt7)    *opt7  = o[6];
   if(opt8)    *opt8  = o[7];
   if(opt9)    *opt9  = o[8];
   if(opt10)   *opt10 = o[9];
   return status;
}

//---------------------------------------------------------------------------
// Function: Revision Query
// Purpose:  This function returns the driver and instrument revisions.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_revisionQuery (ViSession instrumentHandle, ViChar _VI_FAR instrumentDriverRevision[], ViChar _VI_FAR firmwareRevision[])
{
   ViUInt32 retCnt = 0;
   ViStatus status = VI_SUCCESS;
   ViUInt16 stb;

   if(firmwareRevision)
   {
      //Writing - Poll STB - Check EAV Bit and read error - Reading
      if ((status = viWrite (instrumentHandle, "*IDN?", 5, &retCnt)) < 0) return status;
      if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
      if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
      if ((status = viScanf (instrumentHandle, "%*[^,], %*[^,], %*[^,], %[^\n]", firmwareRevision)) < 0) return status;
   }
   if(instrumentDriverRevision) CopyString (instrumentDriverRevision, 0, PRO8_DRIVER_REVISION, 0, -1);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Answer Mode
// Purpose:  This function sets the instruments answer mode.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SetAnswerMode (ViSession instrumentHandle, ViBoolean answerMode)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":SYST:ANSW FULL", ":SYST:ANSW VALUE"};

   //Check input parameter ranges
   if (Pro8_invalidViBooleanRange (answerMode)) return VI_ERROR_PARAMETER2;
   //Writing
   if ((status = viWrite (instrumentHandle, command[answerMode], StringLength (command[answerMode]), &retCnt)) < 0) return status;
   //Store Data
   if ((status = viSetAttribute (instrumentHandle, VI_ATTR_USER_DATA, (ViAttrState)answerMode)) < 0) return status;
   //Poll STB - Check EAV Bit and read error
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Answer Mode
// Purpose:  This function gets the instruments answer mode and synchronizes
//           the instrument dirver settings.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_GetAnswerMode (ViSession instrumentHandle, ViBoolean *answerMode)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":SYST:ANSW?", 11, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, "%s", buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings (":SYST:ANSW FULL",  0, buffer, 0, 0) == 0) *answerMode = 0;
   else if (CompareStrings ("VALUE", 0, buffer, 0, 0) == 0)            *answerMode = 1;
   else return VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Store Data
   if ((status = viSetAttribute (instrumentHandle, VI_ATTR_USER_DATA, (ViAttrState)answerMode)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Close
// Purpose:  This function closes the instrument.
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_close (ViSession instrumentHandle)
{
   ViSession rmSession;
   ViStatus  status = VI_SUCCESS;
   ViBoolean answer;

   //Setting answer mode to full mode
   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   if (answer) Pro8_SetAnswerMode (instrumentHandle, 0);
   //closing
   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_RM_SESSION, &rmSession)) < 0) return status;
   status = viClose (instrumentHandle);
   viClose (rmSession);
   //Ready
   return status;
}

//===========================================================================
// OSW MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set State
// Purpose:  This function sets the switch state
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_OswSetState (ViSession instrumentHandle, ViInt16 state)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (state, 1, 8)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":OSW %d", state);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get State
// Purpose:  This function returns the switch state
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_OswGetState (ViSession instrumentHandle, ViInt16 *state)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":OSW?", 5, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], state)) < 0) return status;
   //Ready
   return status;
}

//===========================================================================
// LDC MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Mode
// Purpose:  This function sets the operation mode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetMode (ViSession instrumentHandle, ViInt16 mode)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":MODE CC", ":MODE CP"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (mode, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[mode], StringLength (command[mode]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Mode
// Purpose:  This function returns the operation mode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetMode (ViSession instrumentHandle, ViInt16 *mode)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":MODE?", 6, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("CC", 0, buffer, 0, 0) == 0) *mode = 0;
   else if (CompareStrings ("CP", 0, buffer, 0, 0) == 0) *mode = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Polarity
// Purpose:  This function sets the polarity of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetLdPol (ViSession instrumentHandle, ViInt16 polarity)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":LDPOL CG", ":LDPOL AG"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (polarity, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[polarity], StringLength (command[polarity]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Laser Polarity
// Purpose:  This function returns the polarity of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetLdPol (ViSession instrumentHandle, ViInt16 *polarity)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":LDPOL?", 7, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("CG", 0, buffer, 0, 0) == 0) *polarity = 0;
   else if (CompareStrings ("AG", 0, buffer, 0, 0) == 0) *polarity = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Photodiode Polarity
// Purpose:  This function sets the photodiode polarity
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetPdPol (ViSession instrumentHandle, ViInt16 polarity)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":PDPOL CG", ":PDPOL AG"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (polarity, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[polarity], StringLength (command[polarity]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Polarity
// Purpose:  This function returns the photodiode polarity
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetPdPol (ViSession instrumentHandle, ViInt16 *polarity)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":PDPOL?", 7, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("CG", 0, buffer, 0, 0) == 0) *polarity = 0;
   else if (CompareStrings ("AG", 0, buffer, 0, 0) == 0) *polarity = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Photodiode Bias
// Purpose:  This function switches the photodiode bias
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetBias (ViSession instrumentHandle, ViInt16 bias)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":PDBIA OFF", ":PDBIA ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (bias, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[bias], StringLength (command[bias]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Bias
// Purpose:  This function returns the state of the photodiode bias
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetBias (ViSession instrumentHandle, ViInt16 *bias)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":PDBIA?", 7, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *bias = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *bias = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Temperature Protection Slot
// Purpose:  This function sets the slot for temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetTpSlot (ViSession instrumentHandle, ViInt16 slot)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (slot, 1, 8)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":TPSLOT %d", slot);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Temperature Protection Slot
// Purpose:  This function returns the slot used for temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetTpSlot (ViSession instrumentHandle, ViInt16 *slot)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TPSLOT?", 8, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], slot)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Temperature Protection
// Purpose:  This function switches the temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetTempProt (ViSession instrumentHandle, ViInt16 tempProtection)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":TP OFF", ":TP ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (tempProtection, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[tempProtection], StringLength (command[tempProtection]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Temperature Protection
// Purpose:  This function returns the state of the temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetTempProt (ViSession instrumentHandle, ViInt16 *temperatureProtection)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TP?", 4, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *temperatureProtection = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *temperatureProtection = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetLdOutput (ViSession instrumentHandle, ViInt16 output)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":LASER OFF", ":LASER ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (output, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[output], StringLength (command[output]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetLdOutput (ViSession instrumentHandle, ViInt16 *output)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":LASER?", 7, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *output = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *output = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Calibration Factor
// Purpose:  This function sets the calibration factor of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetCalibration (ViSession instrumentHandle, ViReal64 calibrationFactor)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":CALPD:SET %f", calibrationFactor);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Calibration Factor
// Purpose:  This function returns the calibration factor of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetCalibration (ViSession instrumentHandle, ViInt16 value, ViReal64 *calibrationfactor)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":CALPD:MIN?", ":CALPD:MAX?", ":CALPD:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], calibrationfactor)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Current
// Purpose:  This function sets the current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetLdCurrent (ViSession instrumentHandle, ViReal64 current)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":ILD:SET %f", current);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Laser Current
// Purpose:  This function returns the current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetLdCurrent (ViSession instrumentHandle, ViInt16 value, ViReal64 *current)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":ILD:MIN?", ":ILD:MAX?", ":ILD:SET?", ":ILD:ACT?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], current)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Photodiode Current
// Purpose:  This function sets the current of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetPdCurrent (ViSession instrumentHandle, ViReal64 current)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":IMD:SET %f", current);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Current
// Purpose:  This function returns the current of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetPdCurrent (ViSession instrumentHandle, ViInt16 value, ViReal64 *current)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":IMD:MIN?", ":IMD:MAX?", ":IMD:SET?", ":IMD:ACT?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], current)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Optical Power
// Purpose:  This function sets the optical power
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetPower (ViSession instrumentHandle, ViReal64 opticalPower)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":POPT:SET %f", opticalPower);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Optical Power
// Purpose:  This function returns the optical power
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetPower (ViSession instrumentHandle, ViInt16 value, ViReal64 *opticalPower)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":POPT:MIN?", ":POPT:MAX?", ":POPT:SET?", ":POPT:ACT?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], opticalPower)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set LD Limit Current
// Purpose:  This function sets the limit current of the laser diode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetLdLimit (ViSession instrumentHandle, ViReal64 limitCurrent)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":LIMC:SET %f", limitCurrent);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get LD Limit Current
// Purpose:  This function returns the limit current of the laser diode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetLdLimit (ViSession instrumentHandle, ViInt16 value, ViReal64 *limitCurrent)
{
   ViStatus  status    = VI_SUCCESS;
   ViUInt32  retCnt    = 0;
   ViUInt16  stb;
   ViString  command[] = {":LIMC:MIN?", ":LIMC:MAX?", ":LIMC:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], limitCurrent)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set PD Limit Current
// Purpose:  This function sets the limit current of the photo diode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetPdLimit (ViSession instrumentHandle, ViReal64 limitCurrent)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":LIMM:SET %f", limitCurrent);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get PD Limit Current
// Purpose:  This function returns the limit current of the photo diode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetPdLimit (ViSession instrumentHandle, ViInt16 value, ViReal64 *limitCurrent)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":LIMM:MIN?", ":LIMM:MAX?", ":LIMM:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], limitCurrent)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Limit Power
// Purpose:  This function sets the limit power
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetPowerLimit (ViSession instrumentHandle, ViReal64 limitPower)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":LIMP:SET %f", limitPower);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Limit Power
// Purpose:  This function returns the limit power
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetPowerLimit (ViSession instrumentHandle, ViInt16 value, ViReal64 *limitPower)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":LIMP:MIN?", ":LIMP:MAX?", ":LIMP:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], limitPower)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Laserdiode Limit Current (adjusted via poti)
// Purpose:  This function returns limit current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetLdLimitPoti (ViSession instrumentHandle, ViReal64 *limitCurrent)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":LIMCP:ACT?", 11, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], limitCurrent)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Laser Voltage
// Purpose:  This function returns the voltage of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetLdVoltage (ViSession instrumentHandle, ViReal64 *voltage)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":VLD:ACT?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], voltage)) < 0) return status;
   //Ready
   return status;
}

//===========================================================================
// MLC MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Mode
// Purpose:  This function sets the operation mode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetMode (ViSession instrumentHandle, ViInt16 mode)
{
   return Pro8_LdcSetMode (instrumentHandle, mode);
}

//---------------------------------------------------------------------------
// Function: Get Mode
// Purpose:  This function returns the operation mode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetMode (ViSession instrumentHandle, ViInt16 *mode)
{
   return Pro8_LdcGetMode (instrumentHandle, mode);
}

//---------------------------------------------------------------------------
// Function: Set Range
// Purpose:  This function sets the current range
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetRange (ViSession instrumentHandle, ViInt16 range)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":RANGE 0", ":RANGE 1"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (range, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[range], StringLength (command[range]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Range
// Purpose:  This function returns the current rage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetRange (ViSession instrumentHandle, ViInt16 *range)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":RANGE?", 7, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("0", 0, buffer, 0, 0) == 0) *range = 0;
   else if (CompareStrings ("1", 0, buffer, 0, 0) == 0) *range = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Temperature Protection Slot
// Purpose:  This function sets the slot for temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetTpSlot (ViSession instrumentHandle, ViInt16 slot)
{
   return Pro8_LdcSetTpSlot (instrumentHandle, slot);
}

//---------------------------------------------------------------------------
// Function: Get Temperature Protection Slot
// Purpose:  This function returns the slot used for temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetTpSlot (ViSession instrumentHandle, ViInt16 *slot)
{
   return Pro8_LdcGetTpSlot (instrumentHandle, slot);
}

//---------------------------------------------------------------------------
// Function: Set Temperature Protection
// Purpose:  This function switches the temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetTempProt (ViSession instrumentHandle, ViInt16 tempProtection)
{
   return Pro8_LdcSetTempProt (instrumentHandle, tempProtection);
}

//---------------------------------------------------------------------------
// Function: Get Temperature Protection
// Purpose:  This function returns the state of the temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetTempProt (ViSession instrumentHandle, ViInt16 *temperatureProtection)
{
   return Pro8_LdcGetTempProt (instrumentHandle, temperatureProtection);
}

//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetLdOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetLdOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Laser Current
// Purpose:  This function sets the current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetLdCurrent (ViSession instrumentHandle, ViReal64 current)
{
   return Pro8_LdcSetLdCurrent (instrumentHandle, current);
}

//---------------------------------------------------------------------------
// Function: Get Laser Current
// Purpose:  This function returns the current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetLdCurrent (ViSession instrumentHandle, ViInt16 value, ViReal64 *current)
{
   return Pro8_LdcGetLdCurrent (instrumentHandle, value, current);
}

//---------------------------------------------------------------------------
// Function: Set Photodiode Current
// Purpose:  This function sets the current of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetPdCurrent (ViSession instrumentHandle, ViReal64 current)
{
   return Pro8_LdcSetPdCurrent (instrumentHandle, current);
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Current
// Purpose:  This function returns the current of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetPdCurrent (ViSession instrumentHandle, ViInt16 value, ViReal64 *current)
{
   return Pro8_LdcGetPdCurrent (instrumentHandle, value, current);
}

//---------------------------------------------------------------------------
// Function: Get Laserdiode Limit Current (adjusted via poti)
// Purpose:  This function returns limit current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetLdLimitPoti (ViSession instrumentHandle, ViReal64 *limitCurrent)
{
   return Pro8_LdcGetLdLimitPoti (instrumentHandle, limitCurrent);
}

//===========================================================================
// TEC MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Sensor
// Purpose:  This function sets the temperature sensor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetSensor (ViSession instrumentHandle, ViInt16 sensor)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":SENS AD", ":SENS THL", ":SENS THH", ":SENS PT100", ":SENS PT1000L", ":SENS PT1000H"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (sensor, 0, 5)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[sensor], StringLength (command[sensor]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Sensor
// Purpose:  This function returns the temperature sensor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetSensor (ViSession instrumentHandle, ViInt16 *sensor)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":SENS?", 6, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("AD",      0, buffer, 0, 0) == 0) *sensor = 0;
   else if (CompareStrings ("THL",     0, buffer, 0, 0) == 0) *sensor = 1;
   else if (CompareStrings ("THH",     0, buffer, 0, 0) == 0) *sensor = 2;
   else if (CompareStrings ("PT100",   0, buffer, 0, 0) == 0) *sensor = 3;
   else if (CompareStrings ("PT1000L", 0, buffer, 0, 0) == 0) *sensor = 4;
   else if (CompareStrings ("PT1000H", 0, buffer, 0, 0) == 0) *sensor = 5;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set I-Share
// Purpose:  This function switches the I-share on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetIShare (ViSession instrumentHandle, ViInt16 IShare)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":INTEG OFF", ":INTEG ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (IShare, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[IShare], StringLength (command[IShare]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get I_Share
// Purpose:  This function returns the state of the I-share
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetIShare (ViSession instrumentHandle, ViInt16 *IShare)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":INTEG?", 7, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *IShare = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *IShare = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Peltier Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetPtOutput (ViSession instrumentHandle, ViInt16 output)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":TEC OFF", ":TEC ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (output, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[output], StringLength (command[output]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Peltier Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetPtOutput (ViSession instrumentHandle, ViInt16 *output)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":TEC?", 5, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *output = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *output = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Thermistor Calibration (exponential method)
// Purpose:  This function sets the calibration values for the thermistor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetThCalExp (ViSession instrumentHandle, ViReal64 BValue, ViReal64 RValue, ViReal64 TValue)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":CALTB:SET %f;:CALTR:SET %f;:CALTT:SET %f", BValue, RValue, TValue);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Thermistor Calibration (exponential method)
// Purpose:  This function returns the calibration values for the thermistor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetThCalExp (ViSession instrumentHandle, ViInt16 values, ViReal64 *BValue, ViReal64 *RValue, ViReal64 *TValue)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command1[] = {":CALTB:MIN?", ":CALTB:MAX?", ":CALTB:SET?"};
   ViString  command2[] = {":CALTR:MIN?", ":CALTR:MAX?", ":CALTR:SET?"};
   ViString  command3[] = {":CALTT:MIN?", ":CALTT:MAX?", ":CALTT:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (values, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command1[values], StringLength (command1[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], BValue)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command2[values], StringLength (command2[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], RValue)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command3[values], StringLength (command3[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], TValue)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Thermistor Calibration (Steinhart-Hart method)
// Purpose:  This function sets the calibration values for the thermistor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetThCalSH (ViSession instrumentHandle, ViReal64 c1Value, ViReal64 c2Value, ViReal64 c3Value)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":CALTC1:SET %f;:CALTC2:SET %f;:CALTC3:SET %f", c1Value, c2Value, c3Value);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Thermistor Calibration (Steinhart-Hart method)
// Purpose:  This function returns the calibration values for the thermistor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetThCalSH (ViSession instrumentHandle, ViInt16 values, ViReal64 *c1Value, ViReal64 *c2Value, ViReal64 *c3Value)
{
   ViStatus  status     = VI_SUCCESS;
   ViUInt32  retCnt     = 0;
   ViUInt16  stb;
   ViString  command1[] = {":CALTC1:MIN?", ":CALTC1:MAX?", ":CALTC1:SET?"};
   ViString  command2[] = {":CALTC2:MIN?", ":CALTC2:MAX?", ":CALTC2:SET?"};
   ViString  command3[] = {":CALTC3:MIN?", ":CALTC3:MAX?", ":CALTC3:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (values, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command1[values], StringLength (command1[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], c1Value)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command2[values], StringLength (command2[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], c2Value)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command3[values], StringLength (command3[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], c3Value)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Temperature
// Purpose:  This function sets the temperature
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetTemp (ViSession instrumentHandle, ViReal64 temperature)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":TEMP:SET %f", temperature);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Temperature
// Purpose:  This function returns the temperature
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetTemp (ViSession instrumentHandle, ViInt16 value, ViReal64 *temperature)
{
   ViStatus  status    = VI_SUCCESS;
   ViUInt32  retCnt    = 0;
   ViUInt16  stb;
   ViString  command[] = {":TEMP:MIN?", ":TEMP:MAX?", ":TEMP:SET?", ":TEMP:ACT?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], temperature)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Resistance
// Purpose:  This function sets the resistance
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetRes (ViSession instrumentHandle, ViReal64 resistance)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":RESI:SET %f", resistance);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Resistance
// Purpose:  This function returns the resistance
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetRes (ViSession instrumentHandle, ViInt16 value, ViReal64 *resistance)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":RESI:MIN?", ":RESI:MAX?", ":RESI:SET?", ":RESI:ACT?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], resistance)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Temperature Window
// Purpose:  This function sets the temperature window
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetTempWin (ViSession instrumentHandle, ViReal64 temperatureWindow)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":TWIN:SET %f", temperatureWindow);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Temperature Window
// Purpose:  This function returns the temperature window
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetTempWin (ViSession instrumentHandle, ViInt16 value, ViReal64 *temperatureWindow)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":TWIN:MIN?", ":TWIN:MAX?", ":TWIN:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], temperatureWindow)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Resistance Window
// Purpose:  This function sets the resistance window
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetResWin (ViSession instrumentHandle, ViReal64 resistanceWindow)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":RWIN:SET %f", resistanceWindow);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Resistance Window
// Purpose:  This function returns the resistance window
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetResWin (ViSession instrumentHandle, ViInt16 value, ViReal64 *resistanceWindow)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":RWIN:MIN?", ":RWIN:MAX?", ":RWIN:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], resistanceWindow)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set PID Shares
// Purpose:  This function sets the PID shares
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetPID (ViSession instrumentHandle, ViReal64 PShare, ViReal64 IShare, ViReal64 DShare)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":SHAREP:SET %f;:SHAREI:SET %f;:SHARED:SET %f", PShare, IShare, DShare);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get PID Shares
// Purpose:  This function returns the PID shares
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetPID (ViSession instrumentHandle, ViInt16 values, ViReal64 *PShare, ViReal64 *IShare, ViReal64 *DShare)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command1[] = {":SHAREP:MIN?", ":SHAREP:MAX?", ":SHAREP:SET?"};
   ViString  command2[] = {":SHAREI:MIN?", ":SHAREI:MAX?", ":SHAREI:SET?"};
   ViString  command3[] = {":SHARED:MIN?", ":SHARED:MAX?", ":SHARED:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (values, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command1[values], StringLength (command1[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], PShare)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command2[values], StringLength (command2[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], IShare)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command3[values], StringLength (command3[values]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], DShare)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Peltier Limit Current
// Purpose:  This function sets the limit current of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetPtLimit (ViSession instrumentHandle, ViReal64 limitCurrent)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":LIMT:SET %f", limitCurrent);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Peltier Limit Current
// Purpose:  This function returns the limit current of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetPtLimit (ViSession instrumentHandle, ViInt16 value, ViReal64 *limitCurrent)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViString command[] = {":LIMT:MIN?", ":LIMT:MAX?", ":LIMT:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], limitCurrent)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Peltier Limit Current (adjusted via poti)
// Purpose:  This function returns the limit current of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetPtLimitPoti (ViSession instrumentHandle, ViReal64 *limitCurrent)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":LIMTP:ACT?", 11, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], limitCurrent)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Peltier Current
// Purpose:  This function returns the current of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetPtCurrent (ViSession instrumentHandle, ViReal64 *current)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":ITE:ACT?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], current)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Peltier Voltage
// Purpose:  This function returns the voltage of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetPtVoltage (ViSession instrumentHandle, ViReal64 *voltage)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":VTE:ACT?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], voltage)) < 0) return status;
   //Ready
   return status;
}

//===========================================================================
// ITC MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Mode
// Purpose:  This function sets the operation mode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetMode (ViSession instrumentHandle, ViInt16 mode)
{
   return Pro8_LdcSetMode (instrumentHandle, mode);
}

//---------------------------------------------------------------------------
// Function: Get Mode
// Purpose:  This function returns the operation mode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetMode (ViSession instrumentHandle, ViInt16 *mode)
{
   return Pro8_LdcGetMode (instrumentHandle, mode);
}

//---------------------------------------------------------------------------
// Function: Set Laser Polarity
// Purpose:  This function sets the polarity of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetLdPol (ViSession instrumentHandle, ViInt16 polarity)
{
   return Pro8_LdcSetLdPol (instrumentHandle, polarity);
}

//---------------------------------------------------------------------------
// Function: Get Laser Polarity
// Purpose:  This function returns the polarity of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetLdPol (ViSession instrumentHandle, ViInt16 *polarity)
{
   return Pro8_LdcGetLdPol (instrumentHandle, polarity);
}

//---------------------------------------------------------------------------
// Function: Set Photodiode Polarity
// Purpose:  This function sets the photodiode polarity
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetPdPol (ViSession instrumentHandle, ViInt16 polarity)
{
   return Pro8_LdcSetPdPol (instrumentHandle, polarity);
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Polarity
// Purpose:  This function returns the photodiode polarity
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetPdPol (ViSession instrumentHandle, ViInt16 *polarity)
{
   return Pro8_LdcGetPdPol (instrumentHandle, polarity);
}

//---------------------------------------------------------------------------
// Function: Set Temperature Protection
// Purpose:  This function switches the temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetTempProt (ViSession instrumentHandle, ViInt16 tempProtection)
{
   return Pro8_LdcSetTempProt (instrumentHandle, tempProtection);
}

//---------------------------------------------------------------------------
// Function: Get Temperature Protection
// Purpose:  This function returns the state of the temperature protection
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetTempProt (ViSession instrumentHandle, ViInt16 *temperatureProtection)
{
   return Pro8_LdcGetTempProt (instrumentHandle, temperatureProtection);
}

//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetLdOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetLdOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Sensor
// Purpose:  This function sets the temperature sensor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetSensor (ViSession instrumentHandle, ViInt16 sensor)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":SENS AD", ":SENS TH"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (sensor, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[sensor], StringLength (command[sensor]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Sensor
// Purpose:  This function returns the temperature sensor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetSensor (ViSession instrumentHandle, ViInt16 *sensor)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":SENS?", 6, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("AD", 0, buffer, 0, 0) == 0) *sensor = 0;
   else if (CompareStrings ("TH", 0, buffer, 0, 0) == 0) *sensor = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set I-Share
// Purpose:  This function switches the I-share on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetIShare (ViSession instrumentHandle, ViInt16 IShare)
{
   return Pro8_TecSetIShare (instrumentHandle, IShare);
}

//---------------------------------------------------------------------------
// Function: Get I_Share
// Purpose:  This function returns the state of the I-share
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetIShare (ViSession instrumentHandle, ViInt16 *IShare)
{
   return Pro8_TecGetIShare (instrumentHandle, IShare);
}

//---------------------------------------------------------------------------
// Function: Set Peltier Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetPtOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_TecSetPtOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Peltier Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetPtOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_TecGetPtOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Calibration Factor
// Purpose:  This function sets the calibration factor of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetCalibration (ViSession instrumentHandle, ViReal64 calibrationFactor)
{
   return Pro8_LdcSetCalibration (instrumentHandle, calibrationFactor);
}

//---------------------------------------------------------------------------
// Function: Get Calibration Factor
// Purpose:  This function returns the calibration factor of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetCalibration (ViSession instrumentHandle, ViInt16 value, ViReal64 *calibrationfactor)
{
   return Pro8_LdcGetCalibration (instrumentHandle, value, calibrationfactor);
}

//---------------------------------------------------------------------------
// Function: Set Laser Current
// Purpose:  This function sets the current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetLdCurrent (ViSession instrumentHandle, ViReal64 current)
{
   return Pro8_LdcSetLdCurrent (instrumentHandle, current);
}

//---------------------------------------------------------------------------
// Function: Get Laser Current
// Purpose:  This function returns the current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetLdCurrent (ViSession instrumentHandle, ViInt16 value, ViReal64 *current)
{
   return Pro8_LdcGetLdCurrent (instrumentHandle, value, current);
}

//---------------------------------------------------------------------------
// Function: Set Photodiode Current
// Purpose:  This function sets the current of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetPdCurrent (ViSession instrumentHandle, ViReal64 current)
{
   return Pro8_LdcSetPdCurrent (instrumentHandle, current);
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Current
// Purpose:  This function returns the current of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetPdCurrent (ViSession instrumentHandle, ViInt16 value, ViReal64 *current)
{
   return Pro8_LdcGetPdCurrent (instrumentHandle, value, current);
}

//---------------------------------------------------------------------------
// Function: Set Optical Power
// Purpose:  This function sets the optical power
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetPower (ViSession instrumentHandle, ViReal64 opticalPower)
{
   return Pro8_LdcSetPower (instrumentHandle, opticalPower);
}

//---------------------------------------------------------------------------
// Function: Get Optical Power
// Purpose:  This function returns the optical power
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetPower (ViSession instrumentHandle, ViInt16 value, ViReal64 *opticalPower)
{
   return Pro8_LdcGetPower (instrumentHandle, value, opticalPower);
}

//---------------------------------------------------------------------------
// Function: Set Bias Voltage
// Purpose:  This function sets the photo diode bias voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetBiasVoltage (ViSession instrumentHandle, ViReal64 biasVoltage)
{
   return Pro8_PdaSetBiasVoltage (instrumentHandle, biasVoltage);
}

//---------------------------------------------------------------------------
// Function: Get Bias Voltage
// Purpose:  This function returns the bias voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetBiasVoltage (ViSession instrumentHandle, ViInt16 value, ViReal64 *biasVoltage)
{
   return Pro8_PdaGetBiasVoltage (instrumentHandle, value, biasVoltage);
}

//---------------------------------------------------------------------------
// Function: Set LD Limit Current
// Purpose:  This function sets the limit current of the laser diode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetLdLimit (ViSession instrumentHandle, ViReal64 limitCurrent)
{
   return Pro8_LdcSetLdLimit (instrumentHandle, limitCurrent);
}

//---------------------------------------------------------------------------
// Function: Get LD Limit Current
// Purpose:  This function returns the limit current of the laser diode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetLdLimit (ViSession instrumentHandle, ViInt16 value, ViReal64 *limitCurrent)
{
   return Pro8_LdcGetLdLimit (instrumentHandle, value, limitCurrent);
}

//---------------------------------------------------------------------------
// Function: Get Laserdiode Limit Current (adjusted via poti)
// Purpose:  This function returns limit current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetLdLimitPoti (ViSession instrumentHandle, ViReal64 *limitCurrent)
{
   return Pro8_LdcGetLdLimitPoti (instrumentHandle, limitCurrent);
}

//---------------------------------------------------------------------------
// Function: Get Laser Voltage
// Purpose:  This function returns the voltage of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetLdVoltage (ViSession instrumentHandle, ViReal64 *voltage)
{
   return Pro8_LdcGetLdVoltage (instrumentHandle, voltage);
}

//---------------------------------------------------------------------------
// Function: Set Thermistor Calibration (exponential method)
// Purpose:  This function sets the calibration values for the thermistor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetThCalExp (ViSession instrumentHandle, ViReal64 BValue, ViReal64 RValue, ViReal64 TValue)
{
   return Pro8_TecSetThCalExp (instrumentHandle, BValue, RValue, TValue);
}

//---------------------------------------------------------------------------
// Function: Get Thermistor Calibration (exponential method)
// Purpose:  This function returns the calibration values for the thermistor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetThCalExp (ViSession instrumentHandle, ViInt16 values, ViReal64 *BValue, ViReal64 *RValue, ViReal64 *TValue)
{
   return Pro8_TecGetThCalExp (instrumentHandle, values, BValue, RValue, TValue);
}

//---------------------------------------------------------------------------
// Function: Set Thermistor Calibration (Steinhart-Hart method)
// Purpose:  This function sets the calibration values for the thermistor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetThCalSH (ViSession instrumentHandle, ViReal64 c1Value, ViReal64 c2Value, ViReal64 c3Value)
{
   return Pro8_TecSetThCalSH (instrumentHandle, c1Value, c2Value, c3Value);
}

//---------------------------------------------------------------------------
// Function: Get Thermistor Calibration (Steinhart-Hart method)
// Purpose:  This function returns the calibration values for the thermistor
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetThCalSH (ViSession instrumentHandle, ViInt16 values, ViReal64 *c1Value, ViReal64 *c2Value, ViReal64 *c3Value)
{
   return Pro8_TecGetThCalSH (instrumentHandle, values, c1Value, c2Value, c3Value);
}

//---------------------------------------------------------------------------
// Function: Set Temperature
// Purpose:  This function sets the temperature
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetTemp (ViSession instrumentHandle, ViReal64 temperature)
{
   return Pro8_TecSetTemp (instrumentHandle, temperature);
}

//---------------------------------------------------------------------------
// Function: Get Temperature
// Purpose:  This function returns the temperature
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetTemp (ViSession instrumentHandle, ViInt16 value, ViReal64 *temperature)
{
   return Pro8_TecGetTemp (instrumentHandle, value, temperature);
}

//---------------------------------------------------------------------------
// Function: Set Resistance
// Purpose:  This function sets the resistance
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetRes (ViSession instrumentHandle, ViReal64 resistance)
{
   return Pro8_TecSetRes (instrumentHandle, resistance);
}

//---------------------------------------------------------------------------
// Function: Get Resistance
// Purpose:  This function returns the resistance
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetRes (ViSession instrumentHandle, ViInt16 value, ViReal64 *resistance)
{
   return Pro8_TecGetRes (instrumentHandle, value, resistance);
}

//---------------------------------------------------------------------------
// Function: Set Temperature Window
// Purpose:  This function sets the temperature window
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetTempWin (ViSession instrumentHandle, ViReal64 temperatureWindow)
{
   return Pro8_TecSetTempWin (instrumentHandle, temperatureWindow);
}

//---------------------------------------------------------------------------
// Function: Get Temperature Window
// Purpose:  This function returns the temperature window
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetTempWin (ViSession instrumentHandle, ViInt16 value, ViReal64 *temperatureWindow)
{
   return Pro8_TecGetTempWin (instrumentHandle, value, temperatureWindow);
}

//---------------------------------------------------------------------------
// Function: Set Resistance Window
// Purpose:  This function sets the resistance window
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetResWin (ViSession instrumentHandle, ViReal64 resistanceWindow)
{
   return Pro8_TecSetResWin (instrumentHandle, resistanceWindow);
}

//---------------------------------------------------------------------------
// Function: Get Resistance Window
// Purpose:  This function returns the resistance window
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetResWin (ViSession instrumentHandle, ViInt16 value, ViReal64 *resistanceWindow)
{
   return Pro8_TecGetResWin (instrumentHandle, value, resistanceWindow);
}

//---------------------------------------------------------------------------
// Function: Set PID Shares
// Purpose:  This function sets the PID shares
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetPID (ViSession instrumentHandle, ViReal64 PShare, ViReal64 IShare, ViReal64 DShare)
{
   return Pro8_TecSetPID (instrumentHandle, PShare, IShare, DShare);
}

//---------------------------------------------------------------------------
// Function: Get PID Shares
// Purpose:  This function returns the PID shares
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetPID (ViSession instrumentHandle, ViInt16 values, ViReal64 *PShare, ViReal64 *IShare, ViReal64 *DShare)
{
   return Pro8_TecGetPID (instrumentHandle, values, PShare, IShare, DShare);
}

//---------------------------------------------------------------------------
// Function: Set Peltier Limit Current
// Purpose:  This function sets the limit current of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetPtLimit (ViSession instrumentHandle, ViReal64 limitCurrent)
{
   return Pro8_TecSetPtLimit (instrumentHandle, limitCurrent);
}

//---------------------------------------------------------------------------
// Function: Get Peltier Limit Current
// Purpose:  This function returns the limit current of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetPtLimit (ViSession instrumentHandle, ViInt16 value, ViReal64 *limitCurrent)
{
   return Pro8_TecGetPtLimit (instrumentHandle, value, limitCurrent);
}

//---------------------------------------------------------------------------
// Function: Get Peltier Current
// Purpose:  This function returns the current of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetPtCurrent (ViSession instrumentHandle, ViReal64 *current)
{
   return Pro8_TecGetPtCurrent (instrumentHandle, current);
}

//---------------------------------------------------------------------------
// Function: Get Peltier Voltage
// Purpose:  This function returns the voltage of the peltier
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetPtVoltage (ViSession instrumentHandle, ViReal64 *voltage)
{
   return Pro8_TecGetPtVoltage (instrumentHandle, voltage);
}

//===========================================================================
// PDA MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Photodiode Polarity
// Purpose:  This function sets the photodiode polarity
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaSetPdPol (ViSession instrumentHandle, ViInt16 polarity)
{
   return Pro8_LdcSetPdPol (instrumentHandle, polarity);
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Polarity
// Purpose:  This function returns the photodiode polarity
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetPdPol (ViSession instrumentHandle, ViInt16 *polarity)
{
   return Pro8_LdcGetPdPol (instrumentHandle, polarity);
}

//---------------------------------------------------------------------------
// Function: Set Photodiode Bias
// Purpose:  This function switches the photodiode bias
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaSetBias (ViSession instrumentHandle, ViInt16 bias)
{
   return Pro8_LdcSetBias (instrumentHandle, bias);
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Bias
// Purpose:  This function returns the state of the photodiode bias
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetBias (ViSession instrumentHandle, ViInt16 *bias)
{
   return Pro8_LdcGetBias (instrumentHandle, bias);
}

//---------------------------------------------------------------------------
// Function: Set Range
// Purpose:  This function sets the current range
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaSetRange (ViSession instrumentHandle, ViInt16 range)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (range, 1, 7)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":RANGE %d", range);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Range
// Purpose:  This function returns the current rage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetRange (ViSession instrumentHandle, ViInt16 *range)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":RANGE?", 7, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], range)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Calibration Factor
// Purpose:  This function sets the calibration factor of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaSetCalibration (ViSession instrumentHandle, ViReal64 calibrationFactor)
{
   return Pro8_LdcSetCalibration (instrumentHandle, calibrationFactor);
}

//---------------------------------------------------------------------------
// Function: Get Calibration Factor
// Purpose:  This function returns the calibration factor of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetCalibration (ViSession instrumentHandle, ViInt16 value, ViReal64 *calibrationfactor)
{
   return Pro8_LdcGetCalibration (instrumentHandle, value, calibrationfactor);
}

//---------------------------------------------------------------------------
// Function: Get Photodiode Current
// Purpose:  This function returns the current of the photodiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetPdCurrent (ViSession instrumentHandle, ViReal64 *current)
{
   ViStatus  status    = VI_SUCCESS;
   ViUInt32  retCnt    = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":IPD:ACT?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], current)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Optical Power
// Purpose:  This function returns the optical power
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetPower (ViSession instrumentHandle, ViReal64 *opticalPower)
{
   ViStatus  status    = VI_SUCCESS;
   ViUInt32  retCnt    = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":POPT:ACT?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], opticalPower)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Bias Voltage
// Purpose:  This function sets the photo diode bias voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaSetBiasVoltage (ViSession instrumentHandle, ViReal64 biasVoltage)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":VBIAS:SET %f", biasVoltage);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Bias Voltage
// Purpose:  This function returns the bias voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetBiasVoltage (ViSession instrumentHandle, ViInt16 value, ViReal64 *biasVoltage)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":VBIAS:MIN?", ":VBIAS:MAX?", ":VBIAS:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], biasVoltage)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Forward Current
// Purpose:  This function sets the photo diode forward current
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaSetFwdCurr (ViSession instrumentHandle, ViReal64 forwardCurrent)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":IFWD:SET %f", forwardCurrent);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Forward Current
// Purpose:  This function returns the photo diode forward current
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetFwdCurr (ViSession instrumentHandle, ViInt16 value, ViReal64 *forwardCurrent)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":IFWD:MIN?", ":IFWD:MAX?", ":IFWD:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], forwardCurrent)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Forward Voltage
// Purpose:  This function returns the photo diode forward voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetFwdVoltage (ViSession instrumentHandle, ViReal64 *forwardVoltage)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":VFWD:ACT?", 10, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], forwardVoltage)) < 0) return status;
   //Ready
   return status;
}

//===========================================================================
// LS MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsSetPowerdBm (ViSession instrumentHandle, ViReal64 power)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":P_DBM:SET %f", power);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsGetPowerdBm (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":P_DBM:MIN?", ":P_DBM:MAX?", ":P_DBM:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], power)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsSetPowerW (ViSession instrumentHandle, ViReal64 power)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":P_W:SET %f", power);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsGetPowerW (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":P_W:MIN?", ":P_W:MAX?", ":P_W:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], power)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Temperature Difference
// Purpose:  This function sets the temperature difference
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsSetTempDiff (ViSession instrumentHandle, ViReal64 temperatureDifference)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":DTEMP:SET %f", temperatureDifference);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Temperature Difference
// Purpose:  This function returns the temperature difference
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsGetTempDifference (ViSession instrumentHandle, ViInt16 value, ViReal64 *temperatureDifference)
{
   ViStatus  status    = VI_SUCCESS;
   ViUInt32  retCnt    = 0;
   ViUInt16  stb;
   ViString  command[] = {":DTEMP:MIN?", ":DTEMP:MAX?", ":DTEMP:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], temperatureDifference)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":SYNCMOD OFF", ":SYNCMOD ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (synchronousModulation, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[synchronousModulation], StringLength (command[synchronousModulation]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":SYNCMOD?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *synchronousModulation = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *synchronousModulation = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}



//===========================================================================
// SLED MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set LED Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":LED OFF", ":LED ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (output, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[output], StringLength (command[output]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":LED?", 5, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *output = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *output = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledSetPowerdBm (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerdBm (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledGetPowerdBm (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   return Pro8_LsGetPowerdBm (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledSetPowerW (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerW (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledGetPowerW (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   return Pro8_LsGetPowerW (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   return Pro8_LsSetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   return Pro8_LsGetSyncMod (instrumentHandle, synchronousModulation);
}


//===========================================================================
// WDM-CW MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetPowerdBm (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerdBm (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetPowerdBm (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   return Pro8_LsGetPowerdBm (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetPowerW (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerW (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetPowerW (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   return Pro8_LsGetPowerW (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Set Coherence Control Ration
// Purpose:  This function sets the coherence control ratio
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetCoherence (ViSession instrumentHandle, ViReal64 coherence)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":COHERENCE:SET %f", coherence);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Coherence Control Ration
// Purpose:  This function returns the coherence control ratio
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetCoherence (ViSession instrumentHandle, ViInt16 value, ViReal64 *coherence)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":COHERENCE:MIN?", ":COHERENCE:MAX?", ":COHERENCE:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], coherence)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Wavelength
// Purpose:  This function sets the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetWavelength (ViSession instrumentHandle, ViReal64 wavelength)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":LAMBDA:SET %f", wavelength);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Wavelength
// Purpose:  This function returns the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetWavelength (ViSession instrumentHandle, ViInt16 value, ViReal64 *wavelength)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":LAMBDA:MIN?", ":LAMBDA:MAX?", ":LAMBDA:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], wavelength)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Frequency
// Purpose:  This function sets the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetFrequency (ViSession instrumentHandle, ViReal64 frequency)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":LASERFREQ:SET %f", frequency);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Frequency
// Purpose:  This function returns the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *frequency)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":LASERFREQ:MIN?", ":LASERFREQ:MAX?", ":LASERFREQ:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], frequency)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   return Pro8_LsSetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   return Pro8_LsGetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation
// Purpose:  This function switches the LF modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetLfMod (ViSession instrumentHandle, ViInt16 LFModulation)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":LFMOD:ENABLE OFF", ":LFMOD:ENABLE ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (LFModulation, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[LFModulation], StringLength (command[LFModulation]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation
// Purpose:  This function retuns the state of the LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetLfMod (ViSession instrumentHandle, ViInt16 *LFModulation)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":LFMOD:ENABLE?", 14, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *LFModulation = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *LFModulation = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Type
// Purpose:  This function switches the LF modulation type
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetLfModType (ViSession instrumentHandle, ViInt16 LFModulationType)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":LFMOD:TYPE NOISE", ":LFMOD:TYPE PULSE", ":LFMOD:TYPE SINE", ":LFMOD:TYPE SQUARE", ":LFMOD:TYPE TRIANGLE"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (LFModulationType, 0, 4)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[LFModulationType], StringLength (command[LFModulationType]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Type
// Purpose:  This function retuns the type of LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetLfModType (ViSession instrumentHandle, ViInt16 *LFModulationType)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":LFMOD:TYPE?", 12, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("NOISE",    0, buffer, 0, 0) == 0) *LFModulationType = 0;
   else if (CompareStrings ("PULSE",    0, buffer, 0, 0) == 0) *LFModulationType = 1;
   else if (CompareStrings ("SINE" ,    0, buffer, 0, 0) == 0) *LFModulationType = 2;
   else if (CompareStrings ("SQUARE",   0, buffer, 0, 0) == 0) *LFModulationType = 3;
   else if (CompareStrings ("TRIANGLE", 0, buffer, 0, 0) == 0) *LFModulationType = 4;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Amplitude
// Purpose:  This function sets the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetLfAmplitude (ViSession instrumentHandle, ViReal64 LFAmplitude)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":LFAMP:SET %f", LFAmplitude);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Amplitude
// Purpose:  This function returns the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetLfAmplitude (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFAmplitude)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":LFAMP:MIN?", ":LFAMP:MAX?", ":LFAMP:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], LFAmplitude)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Frequency
// Purpose:  This function sets the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwSetLfFrequency (ViSession instrumentHandle, ViReal64 LFFrequency)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":LFFREQ:SET %f", LFFrequency);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Frequency
// Purpose:  This function returns the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetLfFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFFrequency)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":LFFREQ:MIN?", ":LFFREQ:MAX?", ":LFFREQ:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], LFFrequency)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get max HF Modulation Voltage
// Purpose:  This function returns the maximum HF modulation voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwGetMaxHfVoltage (ViSession instrumentHandle, ViReal64 *maxHF_modVoltage)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":VHFMAX:ACT?", 12, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], maxHF_modVoltage)) < 0) return status;
   //Ready
   return status;
}

//===========================================================================
// WDM-EA MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Coherence Control
// Purpose:  This function switches the coherence control on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetCohCtrl (ViSession instrumentHandle, ViInt16 coherence)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":COHCNTL OFF", ":COHCNTL ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (coherence, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[coherence], StringLength (command[coherence]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Coherence Control
// Purpose:  This function returns the state of the coherence control
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetCohCtrl (ViSession instrumentHandle, ViInt16 *coherence)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":COHCNTL?", 9, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *coherence = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *coherence = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetPowerdBm (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerdBm (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetPowerdBm (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   return Pro8_LsGetPowerdBm (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetPowerW (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerW (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetPowerW (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   return Pro8_LsGetPowerW (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Set Wavelength
// Purpose:  This function sets the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetWavelength (ViSession instrumentHandle, ViReal64 wavelength)
{
   return Pro8_WdmCwSetWavelength (instrumentHandle, wavelength);
}

//---------------------------------------------------------------------------
// Function: Get Wavelength
// Purpose:  This function returns the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetWavelength (ViSession instrumentHandle, ViInt16 value, ViReal64 *wavelength)
{
   return Pro8_WdmCwGetWavelength (instrumentHandle, value, wavelength);
}

//---------------------------------------------------------------------------
// Function: Set Threshold Voltage
// Purpose:  This function sets the threshold voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetThreshold (ViSession instrumentHandle, ViReal64 thresholdVoltage)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":VTH:SET %f", thresholdVoltage);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Threshold Voltage
// Purpose:  This function returns the threshold voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetThreshold (ViSession instrumentHandle, ViInt16 value, ViReal64 *thresholdVoltage)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":VTH:MIN?", ":VTH:MAX?", ":VTH:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], thresholdVoltage)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Frequency
// Purpose:  This function sets the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetFrequency (ViSession instrumentHandle, ViReal64 frequency)
{
   return Pro8_WdmCwSetFrequency (instrumentHandle, frequency);
}

//---------------------------------------------------------------------------
// Function: Get Frequency
// Purpose:  This function returns the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *frequency)
{
   return Pro8_WdmCwGetFrequency (instrumentHandle, value, frequency);
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   return Pro8_LsSetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   return Pro8_LsGetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation
// Purpose:  This function switches the LF modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetLfMod (ViSession instrumentHandle, ViInt16 LFModulation)
{
   return Pro8_WdmCwSetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation
// Purpose:  This function retuns the state of the LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetLfMod (ViSession instrumentHandle, ViInt16 *LFModulation)
{
   return Pro8_WdmCwGetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Type
// Purpose:  This function switches the LF modulation type
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetLfModType (ViSession instrumentHandle, ViInt16 LFModulationType)
{
   return Pro8_WdmCwSetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Type
// Purpose:  This function retuns the type of LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetLfModType (ViSession instrumentHandle, ViInt16 *LFModulationType)
{
   return Pro8_WdmCwGetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Amplitude
// Purpose:  This function sets the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetLfAmplitude (ViSession instrumentHandle, ViReal64 LFAmplitude)
{
   return Pro8_WdmCwSetLfAmplitude (instrumentHandle, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Amplitude
// Purpose:  This function returns the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetLfAmplitude (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFAmplitude)
{
   return Pro8_WdmCwGetLfAmplitude (instrumentHandle, value, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Frequency
// Purpose:  This function sets the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetLfFrequency (ViSession instrumentHandle, ViReal64 LFFrequency)
{
   return Pro8_WdmCwSetLfFrequency (instrumentHandle, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Frequency
// Purpose:  This function returns the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetLfFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFFrequency)
{
   return Pro8_WdmCwGetLfFrequency (instrumentHandle, value, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Set HF Modulation Amplitude
// Purpose:  This function sets the HF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetHfAmplitude (ViSession instrumentHandle, ViReal64 HFAmplitude)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":HFAMP:SET %f", HFAmplitude);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get HF Modulation Amplitude
// Purpose:  This function returns the HF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetHfAmplitude (ViSession instrumentHandle, ViInt16 value, ViReal64 *HFAmplitude)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":HFAMP:MIN?", ":HFAMP:MAX?", ":HFAMP:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], HFAmplitude)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set HF Modulation
// Purpose:  This function switches the HF modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetHfMod (ViSession instrumentHandle, ViInt16 HFModulation)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":HFMOD OFF", ":HFMOD ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (HFModulation, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[HFModulation], StringLength (command[HFModulation]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get HF Modulation
// Purpose:  This function retuns the state of the HF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetHfMod (ViSession instrumentHandle, ViInt16 *HFModulation)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":HFMOD?", 7, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *HFModulation = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *HFModulation = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Modulation Voltage
// Purpose:  This function sets the modulation voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaSetModVoltage (ViSession instrumentHandle, ViReal64 modulationVoltage)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":VMOD:SET %f", modulationVoltage);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Modulation Voltage
// Purpose:  This function returns the modulation voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmEaGetModVoltage (ViSession instrumentHandle, ViInt16 value, ViReal64 *modulationVoltage)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViString command[] = {":VMOD:MIN?", ":VMOD:MAX?", ":VMOD:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], modulationVoltage)) < 0) return status;
   //Ready
   return status;
}


//===========================================================================
// WDM-DIR MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Coherence Control
// Purpose:  This function switches the coherence control on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetCohCtrl (ViSession instrumentHandle, ViInt16 coherence)
{
   return Pro8_WdmEaSetCohCtrl (instrumentHandle, coherence);
}

//---------------------------------------------------------------------------
// Function: Get Coherence Control
// Purpose:  This function returns the state of the coherence control
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetCohCtrl (ViSession instrumentHandle, ViInt16 *coherence)
{
   return Pro8_WdmEaGetCohCtrl (instrumentHandle, coherence);
}

//---------------------------------------------------------------------------
// Function: Set Modulation
// Purpose:  This function switches the modulation on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetModulation (ViSession instrumentHandle, ViInt16 modulation)
{
   ViStatus status    = VI_SUCCESS;
   ViUInt32 retCnt    = 0;
   ViUInt16 stb;
   ViString command[] = {":MOD OFF", ":MOD ON"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (modulation, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, command[modulation], StringLength (command[modulation]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Modulation
// Purpose:  This function returns the state of the modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetModulation (ViSession instrumentHandle, ViInt16 *modulation)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViBoolean answer;
   ViString  format[] = {"%*s %s", "%s"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":MOD?", 5, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], buffer)) < 0) return status;
   //Evaluate
   if      (CompareStrings ("OFF", 0, buffer, 0, 0) == 0) *modulation = 0;
   else if (CompareStrings ("ON",  0, buffer, 0, 0) == 0) *modulation = 1;
   else status = VI_ERROR_INSTR_INTERPRETING_RESPONSE;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetPowerdBm (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerdBm (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetPowerdBm (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   return Pro8_LsGetPowerdBm (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Get Average Laser Power
// Purpose:  This function returns the average laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetAvPowerdBm (ViSession instrumentHandle, ViReal64 *power)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":PAV_DBM:ACT?", 13, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], power)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetPowerW (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerW (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetPowerW (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   return Pro8_LsGetPowerW (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Get Average Laser Power
// Purpose:  This function returns the average laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetAvPowerW (ViSession instrumentHandle, ViReal64 *power)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":PAV_W:ACT?", 11, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], power)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Wavelength
// Purpose:  This function sets the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetWavelength (ViSession instrumentHandle, ViReal64 wavelength)
{
   return Pro8_WdmCwSetWavelength (instrumentHandle, wavelength);
}

//---------------------------------------------------------------------------
// Function: Get Wavelength
// Purpose:  This function returns the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetWavelength (ViSession instrumentHandle, ViInt16 value, ViReal64 *wavelength)
{
   return Pro8_WdmCwGetWavelength (instrumentHandle, value, wavelength);
}

//---------------------------------------------------------------------------
// Function: Set Threshold Voltage
// Purpose:  This function sets the threshold voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetThreshold (ViSession instrumentHandle, ViReal64 thresholdVoltage)
{
   return Pro8_WdmEaSetThreshold (instrumentHandle, thresholdVoltage);
}

//---------------------------------------------------------------------------
// Function: Get Threshold Voltage
// Purpose:  This function returns the threshold voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetThreshold (ViSession instrumentHandle, ViInt16 value, ViReal64 *thresholdVoltage)
{
   return Pro8_WdmEaGetThreshold (instrumentHandle, value, thresholdVoltage);
}

//---------------------------------------------------------------------------
// Function: Set Modulation Current
// Purpose:  This function sets the modulation current
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetModCurrent (ViSession instrumentHandle, ViReal64 modulationCurrent)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Formatting
   Fmt (buffer, ":CMOD:SET %f", modulationCurrent);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get Modulation Current
// Purpose:  This function returns the modulation current
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetModCurrent (ViSession instrumentHandle, ViInt16 value, ViReal64 *modulationCurrent)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViString command[] = {":CMOD:MIN?", ":CMOD:MAX?", ":CMOD:SET?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], modulationCurrent)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Frequency
// Purpose:  This function sets the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetFrequency (ViSession instrumentHandle, ViReal64 frequency)
{
   return Pro8_WdmCwSetFrequency (instrumentHandle, frequency);
}

//---------------------------------------------------------------------------
// Function: Get Frequency
// Purpose:  This function returns the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *frequency)
{
   return Pro8_WdmCwGetFrequency (instrumentHandle, value, frequency);
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   return Pro8_LsSetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   return Pro8_LsGetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation
// Purpose:  This function switches the LF modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetLfMod (ViSession instrumentHandle, ViInt16 LFModulation)
{
   return Pro8_WdmCwSetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation
// Purpose:  This function retuns the state of the LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetLfMod (ViSession instrumentHandle, ViInt16 *LFModulation)
{
   return Pro8_WdmCwGetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Type
// Purpose:  This function switches the LF modulation type
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetLfModType (ViSession instrumentHandle, ViInt16 LFModulationType)
{
   return Pro8_WdmCwSetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Type
// Purpose:  This function retuns the type of LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetLfModType (ViSession instrumentHandle, ViInt16 *LFModulationType)
{
   return Pro8_WdmCwGetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Amplitude
// Purpose:  This function sets the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetLfAmplitude (ViSession instrumentHandle, ViReal64 LFAmplitude)
{
   return Pro8_WdmCwSetLfAmplitude (instrumentHandle, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Amplitude
// Purpose:  This function returns the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetLfAmplitude (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFAmplitude)
{
   return Pro8_WdmCwGetLfAmplitude (instrumentHandle, value, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Frequency
// Purpose:  This function sets the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetLfFrequency (ViSession instrumentHandle, ViReal64 LFFrequency)
{
   return Pro8_WdmCwSetLfFrequency (instrumentHandle, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Frequency
// Purpose:  This function returns the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetLfFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFFrequency)
{
   return Pro8_WdmEaGetLfFrequency (instrumentHandle, value, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Set HF Modulation Amplitude
// Purpose:  This function sets the HF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetHfAmplitude (ViSession instrumentHandle, ViReal64 HFAmplitude)
{
   return Pro8_WdmEaSetHfAmplitude (instrumentHandle, HFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Get HF Modulation Amplitude
// Purpose:  This function returns the HF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetHfAmplitude (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFAmplitude)
{
   return Pro8_WdmEaGetHfAmplitude (instrumentHandle, value, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Set HF Modulation
// Purpose:  This function switches the HF modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirSetHfMod (ViSession instrumentHandle, ViInt16 HFModulation)
{
   return Pro8_WdmEaSetHfMod (instrumentHandle, HFModulation);
}

//---------------------------------------------------------------------------
// Function: Get HF Modulation
// Purpose:  This function retuns the state of the HF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmDirGetHfMod (ViSession instrumentHandle, ViInt16 *HFModulation)
{
   return Pro8_WdmEaGetHfMod (instrumentHandle, HFModulation);
}


//===========================================================================
// WDM-CCDM MODULE FUNCTIONS
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetPowerdBm (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerdBm (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetPowerdBm (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   ViStatus  status    = VI_SUCCESS;
   ViUInt32  retCnt    = 0;
   ViUInt16  stb;
   ViString  command[] = {":P_DBM:MIN?", ":P_DBM:MAX?", ":P_DBM:SET?", ":P_DBM:ACT?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], power)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetPowerW (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerW (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetPowerW (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":P_W:MIN?", ":P_W:MAX?", ":P_W:SET?", ":P_W:ACT?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (value, 0, 3)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[value], StringLength (command[value]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], power)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Set Wavelength
// Purpose:  This function sets the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetWavelength (ViSession instrumentHandle, ViReal64 wavelength)
{
   return Pro8_WdmCwSetWavelength (instrumentHandle, wavelength);
}

//---------------------------------------------------------------------------
// Function: Get Wavelength
// Purpose:  This function returns the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetWavelength (ViSession instrumentHandle, ViInt16 value, ViReal64 *wavelength)
{
   return Pro8_WdmCwGetWavelength (instrumentHandle, value, wavelength);
}

//---------------------------------------------------------------------------
// Function: Set Frequency
// Purpose:  This function sets the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetFrequency (ViSession instrumentHandle, ViReal64 frequency)
{
   return Pro8_WdmCwSetFrequency (instrumentHandle, frequency);
}

//---------------------------------------------------------------------------
// Function: Get Frequency
// Purpose:  This function returns the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *frequency)
{
   return Pro8_WdmCwGetFrequency (instrumentHandle, value, frequency);
}

//---------------------------------------------------------------------------
// Function: Set Coherence Control Ration
// Purpose:  This function sets the coherence control ratio
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetCoherence (ViSession instrumentHandle, ViReal64 coherence)
{
   return Pro8_WdmCwSetCoherence (instrumentHandle, coherence);
}

//---------------------------------------------------------------------------
// Function: Get Coherence Control Ration
// Purpose:  This function returns the coherence control ratio
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetCoherence (ViSession instrumentHandle, ViInt16 value, ViReal64 *coherence)
{
   return Pro8_WdmCwGetCoherence (instrumentHandle, value, coherence);
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   return Pro8_LsSetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   return Pro8_LsGetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation
// Purpose:  This function switches the LF modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetLfMod (ViSession instrumentHandle, ViInt16 LFModulation)
{
   return Pro8_WdmCwSetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation
// Purpose:  This function retuns the state of the LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetLfMod (ViSession instrumentHandle, ViInt16 *LFModulation)
{
   return Pro8_WdmCwGetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Type
// Purpose:  This function switches the LF modulation type
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetLfModType (ViSession instrumentHandle, ViInt16 LFModulationType)
{
   return Pro8_WdmCwSetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Type
// Purpose:  This function retuns the type of LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetLfModType (ViSession instrumentHandle, ViInt16 *LFModulationType)
{
   return Pro8_WdmCwGetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Amplitude
// Purpose:  This function sets the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetLfAmplitude (ViSession instrumentHandle, ViReal64 LFAmplitude)
{
   return Pro8_WdmCwSetLfAmplitude (instrumentHandle, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Amplitude
// Purpose:  This function returns the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetLfAmplitude (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFAmplitude)
{
   return Pro8_WdmCwGetLfAmplitude (instrumentHandle, value, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Frequency
// Purpose:  This function sets the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmSetLfFrequency (ViSession instrumentHandle, ViReal64 LFFrequency)
{
   return Pro8_WdmCwSetLfFrequency (instrumentHandle, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Frequency
// Purpose:  This function returns the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetLfFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFFrequency)
{
   return Pro8_WdmEaGetLfFrequency (instrumentHandle, value, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Get max HF Modulation Voltage
// Purpose:  This function returns the maximum HF modulation voltage
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCcdmGetMaxHfVoltage (ViSession instrumentHandle, ViReal64 *maxHF_modVoltage)
{
   return Pro8_WdmCwGetMaxHfVoltage (instrumentHandle, maxHF_modVoltage);
}


//===========================================================================
// WDM-CWDM Functions
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   return Pro8_LsSetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   return Pro8_LsGetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation
// Purpose:  This function switches the LF modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetLfMod (ViSession instrumentHandle, ViInt16 LFModulation)
{
   return Pro8_WdmCwSetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation
// Purpose:  This function retuns the state of the LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetLfMod (ViSession instrumentHandle, ViInt16 *LFModulation)
{
   return Pro8_WdmCwGetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Type
// Purpose:  This function switches the LF modulation type
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetLfModType (ViSession instrumentHandle, ViInt16 LFModulationType)
{
   return Pro8_WdmCwSetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Type
// Purpose:  This function retuns the type of LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetLfModType (ViSession instrumentHandle, ViInt16 *LFModulationType)
{
   return Pro8_WdmCwGetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Amplitude
// Purpose:  This function sets the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetLfAmplitude (ViSession instrumentHandle, ViReal64 LFAmplitude)
{
   return Pro8_WdmCwSetLfAmplitude (instrumentHandle, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Amplitude
// Purpose:  This function returns the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetLfAmplitude (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFAmplitude)
{
   return Pro8_WdmCwGetLfAmplitude (instrumentHandle, value, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Frequency
// Purpose:  This function sets the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetLfFrequency (ViSession instrumentHandle, ViReal64 LFFrequency)
{
   return Pro8_WdmCwSetLfFrequency (instrumentHandle, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Frequency
// Purpose:  This function returns the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetLfFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFFrequency)
{
   return Pro8_WdmEaGetLfFrequency (instrumentHandle, value, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetPowerdBm (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerdBm (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetPowerdBm (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   return Pro8_WdmCcdmGetPowerdBm (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Set Laser Power
// Purpose:  This function sets the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetPowerW (ViSession instrumentHandle, ViReal64 power)
{
   return Pro8_LsSetPowerW (instrumentHandle, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetPowerW (ViSession instrumentHandle, ViInt16 value, ViReal64 *power)
{
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   return Pro8_WdmCcdmGetPowerW (instrumentHandle, value, power);
}

//---------------------------------------------------------------------------
// Function: Set Temperature Difference
// Purpose:  This function sets the temperature difference
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmSetTempDiff (ViSession instrumentHandle, ViReal64 temperatureDifference)
{
   return Pro8_LsSetTempDiff (instrumentHandle, temperatureDifference);
}

//---------------------------------------------------------------------------
// Function: Get Temperature Difference
// Purpose:  This function returns the temperature difference
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetTempDifference (ViSession instrumentHandle, ViInt16 value, ViPReal64 temperatureDifference)
{
   return Pro8_LsGetTempDifference(instrumentHandle, value, temperatureDifference);
}

//---------------------------------------------------------------------------
// Function: Get Wavelength
// Purpose:  This function returns the wavelength
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetWavelength (ViSession instrumentHandle, ViReal64 *wavelength)
{
   return Pro8_WdmCwGetWavelength (instrumentHandle, 2, wavelength);
}

//---------------------------------------------------------------------------
// Function: Get Frequency
// Purpose:  This function returns the frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmCwdmGetFrequency (ViSession instrumentHandle, ViReal64 *frequency)
{
   return Pro8_WdmCwGetFrequency (instrumentHandle, 2, frequency);
}


//===========================================================================
// WDM-ITC Functions
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set Laser Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_LdcSetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_LdcGetLdOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   return Pro8_LsSetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   return Pro8_LsGetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation
// Purpose:  This function switches the LF modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetLfMod (ViSession instrumentHandle, ViInt16 LFModulation)
{
   return Pro8_WdmCwSetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation
// Purpose:  This function retuns the state of the LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetLfMod (ViSession instrumentHandle, ViInt16 *LFModulation)
{
   return Pro8_WdmCwGetLfMod (instrumentHandle, LFModulation);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Type
// Purpose:  This function switches the LF modulation type
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetLfModType (ViSession instrumentHandle, ViInt16 LFModulationType)
{
   return Pro8_WdmCwSetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Type
// Purpose:  This function retuns the type of LF modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetLfModType (ViSession instrumentHandle, ViInt16 *LFModulationType)
{
   return Pro8_WdmCwGetLfModType (instrumentHandle, LFModulationType);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Amplitude
// Purpose:  This function sets the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetLfAmplitude (ViSession instrumentHandle, ViReal64 LFAmplitude)
{
   return Pro8_WdmCwSetLfAmplitude (instrumentHandle, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Amplitude
// Purpose:  This function returns the LF modulation amplitude
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetLfAmplitude (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFAmplitude)
{
   return Pro8_WdmCwGetLfAmplitude (instrumentHandle, value, LFAmplitude);
}

//---------------------------------------------------------------------------
// Function: Set LF Modulation Frequency
// Purpose:  This function sets the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetLfFrequency (ViSession instrumentHandle, ViReal64 LFFrequency)
{
   return Pro8_WdmCwSetLfFrequency (instrumentHandle, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Get LF Modulation Frequency
// Purpose:  This function returns the LF modulation frequency
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetLfFrequency (ViSession instrumentHandle, ViInt16 value, ViReal64 *LFFrequency)
{
   return Pro8_WdmCwGetLfFrequency (instrumentHandle, value, LFFrequency);
}

//---------------------------------------------------------------------------
// Function: Set Laser Current
// Purpose:  This function sets the current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetLdCurrent (ViSession instrumentHandle, ViReal64 current)
{
   return Pro8_LdcSetLdCurrent (instrumentHandle, current);
}

//---------------------------------------------------------------------------
// Function: Get Laser Current
// Purpose:  This function returns the current of the laserdiode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetLdCurrent (ViSession instrumentHandle, ViInt16 value, ViReal64 *current)
{
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   return Pro8_LdcGetLdCurrent (instrumentHandle, value, current);
}

//---------------------------------------------------------------------------
// Function: Set LD Limit Current
// Purpose:  This function sets the limit current of the laser diode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetLdLimit (ViSession instrumentHandle, ViReal64 limitCurrent)
{
   return Pro8_LdcSetLdLimit (instrumentHandle, limitCurrent);
}

//---------------------------------------------------------------------------
// Function: Get LD Limit Current
// Purpose:  This function returns the limit current of the laser diode
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetLdLimit (ViSession instrumentHandle, ViInt16 value, ViReal64 *limitCurrent)
{
   return Pro8_LdcGetLdLimit (instrumentHandle, value, limitCurrent);
}

//---------------------------------------------------------------------------
// Function: Set Temperature
// Purpose:  This function sets the temperature
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetTemp (ViSession instrumentHandle, ViReal64 temperature)
{
   return Pro8_TecSetTemp (instrumentHandle, temperature);
}

//---------------------------------------------------------------------------
// Function: Get Temperature
// Purpose:  This function returns the temperature
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetTemp (ViSession instrumentHandle, ViInt16 value, ViReal64 *temperature)
{
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   return Pro8_TecGetTemp (instrumentHandle, value, temperature);
}

//---------------------------------------------------------------------------
// Function: Set Resistance
// Purpose:  This function sets the resistance
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcSetRes (ViSession instrumentHandle, ViReal64 resistance)
{
   return Pro8_TecSetRes (instrumentHandle, resistance);
}

//---------------------------------------------------------------------------
// Function: Get Resistance
// Purpose:  This function returns the resistance
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_WdmItcGetRes (ViSession instrumentHandle, ViInt16 value, ViReal64 *resistance)
{
   if (Pro8_invalidViInt16Range (value, 0, 2)) return VI_ERROR_PARAMETER2;
   return Pro8_TecGetRes (instrumentHandle, value, resistance);
}


//===========================================================================
// BBS Functions
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set LED Output
// Purpose:  This function switches the output on or off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_BbsSetOutput (ViSession instrumentHandle, ViInt16 output)
{
   return Pro8_SledSetOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Get Laser Output
// Purpose:  This function returns the state of the output
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_BbsGetOutput (ViSession instrumentHandle, ViInt16 *output)
{
   return Pro8_SledGetOutput (instrumentHandle, output);
}

//---------------------------------------------------------------------------
// Function: Set Synchronous Modulation
// Purpose:  This function switches the sync. modulation on and off
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_BbsSetSyncMod (ViSession instrumentHandle, ViInt16 synchronousModulation)
{
   return Pro8_LsSetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Synchronous Modulation
// Purpose:  This function retuns the state of the sync. modulation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_BbsGetSyncMod (ViSession instrumentHandle, ViInt16 *synchronousModulation)
{
   return Pro8_LsGetSyncMod (instrumentHandle, synchronousModulation);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (dBm)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_BbsGetPowerdBm (ViSession instrumentHandle, ViReal64 *power)
{
   return Pro8_WdmCcdmGetPowerdBm (instrumentHandle, 2, power);
}

//---------------------------------------------------------------------------
// Function: Get Laser Power
// Purpose:  This function returns the laser power (W)
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_BbsGetPowerW (ViSession instrumentHandle, ViReal64 *power)
{
   return Pro8_WdmCcdmGetPowerW (instrumentHandle, 2, power);
}


//===========================================================================
// ELCH Functions
//===========================================================================
//---------------------------------------------------------------------------
// Function: Set ELCH Parameters
// Purpose:  This function sets the parameters for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ElchSetParameters (ViSession instrumentHandle, ViInt16 steps, ViInt16 measurementValues)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (steps, 2, 1001))           return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViInt16Range (measurementValues, 1, 8))  return VI_ERROR_PARAMETER3;
   //Formatting
   Fmt (buffer, ":ELCH:STEPS %d;:ELCH:MEAS %d", steps, measurementValues);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get ELCH Parameters
// Purpose:  This function returns the parameters for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ElchGetParameters (ViSession instrumentHandle, ViInt16 *steps, ViInt16 *measurementValues)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViBoolean answer;
   ViString  format1[] = {"%*s %d", "%d"};
   ViString  format2[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":ELCH:STEPS?", 12, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format1[answer], steps)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":ELCH:MEAS?", 11, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format2[answer], measurementValues)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: ELCH Run
// Purpose:  This function starts a electrical characterisation
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ElchRun (ViSession instrumentHandle, ViInt16 function)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (function, 0, 2)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":ELCH:RUN %d", function);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: Get ELCH Measurement Values
// Purpose:  This function returns ELCH measurement values
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ElchGetMeasVal (ViSession instrumentHandle, ViInt16 measurementValues, ViReal64 *value1, ViReal64 *value2, ViReal64 *value3, ViReal64 *value4, ViReal64 *value5, ViReal64 *value6, ViReal64 *value7, ViReal64 *value8)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViBoolean answer;
   ViString  format1[] = {"%*s %LE",                             "%LE"};
   ViString  format2[] = {"%*s %LE,%LE",                         "%LE,%LE"};
   ViString  format3[] = {"%*s %LE,%LE,%LE",                     "%LE,%LE,%LE"};
   ViString  format4[] = {"%*s %LE,%LE,%LE,%LE",                 "%LE,%LE,%LE,%LE"};
   ViString  format5[] = {"%*s %LE,%LE,%LE,%LE,%LE",             "%LE,%LE,%LE,%LE,%LE"};
   ViString  format6[] = {"%*s %LE,%LE,%LE,%LE,%LE,%LE",         "%LE,%LE,%LE,%LE,%LE,%LE"};
   ViString  format7[] = {"%*s %LE,%LE,%LE,%LE,%LE,%LE,%LE",     "%LE,%LE,%LE,%LE,%LE,%LE,%LE"};
   ViString  format8[] = {"%*s %LE,%LE,%LE,%LE,%LE,%LE,%LE,%LE", "%LE,%LE,%LE,%LE,%LE,%LE,%LE,%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementValues, 1, 8)) return VI_ERROR_PARAMETER3;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, ":ELCH:TRIG?", 11, &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Reading
   switch (measurementValues)
   {
      case 1:
         status = viScanf (instrumentHandle, format1[answer], value1);
         break;
      case 2:
         status = viScanf (instrumentHandle, format2[answer], value1, value2);
         break;
      case 3:
         status = viScanf (instrumentHandle, format3[answer], value1, value2, value3);
         break;
      case 4:
         status = viScanf (instrumentHandle, format4[answer], value1, value2, value3, value4);
         break;
      case 5:
         status = viScanf (instrumentHandle, format5[answer], value1, value2, value3, value4, value5);
         break;
      case 6:
         status = viScanf (instrumentHandle, format6[answer], value1, value2, value3, value4, value5, value6);
         break;
      case 7:
         status = viScanf (instrumentHandle, format7[answer], value1, value2, value3, value4, value5, value6, value7);
         break;
      case 8:
         status = viScanf (instrumentHandle, format8[answer], value1, value2, value3, value4, value5, value6, value7, value8);
         break;
   }
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: LDC Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"ILD", "IMD"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (setParameter, 0, 1)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":%s:START %f;:%s:STOP %f", command[setParameter], startValue, command[setParameter], stopValue);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: LDC Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command1[] = {":ILD:START?", ":IMD:START?"};
   ViString  command2[] = {":ILD:STOP?",  ":IMD:STOP?" };
   ViBoolean answer;
   ViString  format1[] = {"%*s %LE", "%LE"};
   ViString  format2[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (setParameter, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command1[setParameter], StringLength (command1[setParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format1[answer], startValue)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command2[setParameter], StringLength (command2[setParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format2[answer], stopValue)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: LDC Set ELCH Measurement Value Position
// Purpose:  This function sets the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcSetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 position)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"ILD", "IMD", "VLD"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 2))  return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViInt16Range (position, 1, 8))              return VI_ERROR_PARAMETER3;
   //Formatting
   Fmt (buffer, ":%s:MEAS %d", command[measurementParameter], position);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: LDC Get ELCH Measurement Value Position
// Purpose:  This function returns the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LdcGetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 *position)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":ILD:MEAS?", ":IMD:MEAS?", ":VLD:MEAS?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 2))  return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[measurementParameter], StringLength (command[measurementParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], position)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: MLC Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   return Pro8_LdcSetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: MLC Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   return Pro8_LdcGetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: MLC Set ELCH Measurement Value Position
// Purpose:  This function sets the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcSetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 position)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViChar    buffer[CMD_BUF_SIZE];
   ViString  command[] = {"ILD", "IMD"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 1))  return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViInt16Range (position, 1, 8))              return VI_ERROR_PARAMETER3;
   //Formatting
   Fmt (buffer, ":%s:MEAS %d", command[measurementParameter], position);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: MLC Get ELCH Measurement Value Position
// Purpose:  This function returns the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_MlcGetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 *position)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":ILD:MEAS?", ":IMD:MEAS?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 1))  return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[measurementParameter], StringLength (command[measurementParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], position)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: TEC Set ELCH Measurement Value Position
// Purpose:  This function sets the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecSetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 position)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"ITE", "VTE", "RESI", "TEMP"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 3))  return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViInt16Range (position, 1, 8))              return VI_ERROR_PARAMETER3;
   //Formatting
   Fmt (buffer, ":%s:MEAS %d", command[measurementParameter], position);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: TEC Get ELCH Measurement Value Position
// Purpose:  This function returns the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_TecGetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 *position)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":ITE:MEAS?", ":VTE:MEAS?", ":RESI:MEAS?", ":TEMP:MEAS?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 3))  return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[measurementParameter], StringLength (command[measurementParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], position)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: ITC Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"ILD", "IMD", "VBIAS"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (setParameter, 0, 2)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":%s:START %f;:%s:STOP %f", command[setParameter], startValue, command[setParameter], stopValue);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: ITC Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   ViStatus status     = VI_SUCCESS;
   ViUInt32 retCnt     = 0;
   ViUInt16 stb;
   ViString command1[] = {":ILD:START?", ":IMD:START?", ":VBIAS:START?"};
   ViString command2[] = {":ILD:STOP?",  ":IMD:STOP?" , ":VBIAS:STOP?" };
   ViBoolean answer;
   ViString  format1[] = {"%*s %LE", "%LE"};
   ViString  format2[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (setParameter, 0, 2)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command1[setParameter], StringLength (command1[setParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format1[answer], startValue)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command2[setParameter], StringLength (command2[setParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format2[answer], stopValue)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: ITC Set ELCH Measurement Value Position
// Purpose:  This function sets the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcSetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 position)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"ILD", "IMD", "VLD", "ITE", "VTE", "RESI", "TEMP"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 6))  return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViInt16Range (position, 1, 8))              return VI_ERROR_PARAMETER3;
   //Formatting
   Fmt (buffer, ":%s:MEAS %d", command[measurementParameter], position);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: ITC Get ELCH Measurement Value Position
// Purpose:  This function returns the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_ItcGetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 *position)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViString command[] = {":ILD:MEAS?", ":IMD:MEAS?", ":VLD:MEAS?", ":ITE:MEAS?", ":VTE:MEAS?", ":RESI:MEAS?", ":TEMP:MEAS?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 6))  return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[measurementParameter], StringLength (command[measurementParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], position)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: PDA Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"VBIAS"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (setParameter, 0, 0)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":%s:START %f;:%s:STOP %f", command[setParameter], startValue, command[setParameter], stopValue);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: PDA Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   ViStatus status     = VI_SUCCESS;
   ViUInt32 retCnt     = 0;
   ViUInt16 stb;
   ViString command1[] = {":VBIAS:START?"};
   ViString command2[] = {":VBIAS:STOP?" };
   ViBoolean answer;
   ViString  format1[] = {"%*s %LE", "%LE"};
   ViString  format2[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (setParameter, 0, 0)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command1[setParameter], StringLength (command1[setParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format1[answer], startValue)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command2[setParameter], StringLength (command2[setParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format2[answer], stopValue)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: PDA Set ELCH Measurement Value Position
// Purpose:  This function sets the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaSetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 position)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"IPD"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 0))  return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViInt16Range (position, 1, 8))              return VI_ERROR_PARAMETER3;
   //Formatting
   Fmt (buffer, ":%s:MEAS %d", command[measurementParameter], position);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: PDA Get ELCH Measurement Value Position
// Purpose:  This function returns the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_PdaGetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 *position)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":IPD:MEAS?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 0))  return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[measurementParameter], StringLength (command[measurementParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], position)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: LS Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"P_DBM", "P_W"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (setParameter, 0, 1)) return VI_ERROR_PARAMETER2;
   //Formatting
   Fmt (buffer, ":%s:START %f;:%s:STOP %f", command[setParameter], startValue, command[setParameter], stopValue);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: LS Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_LsGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command1[] = {":P_DBM:START?", ":P_W:START?"};
   ViString  command2[] = {":P_DBM:STOP?",  ":P_W:STOP?" };
   ViBoolean answer;
   ViString  format1[] = {"%*s %LE", "%LE"};
   ViString  format2[] = {"%*s %LE", "%LE"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (setParameter, 0, 1)) return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command1[setParameter], StringLength (command1[setParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format1[answer], startValue)) < 0) return status;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command2[setParameter], StringLength (command2[setParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format2[answer], stopValue)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: SLED Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   return Pro8_LsSetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: SLED Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_SledGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   return Pro8_LsGetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM-CW Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_CwSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   return Pro8_LsSetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM_CW Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_CwGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   return Pro8_LsGetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM-EA Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_EaSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   return Pro8_LsSetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM_EA Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_EaGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   return Pro8_LsGetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM-DIR Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_DirSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   return Pro8_LsSetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM_DIR Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_DirGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   return Pro8_LsGetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM-CCDM Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_CcdmSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   return Pro8_LsSetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM_CCDM Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_CcdmGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   return Pro8_LsGetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM-CCDM Set ELCH Measurement Value Position
// Purpose:  This function sets the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_CcdmSetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 position)
{
   ViStatus status = VI_SUCCESS;
   ViUInt32 retCnt = 0;
   ViUInt16 stb;
   ViChar   buffer[CMD_BUF_SIZE];
   ViString command[] = {"P_DBM", "P_W"};

   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 1))  return VI_ERROR_PARAMETER2;
   if (Pro8_invalidViInt16Range (position, 1, 8))              return VI_ERROR_PARAMETER3;
   //Formatting
   Fmt (buffer, ":%s:MEAS %d", command[measurementParameter], position);
   //Writing - Poll STB - Check EAV Bit and read error
   if ((status = viWrite (instrumentHandle, buffer, StringLength (buffer), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: WDM-CCDM Get ELCH Measurement Value Position
// Purpose:  This function returns the position of the measurement value
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_CcdmGetElchMeasVal (ViSession instrumentHandle, ViInt16 measurementParameter, ViInt16 *position)
{
   ViStatus  status = VI_SUCCESS;
   ViUInt32  retCnt = 0;
   ViUInt16  stb;
   ViString  command[] = {":P_DBM:MEAS?", ":P_W:MEAS?"};
   ViBoolean answer;
   ViString  format[] = {"%*s %d", "%d"};

   if ((status = viGetAttribute (instrumentHandle, VI_ATTR_USER_DATA, &answer)) < 0) return status;
   //Check input parameter ranges
   if (Pro8_invalidViInt16Range (measurementParameter, 0, 1))  return VI_ERROR_PARAMETER2;
   //Writing - Poll STB - Check EAV Bit and read error - Reading
   if ((status = viWrite (instrumentHandle, command[measurementParameter], StringLength (command[measurementParameter]), &retCnt)) < 0) return status;
   if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) return status;
   if (stb & 0x0004) return Pro8_GetInstrumentError (instrumentHandle);
   if ((status = viScanf (instrumentHandle, format[answer], position)) < 0) return status;
   //Ready
   return status;
}

//---------------------------------------------------------------------------
// Function: WDM-CWDM Set ELCH Set Values
// Purpose:  This function sets the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_CwdmSetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 startValue, ViReal64 stopValue)
{
   return Pro8_LsSetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}

//---------------------------------------------------------------------------
// Function: WDM_CWDM Get ELCH Set Values
// Purpose:  This function returns the start and stop values for ELCH
//---------------------------------------------------------------------------
ViStatus _VI_FUNC Pro8_CwdmGetElchSetVal (ViSession instrumentHandle, ViInt16 setParameter, ViReal64 *startValue, ViReal64 *stopValue)
{
   return Pro8_LsGetElchSetVal (instrumentHandle, setParameter, startValue, stopValue);
}


//===========================================================================
// UTILITY ROUTINES (Non-Exportable Functions)
//===========================================================================
//---------------------------------------------------------------------------
// Function: Wait for FIN Bit (Bit0 in Status Byte)
// Purpose:  This function waits for the FIN (finished) Bit and returns
//           the status byte.
//---------------------------------------------------------------------------
ViStatus Pro8_WaitForFinBit (ViSession instrumentHandle, ViUInt16 *stb)
{
   ViStatus  status  = VI_SUCCESS;
   ViBoolean ready   = VI_FALSE;
   ViBoolean error   = VI_FALSE;
   ViUInt16  counter = PRO8_POLL_STB_LOOP_COUNTER;
   ViUInt16  value   = 0;

   //Preset
   *stb = 0;
   //Loop
   do
   {
      //Poll STB
      if ((status = viReadSTB (instrumentHandle, &value)) < 0) error = VI_TRUE;
      else
      {
         //Check FIN Bit
         if (value & 0x0001) ready = VI_TRUE;
         else
         {
            //Delay and decrement counter
            Delay (PRO8_POLL_STB_LOOP_DELAY);
            counter --;
         }
      }
   }
   while ((ready == VI_FALSE) && (error == VI_FALSE) && (counter > 0));
   //Evaluate
   if (error == VI_TRUE)   return status;
   if (counter == 0)       return VI_ERROR_POLL_FIN_BIT;
   //Ready
   *stb = value;
   return VI_SUCCESS;
}

//---------------------------------------------------------------------------
// Function: Get Instrument Error Code
// Purpose:  This function queries the instruments error queue until the
//           queue is empty and returns the error code.
//---------------------------------------------------------------------------
ViStatus Pro8_GetInstrumentError (ViSession instrumentHandle)
{
   ViStatus  status  = VI_SUCCESS;
   ViBoolean ready   = VI_FALSE;
   ViBoolean error   = VI_FALSE;
   ViUInt16  counter = PRO8_GET_ERROR_LOOP_COUNTER;
   ViUInt32  retCnt  = 0;
   ViUInt16  value   = 0;
   ViUInt16  stb     = 0;

   //Loop
   do
   {
      //Writing
      if ((status = viWrite (instrumentHandle, ":SYST:ERR?", 10, &retCnt)) < 0) error = VI_TRUE;
      //Poll STB
      if (error == VI_FALSE)
      {
         if ((status = Pro8_WaitForFinBit (instrumentHandle, &stb)) < 0) error = VI_TRUE;
         //Reading
         if (error == VI_FALSE)
         {
            if ((status = viScanf (instrumentHandle, "%ld", &value)) < 0) error = VI_TRUE;
            //Evaluate STB
            if (error == VI_FALSE)
            {
               if ((stb & 0x0004) == 0) ready = VI_TRUE;
               else counter --;
            }
         }
      }
   }
   while ((ready == VI_FALSE) && (error == VI_FALSE) && (counter > 0));
   //Evaluate
   if (error == VI_TRUE)   return status;
   if (counter == 0)       return VI_ERROR_GET_INSTR_ERROR;
   //Ready
   return (value + VI_INSTR_ERROR_OFFSET);
}

//---------------------------------------------------------------------------
// Function: Boolean Value Out Of Range - ViBoolean
// Purpose:  This function checks a Boolean to see if it is equal to VI_TRUE
//           or VI_FALSE. If the value is out of range, the return value is
//           VI_TRUE, otherwise the return value is VI_FALSE.
//---------------------------------------------------------------------------
ViBoolean Pro8_invalidViBooleanRange (ViBoolean val)
{
   return ((val != VI_FALSE && val != VI_TRUE) ? VI_TRUE : VI_FALSE);
}

//---------------------------------------------------------------------------
// Function: Short Signed Integer Value Out Of Range - ViInt16
// Purpose:  This function checks a short signed integer value to see if it
//           lies between a minimum and maximum value.  If the value is out
//           of range, the return value is VI_TRUE, otherwise the return
//           value is VI_FALSE.
//---------------------------------------------------------------------------
ViBoolean Pro8_invalidViInt16Range (ViInt16 val, ViInt16 min, ViInt16 max)
{
   return ((val < min || val > max) ? VI_TRUE : VI_FALSE);
}

//---------------------------------------------------------------------------
// Function: Long Signed Integer Value Out Of Range - ViInt32
// Purpose:  This function checks a long signed integer value to see if it
//           lies between a minimum and maximum value.  If the value is out
//           of range, the return value is VI_TRUE, otherwise the return
//           value is VI_FALSE.
//---------------------------------------------------------------------------
ViBoolean Pro8_invalidViInt32Range (ViInt32 val, ViInt32 min, ViInt32 max)
{
   return ((val < min || val > max) ? VI_TRUE : VI_FALSE);
}

//---------------------------------------------------------------------------
// Function: Unsigned Char Value Out Of Range - ViUInt8
// Purpose:  This function checks an unsigned char value to see if it
//           lies between a minimum and maximum value.  If the value is out
//           of range, the return value is VI_TRUE, otherwise the return
//           value is VI_FALSE.
//---------------------------------------------------------------------------
ViBoolean Pro8_invalidViUInt8Range (ViUInt8 val, ViUInt8 min, ViUInt8 max)
{
   return ((val < min || val > max) ? VI_TRUE : VI_FALSE);
}

//---------------------------------------------------------------------------
// Function: Short Unsigned Integer Value Out Of Range - ViUInt16
// Purpose:  This function checks a short unsigned integer value to see if it
//           lies between a minimum and maximum value.  If the value is out
//           of range, the return value is VI_TRUE, otherwise the return
//           value is VI_FALSE.
//---------------------------------------------------------------------------
ViBoolean Pro8_invalidViUInt16Range (ViUInt16 val, ViUInt16 min, ViUInt16 max)
{
   return ((val < min || val > max) ? VI_TRUE : VI_FALSE);
}

//---------------------------------------------------------------------------
// Function: Long Unsigned Integer Value Out Of Range - ViUInt32
// Purpose:  This function checks a long unsigned integer value to see if it
//           lies between a minimum and maximum value.  If the value is out
//           of range, the return value is VI_TRUE, otherwise the return
//           value is VI_FALSE.
//---------------------------------------------------------------------------
ViBoolean Pro8_invalidViUInt32Range (ViUInt32 val, ViUInt32 min, ViUInt32 max)
{
   return ((val < min || val > max) ? VI_TRUE : VI_FALSE);
}

//---------------------------------------------------------------------------
// Function: Real (Float) Value Out Of Range - ViReal32
// Purpose:  This function checks a real (float) value to see if it lies
//           between a minimum and maximum value.  If the value is out of
//           range, the return value is VI_TRUE, otherwise the return value
//           is VI_FALSE.
//---------------------------------------------------------------------------
ViBoolean Pro8_invalidViReal32Range (ViReal32 val, ViReal32 min, ViReal32 max)
{
   return ((val < min || val > max) ? VI_TRUE : VI_FALSE);
}

//---------------------------------------------------------------------------
// Function: Real (Double) Value Out Of Range - ViReal64
// Purpose:  This function checks a real (double) value to see if it lies
//           between a minimum and maximum value.  If the value is out of
//           range, the return value is VI_TRUE, otherwise the return value
//           is VI_FALSE.
//---------------------------------------------------------------------------
ViBoolean Pro8_invalidViReal64Range (ViReal64 val, ViReal64 min, ViReal64 max)
{
   return ((val < min || val > max) ? VI_TRUE : VI_FALSE);
}

//---------------------------------------------------------------------------
// Function: Initialize Clean Up
// Purpose:  This function is used only by the Pro8_init function.  When
//           an error is detected this function is called to close the
//           open resource manager and instrument object sessions and to
//           set the instrSession that is returned from Pro8_init to
//           VI_NULL.
//---------------------------------------------------------------------------
ViStatus Pro8_initCleanUp (ViSession openRMSession, ViPSession openInstrSession, ViStatus currentStatus)
{
   viClose (*openInstrSession);
   viClose (openRMSession);
   *openInstrSession = VI_NULL;
   //Ready
   return currentStatus;
}

/****************************************************************************

  End of Source file

****************************************************************************/
