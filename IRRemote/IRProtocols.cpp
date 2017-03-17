// IR Protocol handlers

#include "IRCommand.h"
#include "IRReceiver.h"
#include "IRProtocols.h"

// -------------------------------------------------------------------------
// 
// IRProtocol base class implementation
//
CircBuf<DebugItem,256> debug;

// Look up a protocol by ID
IRProtocol *IRProtocol::senderForId(int id)
{
    // try each protocol singleton in the sender list
    #define IR_PROTOCOL_TX(className) \
        if (protocols->s_##className.isSenderFor(id)) \
            return &protocols->s_##className;
    #include "IRProtocolList.h"
    
    // not found    
    return 0;
}

// report code with a specific protocol
void IRProtocol::reportCode(
    IRRecvProIfc *receiver, int pro, uint64_t code, bool3 toggle, bool3 ditto)
{
    IRCommand cmd(pro, code, toggle, ditto);
    receiver->writeCommand(cmd);
}

// protocol handler singletons
IRProtocols *IRProtocol::protocols;

// allocate the protocol singletons
void IRProtocol::allocProtocols()
{
    if (protocols == 0)
        protocols = new IRProtocols();
}

// -------------------------------------------------------------------------
//
// Kaseikyo class implementation.
//

// OEM <-> subprotocol map
const IRPKaseikyo::OEMMap IRPKaseikyo::oemMap[] = {
    { 0x0000, IRPRO_KASEIKYO48, 48 },
    { 0x0000, IRPRO_KASEIKYO56, 56 },
    { 0x5432, IRPRO_DENONK, 48 },
    { 0x1463, IRPRO_FUJITSU48, 48 },
    { 0x1463, IRPRO_FUJITSU56, 56 },
    { 0x0301, IRPRO_JVC48, 48 },
    { 0x0301, IRPRO_JVC56, 56 },
    { 0x23CB, IRPRO_MITSUBISHIK, 48 },
    { 0x0220, IRPRO_PANASONIC48, 48 },
    { 0x0220, IRPRO_PANASONIC56, 56 },
    { 0xAA5A, IRPRO_SHARPK, 48 },
    { 0x4353, IRPRO_TEACK, 48 }
};
const int IRPKaseikyo::nOemMap = sizeof(oemMap)/sizeof(oemMap[0]);

