/** @file ash-host.c
 *  @brief  ASH protocol Host functions
 * 
 * <!-- Copyright 2009 by Ember Corporation. All rights reserved.       *80*-->
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"
#include "hal/micro/generic/ash-protocol.h"
#include "hal/micro/generic/ash-common.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-priv.h"
#include "app/ezsp-uart-host/ash-host-queues.h"
#include "app/ezsp-uart-host/ash-host-ui.h"

//------------------------------------------------------------------------------
// Preprocessor definitions

// Values for sendState
#define SEND_STATE_IDLE       0
#define SEND_STATE_SHFRAME    1
#define SEND_STATE_TX_DATA    2
#define SEND_STATE_RETX_DATA  3

// Bits in ashFlags
#define FLG_REJ               0x01          // Reject Condition
#define FLG_RETX              0x02          // Retransmit Condition
#define FLG_NAK               0x04          // send NAK
#define FLG_ACK               0x08          // send ACK
#define FLG_RST               0x10          // send RST
#define FLG_CAN               0x20          // cancel DATA frame in progress
#define FLG_CONNECTED         0x40          // in CONNECTED state, else ERROR
#define FLG_NR               0x100          // not ready to receive DATA frames
#define FLG_NRTX             0x200          // last transmitted NR status

// Values returned by ashFrameType()
#define TYPE_INVALID          0
#define TYPE_DATA             1
#define TYPE_ACK              2
#define TYPE_NAK              3
#define TYPE_RSTACK           4
#define TYPE_ERROR            5

#define txControl (txBuffer[0])             // more descriptive aliases
#define rxControl (rxBuffer[0])
#define TX_BUFFER_LEN ASH_HOST_SHFRAME_TX_LEN
#define RX_BUFFER_LEN ASH_HOST_SHFRAME_RX_LEN

//------------------------------------------------------------------------------
// Global Variables

EzspStatus ncpError;                        // ncp error or reset code
EzspStatus ashError;                        // host error code

AshCount ashCount;                          // struct of ASH counters

// Config 0 (default) : EM2xx @ 115200 bps with RTS/CTS flow control 
#define ASH_HOST_CONFIG_DEFAULT                                                \
{                                                                              \
  "/dev/ttyS0",         /* serial port name                                  */\
  115200,               /* baud rate (bits/second)                           */\
  1,                    /* stop bits                                         */\
  TRUE,                 /* TRUE enables RTS/CTS flow control, FALSE XON/XOFF */\
  256,                  /* max bytes to buffer before writing to serial port */\
  256,                  /* max bytes to read ahead from serial port          */\
  0,                    /* trace output control bit flags                    */\
  3,                    /* max frames sent without being ACKed (1-7)         */\
  TRUE,                 /* enables randomizing DATA frame payloads           */\
  800,                  /* adaptive rec'd ACK timeout initial value          */\
  400,                  /*  "     "     "     "     "  minimum value         */\
  2400,                 /*  "     "     "     "     "  maximum value         */\
  2500,                 /* time allowed to receive RSTACK after ncp is reset */\
  RX_FREE_LWM,          /* if free buffers < limit, host receiver isn't ready */\
  RX_FREE_HWM,          /* if free buffers > limit, host receiver is ready   */\
  480,                  /* time after which a set nFlag must be resent       */\
  ASH_RESET_METHOD_RST, /* method used to reset ncp                          */\
  ASH_NCP_TYPE_EM2XX    /* type of ncp processor                             */\
}

// Host configuration structure
AshHostConfig ashHostConfig = ASH_HOST_CONFIG_DEFAULT;

//------------------------------------------------------------------------------
// Local Variables

static AshHostConfig ashHostConfigArray[] =
{
  // Config 0: defined above
  ASH_HOST_CONFIG_DEFAULT,

  { // Config 1: EM2xx @ 57600 bps with XON/XOFF flow control
    "/dev/ttyS0",         // serial port name
    57600,                // baud rate (bits/second)
    1,                    // stop bits
    FALSE,                // TRUE enables RTS/CTS flow control, FALSE XON/XOFF
    256,                  // max bytes to buffer before writing to serial port
    256,                  // max bytes to read ahead from serial port
    0,                    // trace output control bit flags
    3,                    // max frames sent without being ACKed (1-7)
    TRUE,                 // enables randomizing DATA frame payloads
    800,                  // adaptive rec'd ACK timeout initial value
    400,                  //  "     "     "     "     "  minimum value
    2400,                 //  "     "     "     "     "  maximum value
    2500,                 // time allowed to receive RSTACK after ncp is reset
    RX_FREE_LWM,          // if free buffers < limit, host receiver isn't ready
    RX_FREE_HWM,          // if free buffers > limit, host receiver is ready
    480,                  // time after which a set nFlag must be resent
    ASH_RESET_METHOD_RST, // method used to reset ncp
    ASH_NCP_TYPE_EM2XX    // type of ncp processor
  },

  { // Config 2: AVR ATmega 128 @38400 with XON/XOFF flow control
    "/dev/ttyS0",         // serial port name
    38400,                // baud rate (bits/second)
    1,                    // stop bits
    FALSE,                // TRUE enables RTS/CTS flow control, FALSE XON/XOFF
    256,                  // max bytes to buffer before writing to serial port
    256,                  // max bytes to read ahead from serial port
    0,                    // trace output control bit flags
    3,                    // max frames sent without being ACKed (1-7)
    TRUE,                 // enables randomizing DATA frame payloads
    1000,                 // adaptive rec'd ACK timeout initial value
    500,                  //  "     "     "     "     "  minimum value
    2800,                 //  "     "     "     "     "  maximum value
    1500,                 // time allowed to receive RSTACK after ncp is reset
    RX_FREE_LWM,          // if free buffers < limit, host receiver isn't ready
    RX_FREE_HWM,          // if free buffers > limit, host receiver is ready
    480,                  // time after which a set nFlag must be resent
    ASH_RESET_METHOD_RST, // method used to reset ncp
    ASH_NCP_TYPE_AVR      // type of ncp processor
  }
};

static int8u txBuffer[TX_BUFFER_LEN];       // outgoing short frames
static int8u rxBuffer[RX_BUFFER_LEN];       // incoming short frames
static int8u sendState;                     // ashSendExec() state variable
static int8u ackRx;                         // frame ack'ed from remote peer
static int8u ackTx;                         // frame ack'ed to remote peer
static int8u frmTx;                         // next frame to be transmitted
static int8u frmReTx;                       // next frame to be retransmitted
static int8u frmRx;                         // next frame expected to be rec'd
static int8u frmReTxHead;                   // frame at retx queue's head
static int8u ashTimeouts;                   // consecutive timeout counter
static int16u ashFlags;                     // bit flags for top-level logic
static AshBuffer *rxDataBuffer;             // rec'd DATA frame buffer
static int8u rxLen;                         // rec'd frame length

//------------------------------------------------------------------------------
// Forward Declarations

static EzspStatus ashReceiveFrame(void);
static int16u ashFrameType(int16u control, int16u len);
static void ashRejectFrame(void);
static void ashFreeNonNullRxBuffer(void);
static void ashScrubReTxQueue(void);
static void ashStartRetransmission(void);
static void ashInitVariables(void);
static void ashDataFrameFlowControl(void);
static EzspStatus ashHostDisconnect(int8u error);
static EzspStatus ashNcpDisconnect(int8u error);
static EzspStatus ashReadFrame(void);
static void ashRandomizeBuffer(int8u *buffer, int8u len);

//------------------------------------------------------------------------------
// Functions implementing the interface upward to EZSP

EzspStatus ashSelectHostConfig(int8u cfg)
{
  int8u status;

  if (cfg < (sizeof(ashHostConfigArray) / sizeof(ashHostConfigArray[0])) ) {
    ashHostConfig = ashHostConfigArray[cfg];
    status = EZSP_SUCCESS;
  } else {
    ashError = EZSP_ASH_ERROR_NCP_TYPE;
    status = EZSP_ASH_HOST_FATAL_ERROR;
  }
  return status;
}

EzspStatus ashResetNcp(void)
{
  EzspStatus status;

  ashInitVariables();
  ashTraceEvent("\r\n=== ASH started ===\r\n");
  switch(ashReadConfig(resetMethod)) {
  case ASH_RESET_METHOD_RST:    // ask ncp to reset itself using RST frame
    status = ashSerialInit();
    ashFlags = FLG_RST;
    ashSerialWriteByte(ASH_CAN);  // discards any startup noise
    break;
  case ASH_RESET_METHOD_DTR:    // DTR is connected to nRESET
    ashResetDtr();
    status = ashSerialInit();
    break;
  case ASH_RESET_METHOD_CUSTOM: // a hook for a custom reset method
    ashResetCustom();
    status = ashSerialInit();
    break;
  case ASH_RESET_METHOD_NONE:   // no reset - for testing
    status = ashSerialInit();
    break;
  default:
    ashError = EZSP_ASH_ERROR_RESET_METHOD;
    status = EZSP_ASH_HOST_FATAL_ERROR;
    break;
  } // end of switch(ashReadConfig(resetMethod)
  return status;
}

EzspStatus ashStart(void)
{
  EzspStatus status;

  if (ashReadConfig(resetMethod) != ASH_RESET_METHOD_NONE) {
    ashSerialReadFlush();
  }
  ashSetAndStartAckTimer(ashReadConfig(timeRst));
  while (!(ashFlags & FLG_CONNECTED)) {
    ashSendExec();
    status = ashReceiveExec();
    if (ashError == EZSP_ASH_ERROR_RESET_FAIL) {
      status = EZSP_ASH_HOST_FATAL_ERROR;
    }
    if (  (status == EZSP_ASH_HOST_FATAL_ERROR) || 
          (status == EZSP_ASH_NCP_FATAL_ERROR) ) {
      return status;
    }
    simulatedTimePasses();
  }
  ashStopAckTimer();
  return EZSP_SUCCESS;
}

void ashStop(void)
{
  ashTraceEvent("======== ASH stopped ========\r\n");
  ashSerialClose();
  ashInitVariables();
}

EzspStatus ashReceiveExec(void)
{
  EzspStatus status;

  do {
    status = ashReceiveFrame();
    simulatedTimePasses();
  } while(status == EZSP_ASH_IN_PROGRESS);
  return status;
}

static EzspStatus ashReceiveFrame(void)
{
  int8u ackNum;
  int8u frmNum;
  int16u frameType;
  EzspStatus status;

  // Check for errors that might have been detected in ashSendExec()
  if (ashError != EZSP_ASH_NO_ERROR) {
    return EZSP_ASH_HOST_FATAL_ERROR;
  }
  if (ncpError != EZSP_ASH_NO_ERROR) {
    return EZSP_ASH_NCP_FATAL_ERROR;
  }

  // Read data from serial port and assemble a frame until complete, aborted
  // due to an error, cancelled, or there is no more serial data available.
  do {
    status = ashReadFrame();
    switch (status) {
    case EZSP_SUCCESS:
      break;
    case EZSP_ASH_IN_PROGRESS:
      break;
    case EZSP_ASH_NO_RX_DATA:
      return status;
    case EZSP_ASH_CANCELLED:
      if (ashFlags & FLG_CONNECTED) {       // ignore the cancel before RSTACK
        BUMP_HOST_COUNTER(rxCancelled);
        ashTraceEventRecdFrame("cancelled");
      }
      break;
    case EZSP_ASH_BAD_CRC:
      BUMP_HOST_COUNTER(rxCrcErrors);
      ashRejectFrame();
      ashTraceEventRecdFrame("CRC error");
      break;
    case EZSP_ASH_COMM_ERROR:
      BUMP_HOST_COUNTER(rxCommErrors);
      ashRejectFrame();
      ashTraceEventRecdFrame("comm error");
      break;
    case EZSP_ASH_TOO_SHORT:
      BUMP_HOST_COUNTER(rxTooShort);
      ashRejectFrame();
      ashTraceEventRecdFrame("too short");
      break;
    case EZSP_ASH_TOO_LONG:
      BUMP_HOST_COUNTER(rxTooLong);
      ashRejectFrame();
      ashTraceEventRecdFrame("too long");
      break;
    case EZSP_ASH_ERROR_XON_XOFF:
      return ashHostDisconnect(status);
     default:
      assert(FALSE);
    } // end of switch (status)
  } while (status != EZSP_SUCCESS);

  // Got a complete frame - validate its control and length.
  // On an error the type returned will be TYPE_INVALID.
  frameType = ashFrameType(rxControl, rxLen);

  // Free buffer allocated for a received frame if:
  //    DATA frame, and out of order
  //    DATA frame, and not in the CONNECTED state
  //    not a DATA frame
  if (frameType == TYPE_DATA) {
    if ( !(ashFlags & FLG_CONNECTED) || (ASH_GET_FRMNUM(rxControl) != frmRx) ) {
      ashFreeNonNullRxBuffer();
    }
  } else {
    ashFreeNonNullRxBuffer();
  }
  ashTraceFrame(FALSE);                       // trace output (if enabled)

  // Process frames received while not in the connected state -
  // ignore everything except RSTACK and ERROR frames
  if (!(ashFlags & FLG_CONNECTED)) {
    // RSTACK frames have the ncp ASH version in the first data field byte,
    // and the reset reason in the second byte
    if (frameType == TYPE_RSTACK) {
      if (rxBuffer[1] != ASH_VERSION) {
        return ashHostDisconnect(EZSP_ASH_ERROR_VERSION);
      }
      // Ignore a RSTACK if the reset reason doesn't match our reset method
      switch (ashReadConfig(resetMethod)) {
      case ASH_RESET_METHOD_RST:
        if (rxBuffer[2] != RESET_SOFTWARE) {
            return EZSP_ASH_IN_PROGRESS;
        }
        break;
      // Note that the EM2xx reports resets from nRESET as power on resets
      case ASH_RESET_METHOD_DTR:
      case ASH_RESET_METHOD_CUSTOM:
      case ASH_RESET_METHOD_NONE:
        if ( !( (rxBuffer[2] == RESET_EXTERNAL) &&
                (ashReadConfig(ncpType) == ASH_NCP_TYPE_AVR) ) &&
              !( (rxBuffer[2] == RESET_POWERON) &&
                (ashReadConfig(ncpType) == ASH_NCP_TYPE_EM2XX) ) ) {
            return EZSP_ASH_IN_PROGRESS;
        }
        break;
      }
      ncpError = EZSP_ASH_NO_ERROR;
      ashStopAckTimer();
      ashTimeouts = 0;
      ashSetAckPeriod(ashReadConfig(ackTimeInit));
      ashFlags = FLG_CONNECTED | FLG_ACK;
      ashTraceEventTime("ASH connected");
    } else if (frameType == TYPE_ERROR) {
      return ashNcpDisconnect(rxBuffer[2]);
    }
    return EZSP_ASH_IN_PROGRESS;
  }

  // Connected - process the ackNum in ACK, NAK and DATA frames
  if (  (frameType == TYPE_DATA) || 
        (frameType == TYPE_ACK)  ||
        (frameType == TYPE_NAK) ) {
    ackNum = ASH_GET_ACKNUM(rxControl);
    if ( !WITHIN_RANGE(ackRx, ackNum, frmTx) ) {
      BUMP_HOST_COUNTER(rxBadAckNumber);
      ashTraceEvent("bad ackNum");
      frameType = TYPE_INVALID;
    } else if (ackNum != ackRx) {               // new frame(s) ACK'ed?
      ackRx = ackNum;
      ashTimeouts = 0;
      if (ashFlags & FLG_RETX) {                // start timer if unACK'ed frames
        ashStopAckTimer();
        if (ackNum != frmReTx) {
          ashStartAckTimer();
        }
      } else {
        ashAdjustAckPeriod(FALSE);              // factor ACK time into period
        if (ackNum != frmTx) {                  // if more unACK'ed frames,
          ashStartAckTimer();                   // then restart ACK timer
        }
        ashScrubReTxQueue();                    // free buffer(s) in ReTx queue
      }
    }
  }

  // Process frames received while connected
  switch (frameType) {
  case TYPE_DATA:
    frmNum = ASH_GET_FRMNUM(rxControl);
    if (frmNum == frmRx) {                    // is frame in sequence?
      if (rxDataBuffer == NULL) {             // valid frame but no memory?
        BUMP_HOST_COUNTER(rxNoBuffer);
        ashTraceEventRecdFrame("no buffer available");
        ashRejectFrame();
        return EZSP_ASH_NO_RX_SPACE;
      }
      if (rxControl & ASH_RFLAG_MASK) {       // if retransmitted, force ACK
        ashFlags |= FLG_ACK;
      }
      ashFlags &= ~(FLG_REJ | FLG_NAK);       // clear the REJ condition
      INC8(frmRx);
      ashRandomizeBuffer(rxDataBuffer->data, rxDataBuffer->len);
      ashAddQueueTail(&rxQueue, rxDataBuffer);// add frame to receive queue
      ashTraceEzspFrameId("add to queue", rxDataBuffer->data);
      ashTraceEzspVerbose("ashReceiveFrame(): ID=0x%x Seq=0x%x Buffer=%u",
                          rxDataBuffer->data[EZSP_FRAME_ID_INDEX],
                          rxDataBuffer->data[EZSP_SEQUENCE_INDEX],
                          rxDataBuffer);
      ADD_HOST_COUNTER(rxDataBuffer->len, rxData);
      return EZSP_SUCCESS;
    } else {                                  // frame is out of sequence
      if (rxControl & ASH_RFLAG_MASK) {       // if retransmitted, force ACK
        BUMP_HOST_COUNTER(rxDuplicates);
        ashFlags |= FLG_ACK;
      } else {                                // 1st OOS? then set REJ, send NAK
        if ((ashFlags & FLG_REJ) == 0) {
          BUMP_HOST_COUNTER(rxOutOfSequence);
          ashTraceEventRecdFrame("out of sequence");
        }
        ashRejectFrame();
      }
    }
    break;
  case TYPE_ACK:                              // already fully processed
    break;
  case TYPE_NAK:                              // start retransmission if needed
    ashStartRetransmission();
    break;
  case TYPE_RSTACK:                           // unexpected ncp reset
    ncpError = rxBuffer[2];
    return ashHostDisconnect(EZSP_ASH_ERROR_NCP_RESET);
  case TYPE_ERROR:                            // ncp error
    return ashNcpDisconnect(rxBuffer[2]);
  case TYPE_INVALID:                          // reject invalid frames
    ashTraceArray((int8u *)"Rec'd frame:", rxLen, rxBuffer);
    ashRejectFrame();
    break;
  } // end switch(frameType)
  return EZSP_ASH_IN_PROGRESS;
} // end of ashReceiveExec()

void ashSendExec(void)
{
  static int8u offset;
  int8u out, in, len;
  static AshBuffer *buffer;

  // Check for received acknowledgement timer expiry
  if (ashAckTimerHasExpired()) {
    if (ashFlags & FLG_CONNECTED) {
      if (ackRx != ((ashFlags & FLG_RETX) ? frmReTx : frmTx) ) {
        BUMP_HOST_COUNTER(rxAckTimeouts);
        ashAdjustAckPeriod(TRUE);
        ashTraceEventTime("Timer expired waiting for ACK");
        if (++ashTimeouts >= ASH_MAX_TIMEOUTS) {
          (void)ashHostDisconnect(EZSP_ASH_ERROR_TIMEOUTS);
          return;
        }
        ashStartRetransmission();
      } else {
        ashStopAckTimer();
      }
    } else {
      (void)ashHostDisconnect(EZSP_ASH_ERROR_RESET_FAIL);
    }
  } 
 
  while (ashSerialWriteAvailable() == EZSP_SUCCESS) {
    // If NAK was received, cancel data frame if it is in progress
    if (ashFlags & FLG_CAN) {
      if (sendState == SEND_STATE_TX_DATA) {
        BUMP_HOST_COUNTER(txCancelled);
        ashSerialWriteByte(ASH_CAN);
        ashStopAckTimer();
        sendState = SEND_STATE_IDLE;
      }
      ashFlags &= ~FLG_CAN;
      continue;
    }
    switch (sendState) {

    // In between frames - do some housekeeping and decide what to send next
    case SEND_STATE_IDLE:
      // If retransmitting, set the next frame to send to the last ackNum
      // received, then check to see if retransmission is now complete.
      if (ashFlags & FLG_RETX) {
        if (WITHIN_RANGE(frmReTx, ackRx, frmTx)) {
          frmReTx = ackRx;
        }
        if (frmReTx == frmTx) {
          ashFlags &= ~FLG_RETX;
          ashScrubReTxQueue();
        }
      }

      ashDataFrameFlowControl();              // restrain ncp if needed

      // See if a short frame is flagged to be sent
      // The order of the tests below - RST, NAK and ACK -
      // sets the relative priority of sending these frame types.
      if (ashFlags & FLG_RST) {
        txControl = ASH_CONTROL_RST;
        ashSetAndStartAckTimer(ashReadConfig(timeRst));
        len = 1;
        ashFlags &= ~( FLG_RST | FLG_NAK | FLG_ACK );
        sendState = SEND_STATE_SHFRAME;
      } else if (ashFlags & ( FLG_NAK | FLG_ACK ) ) {
        if (ashFlags & FLG_NAK) {
          txControl = ASH_CONTROL_NAK + (frmRx << ASH_ACKNUM_BIT);
          ashFlags &= ~( FLG_NRTX | FLG_NAK | FLG_ACK );
        } else {
          txControl = ASH_CONTROL_ACK + (frmRx << ASH_ACKNUM_BIT);
          ashFlags &= ~( FLG_NRTX | FLG_ACK );
        }          
        if (ashFlags & FLG_NR) {
          txControl |= ASH_NFLAG_MASK;
          ashFlags |= FLG_NRTX;
          ashStartNrTimer();
        }
        ackTx = frmRx;
        len = 1;
        sendState = SEND_STATE_SHFRAME;
      // See if retransmitting DATA frames for error recovery
      } else if (ashFlags & FLG_RETX) {
        buffer = ashQueueNthEntry( &reTxQueue, MOD8(frmTx - frmReTx) );
        len = buffer->len + 1;
        txControl = ASH_CONTROL_DATA |
                      (frmReTx << ASH_FRMNUM_BIT) |
                      (frmRx << ASH_ACKNUM_BIT) |
                      ASH_RFLAG_MASK;
        sendState = SEND_STATE_RETX_DATA;
      // See if an ACK should be generated
      } else if (ackTx != frmRx) {
        ashFlags |= FLG_ACK;
        break;
      // Send a DATA frame if ready
      } else if ( !ashQueueIsEmpty(&txQueue) && 
                   WITHIN_RANGE(ackRx, frmTx, ackRx + ashReadConfig(txK) - 1) ) {
        buffer = ashQueueHead(&txQueue);
        len = buffer->len + 1;
        ADD_HOST_COUNTER(len - 1, txData);
        txControl = ASH_CONTROL_DATA |
                      (frmTx << ASH_FRMNUM_BIT) |
                      (frmRx << ASH_ACKNUM_BIT);
        sendState = SEND_STATE_TX_DATA;
      // Otherwise there's nothing to send
      } else {
        ashSerialWriteFlush();
        return;
      }

      // Start frame - ashEncodeByte() is inited by a non-zero length argument
      ashTraceFrame(TRUE);                    // trace output (if enabled)
      out = ashEncodeByte(len, txControl, &offset);
      ashSerialWriteByte(out);
      break;

    case SEND_STATE_SHFRAME:                  // sending short frame
      if (offset != 0xFF) {
        in = txBuffer[offset];
        out = ashEncodeByte(0, in, &offset);
        ashSerialWriteByte(out);
      } else {
        sendState = SEND_STATE_IDLE;
      }
      break;

    case SEND_STATE_TX_DATA:                  // sending data frame
    case SEND_STATE_RETX_DATA:                // resending data frame
      if (offset != 0xFF) {
        in = offset ? buffer->data[offset - 1] : txControl;
        out = ashEncodeByte(0, in, &offset);
        ashSerialWriteByte(out);
      } else {
        if (sendState == SEND_STATE_TX_DATA) {
          INC8(frmTx);
          buffer = ashRemoveQueueHead(&txQueue);
          ashAddQueueTail(&reTxQueue, buffer);
        } else {
          INC8(frmReTx);
        }
        if (ashAckTimerIsNotRunning()) {
          ashStartAckTimer();
        }
        ackTx = frmRx;
        sendState = SEND_STATE_IDLE;
      }
      break;

    }   // end of switch(sendState)
  }     // end of while (ashSerialWriteAvailable() == EZSP_SUCCESS)
  ashSerialWriteFlush();
} // end of ashSendExec()

boolean ashIsConnected(void)
{
  return ((ashFlags & FLG_CONNECTED) != 0);
}

EzspStatus ashReceive(int8u *len, int8u *inbuf)
{
  AshBuffer *buffer;

  *len = 0;
  if (!(ashFlags & FLG_CONNECTED)) {
    return EZSP_ASH_NOT_CONNECTED;
  }
  if (ashQueueIsEmpty(&rxQueue)) {
    return EZSP_ASH_NO_RX_DATA;
  }
  buffer = ashRemoveQueueHead(&rxQueue);
  memcpy(inbuf, buffer->data, buffer->len);
  *len = buffer->len;
  ashFreeBuffer(&rxFree, buffer);
  ashTraceEzspVerbose("ashReceive(): ashFreeBuffer(): %u", buffer);
  return EZSP_SUCCESS;
}

// After verifying that the data field length is within bounds,
// copies data frame to a buffer and appends it to the transmit queue.
EzspStatus ashSend(int8u len, const int8u *inptr)
{
  AshBuffer *buffer;

  if (len < ASH_MIN_DATA_FIELD_LEN ) {
    return EZSP_ASH_DATA_FRAME_TOO_SHORT;
  } else if (len > ASH_MAX_DATA_FIELD_LEN) {
    return EZSP_ASH_DATA_FRAME_TOO_LONG;
  }
  if (!(ashFlags & FLG_CONNECTED)) {
    return EZSP_ASH_NOT_CONNECTED;
  }
  buffer = ashAllocBuffer(&txFree);
  ashTraceEzspVerbose("ashSend(): ashAllocBuffer(): %u", buffer);
  if (buffer == NULL) {
    return EZSP_ASH_NO_TX_SPACE;
  }
  memcpy(buffer->data, inptr, len);
  buffer->len = len;
  ashRandomizeBuffer(buffer->data, buffer->len);
  ashAddQueueTail(&txQueue, buffer);
  ashSendExec();
  return EZSP_SUCCESS;
}

//------------------------------------------------------------------------------
// Utility functions

// Initialize ASH variables, timers and queues, but not the serial port
static void ashInitVariables()
{
  ashFlags = 0;
  ashDecodeInProgress = FALSE;
  ackRx = 0;
  ackTx = 0;
  frmTx = 0;
  frmReTx = 0;
  frmRx = 0;
  frmReTxHead = 0;
  ashTimeouts = 0;
  ncpError = EZSP_ASH_NO_ERROR;
  ashError = EZSP_ASH_NO_ERROR;
  sendState = SEND_STATE_IDLE;
  ashStopAckTimer();
  ashStopNrTimer();
  ashInitQueues();
  ashClearCounters(&ashCount);
}

// Check free rx buffers to see whether able to receive DATA frames: set or
// clear NR flag appropriately. Inform ncp of our status using the nFlag in 
// ACKs and NAKs. Note that not ready status must be refreshed if it persists
// beyond a maximum time limit.
static void ashDataFrameFlowControl(void) {
  int8u freeRxBuffers;

  if (ashFlags & FLG_CONNECTED) {
    freeRxBuffers = ashFreeListLength(&rxFree);
    // Set/clear NR flag based on the number of buffers free
    if (freeRxBuffers < ashReadConfig(nrLowLimit)) {
      ashFlags |= FLG_NR;
    } else if (freeRxBuffers > ashReadConfig(nrHighLimit)) {
      ashFlags &= ~FLG_NR;
      ashStopNrTimer();    //** needed??
    }
    // Force an ACK (or possibly NAK) if we need to send an updated nFlag
    // due to either a changed NR status or to refresh a set nFlag
    if (ashFlags & FLG_NR) {
      if ( !(ashFlags & FLG_NRTX) || ashNrTimerHasExpired()) {
        ashFlags |= FLG_ACK;
        ashStartNrTimer();
      }
    } else {
      (void)ashNrTimerHasExpired();  // esnure timer checked often
      if (ashFlags & FLG_NRTX) {
        ashFlags |= FLG_ACK;
        ashStopNrTimer();    //** needed???
      }
    }
  } else {
    ashStopNrTimer();
    ashFlags &= ~(FLG_NRTX | FLG_NR);
  }
}

// If not already retransmitting, and there are unacked frames, start 
// retransmitting after the last frame that was acked. 
static void ashStartRetransmission(void)
{
  if ( !(ashFlags & FLG_RETX) && (ackRx != frmTx) ) {
    ashStopAckTimer();
    frmReTx = ackRx;
    ashFlags |= (FLG_RETX | FLG_CAN);
  }  
}

// If the last control byte received was a DATA control,
// and we are connected and not already in the reject condition,
// then send a NAK and set the reject condition.
static void ashRejectFrame(void)
{
  if ( ((rxControl & ASH_DFRAME_MASK) == ASH_CONTROL_DATA) &&
       ((ashFlags & (FLG_REJ | FLG_CONNECTED)) == FLG_CONNECTED) ) {
    ashFlags |= (FLG_REJ | FLG_NAK);
  }
}

static void ashFreeNonNullRxBuffer(void)
{
  if (rxDataBuffer != NULL) {
    ashFreeBuffer(&rxFree, rxDataBuffer);
    ashTraceEzspVerbose("ashFreeNonNullRxBuffer(): ashFreeBuffer(): %u", rxDataBuffer);
    rxDataBuffer = NULL;
  }
}

static void ashScrubReTxQueue(void)
{
  AshBuffer *buffer;

  while (ackRx != frmReTxHead) {
    buffer = ashRemoveQueueHead(&reTxQueue);
    ashFreeBuffer(&txFree, buffer);
    ashTraceEzspVerbose("ashScrubReTxQueue(): ashFreeBuffer(): %u", buffer);
    INC8(frmReTxHead);
  }
}

static EzspStatus ashHostDisconnect(int8u error)
{
  ashFlags = 0;
  ashError = error;
  ashTraceDisconnected(error);
  return EZSP_ASH_HOST_FATAL_ERROR;
}

static EzspStatus ashNcpDisconnect(int8u error)
{
  ashFlags = 0;
  ncpError = error;
  ashTraceDisconnected(error);
  return EZSP_ASH_NCP_FATAL_ERROR;
}

static EzspStatus ashReadFrame(void)
{
  int8u index;
  int8u in;
  int8u out;
  EzspStatus status;

  if (!ashDecodeInProgress) {
    rxLen = 0;
    rxDataBuffer = NULL;
  }

  do {
    // Get next byte from serial port, return if no data
    status = ashSerialReadByte(&in);
    if (status == EZSP_ASH_NO_RX_DATA) {
      break;
    }

    // Decode next input byte - note that many input bytes do not produce
    // an output byte. Return on any error in decoding.
    index = rxLen;
    status = ashDecodeByte(in, &out, &rxLen);
    if ( (status != EZSP_ASH_IN_PROGRESS) && (status != EZSP_SUCCESS) ) {
      ashFreeNonNullRxBuffer();
      break;                  // discard an invalid frame
    }

    if (rxLen != index) {           // if input byte produced an output byte
      if (rxLen <= RX_BUFFER_LEN) { // if a short frame, return in rxBuffer
        rxBuffer[index] = out;
      // If a longer DATA frame, allocate an AshBuffer for it.
      // (Note the control byte is always returned in rxControl. Even if
      // no buffer can be allocated, the control's ackNum must be processed.)
      } else {
        if (rxLen == RX_BUFFER_LEN + 1)  {  // alloc buffer, copy prior data
          rxDataBuffer = ashAllocBuffer(&rxFree);
          ashTraceEzspVerbose("ashReadFrame(): ashAllocBuffer(): %u", rxDataBuffer);
          if (rxDataBuffer != NULL) {
            memcpy(rxDataBuffer->data, rxBuffer + 1, RX_BUFFER_LEN - 1);
            rxDataBuffer->len = RX_BUFFER_LEN - 1;
          }
        }
        if (rxDataBuffer != NULL) {     // copy next byte to buffer
          rxDataBuffer->data[index-1] = out;  // -1 since control is omitted
          rxDataBuffer->len = index;
        }
      }
    }
  } while (status == EZSP_ASH_IN_PROGRESS);
  return status;
} // end of ashReadFrame()

// If enabled, exclusive-OR buffer data with a pseudo-random sequence
static void ashRandomizeBuffer(int8u *buffer, int8u len)
{
  if (ashReadConfig(randomize)) {
    (void)ashRandomizeArray(0, buffer, len);// zero inits the random sequence
  }
}

// Determines frame type from the control byte then validates its length.
// If invalid type or length, returns TYPE_INVALID.
static int16u ashFrameType(int16u control, int16u len)
{
  if (control == ASH_CONTROL_RSTACK) {
    if (len == ASH_FRAME_LEN_RSTACK) {  
      return TYPE_RSTACK;
    } 
  } else if (control == ASH_CONTROL_ERROR) {
    if (len == ASH_FRAME_LEN_ERROR) {
      return TYPE_ERROR;
    }
  } else if ( (control & ASH_DFRAME_MASK) == ASH_CONTROL_DATA) {
    if (len >= ASH_FRAME_LEN_DATA_MIN) {
      return TYPE_DATA;
    }
  } else if ( (control & ASH_SHFRAME_MASK) == ASH_CONTROL_ACK) {
    if (len == ASH_FRAME_LEN_ACK) {
      return TYPE_ACK;
    }
  } else if ( (control & ASH_SHFRAME_MASK) == ASH_CONTROL_NAK) {
    if (len == ASH_FRAME_LEN_NAK) {
      return TYPE_NAK;
    }
  } else {
    BUMP_HOST_COUNTER(rxBadControl);
    ashTraceEventRecdFrame("illegal control");
    return TYPE_INVALID;
  }
  BUMP_HOST_COUNTER(rxBadLength);
  ashTraceEventRecdFrame("illegal length");
  return TYPE_INVALID;
} // end of ashFrameType()


// Functions that read local variables for tracing
int8u readTxControl(void)   { return txControl;   };
int8u readRxControl(void)   { return rxControl;   };
int8u readAckRx(void)       { return ackRx;       };
int8u readAckTx(void)       { return ackTx;       };
int8u readFrmTx(void)       { return frmTx;       };
int8u readFrmReTx(void)     { return frmReTx;     };
int8u readFrmRx(void)       { return frmRx;       };
int8u readAshTimeouts(void) { return ashTimeouts; };
