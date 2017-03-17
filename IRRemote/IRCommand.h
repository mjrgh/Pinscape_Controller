// IR Command Descriptor
//
// This is a common representation for command codes across all of our
// supported IR protocols.  A "command code" generally maps to the data
// sent for one key press on a remote control.  The common format contains:
//
// - The protocol ID.  This represents the mapping between data bits
//   and the physical IR signals.  Each protocol has its own rules for
//   how the individual bits are represented and how the bits making up
//   a single command are arranged into a "command word" unit for
//   transmission.
//
// - An integer with the unique key code.  For most protocols, this is
//   simply the sequence of bits sent by the remote.  If the protocol has
//   the notion of a "toggle bit", we pull that out into the separate toggle
//   bit field (below), and set the bit position in the key code field to
//   zero so that the same key always yields the same code.  If the protocol
//   has a published spec with a defined bit ordering (LSB first or MSB 
//   first), we'll use the same bit ordering to construct the key code
//   value, otherwise the bit order is arbitrary.  We use the canonical bit
//   order when one exists to make it more likely that our codes will match
//   those in published tables for commercial remotes using the protocol.
//   We'll also try to use the same treatment as published tables for any
//   meaningless structural bits, such as start and stop bits, again so that
//   our codes will more closely match published codes.
//
// - A "toggle bit".  Some protocols have the notion of a toggle bit,
//   which is a bit that gets flipped on each key press, but stays the
//   same when the same code is sent repeatedly while the user is holding
//   down a key.  This lets the receiver distinguish two distinct key
//   presses from one long key press, which is important for keys like
//   "power toggle" and "5".
//
// - A "ditto bit".  Some protocols use a special code to indicate an
//   auto-repeat.  Like the toggle bit, this serves to distinguish
//   repeatedly pressing the same key from holding the key down.  Ditto
//   codes in most protocols that use them don't contain any data bits,
//   so the key code will usually be zero if the ditto bit is set.
//
// Note that most of the published protocols have more internal structure
// than this to the bit stream.  E.g., there's often an "address" field of
// some kind that specifies the type of device the code is for (TV, DVD,
// etc), and a "command" field with a key code scoped to the device type.
// We don't try to break down the codes into subfields like this.  (With
// the exception of "toggle" bits, which get special treatment because 
// they'd otherwise make each key look like it had two separate codes.)
//

#ifndef _IRCOMMAND_H_
#define _IRCOMMAND_H_

#include <mbed.h>
#include "IRProtocolID.h"

// Three-state logic for reporting dittos and toggle bits.  "Null"
// means that the bit isn't used at all, which isn't quite the same
// as false.
class bool3
{
public:
    enum val { null3, false3, true3 } __attribute__ ((packed));
    
    static const bool3 null;

    bool3() : v(null3) { }
    bool3(val v) : v(v) { }
    bool3(bool v) : v(v ? true3 : false3) { }
    bool3(int v) : v(v ? true3 : false3) { }
    
    operator int() { return v == true3; }
    operator bool() { return v == true3; }
    
    bool isNull() const { return v == null3; }
        
private:
    const val v;
} __attribute__ ((packed));
    

struct IRCommand
{
    IRCommand() 
    { 
        proId = IRPRO_NONE;
        code = 0;
        toggle = false;
        ditto = false;
    }

    IRCommand(uint8_t proId, uint64_t code, bool3 toggle, bool3 ditto)
    {
        this->proId = proId;
        this->code = code;
        this->toggle = bool(toggle);
        this->hasToggle = !toggle.isNull();
        this->ditto = bool(ditto);
        this->hasDittos = !ditto.isNull();
    }

    // 64-bit command code, containing the decoded bits of the command.
    // The bits are arranged in LSB-first or MSB-first order, relative
    // to the order of IR transmission, according to the conventions of
    // the protocol.  This includes all bits from the transmission,
    // including things like error detection bits, except for meaningless
    // fixed structural elements like header marks, start bits, and stop
    // bits.  
    //
    // If there's a "toggle" bit in the code, its bit position in 'code' 
    // is ALWAYS set to zero, but we store the actual bit value in 'toggle'.
    // This ensures that clients who don't care about toggle bits will see
    // code value every time for a given key press, while still preserving
    // the toggle bit information for clients who can use it.
    //
    // See the individual protocol encoder/decoder classes for the exact 
    // mapping between the serial bit stream and the 64-bit code value here.
    uint64_t code;
    
    // Protocol ID - a PRO_xxx value
    uint8_t proId;
    
    // Toggle bit.  Some protocols have a "toggle bit", which the sender
    // flips each time a new key is pressed.  This allows receivers to
    // distinguish auto-repeat from separate key presses.  For protocols
    // that define a toggle bit, we'll store the bit here, and set the
    // bit position in 'code' to zero.  That way, the client always sees
    // the same code for every key press, regardless of the toggle state,
    // but callers who want to make use of the toggle bit can still get
    // at the transmitted value by inspecting this field.
    uint8_t toggle : 1;
    
    // Does the protocol use toggle bits?  This is a fixed feature of 
    // the protocol, so it doesn't tell us whether the sender is
    // actually using toggles properly, only that the bit exists in
    // the protocol.  If you want to determine if the sender is using
    // toggles for learning remote purposes, ask the user to press the
    // same key several times in a row, and observe if the reported
    // toggle bits alternate.
    uint8_t hasToggle : 1;
    
    // Ditto bit.  Some protocols send a distinct code to indicate auto-
    // repeat when a key is held down.  These protocols will send the 
    // normal code for the key first, then send the special "ditto" code
    // repeatedly as long as the key is held down.  If this bit is set,
    // the command represents one of these auto-repeat messages.  Ditto
    // codes usually don't have any data bits, so the 'code' value will
    // usually be zero if this is set.
    uint8_t ditto : 1;
    
    // Does the protocol have a ditto format?  This only indicates if
    // the protocol has a defined ditto format, not if the sender is
    // actually using it.  If you want to determine if the sender uses
    // dittos for learning remote purposes, ask the user to hold a key
    // down long enough to repeat, and observe the reported codes to
    // see if the ditto bit is set after the first repeat.
    uint8_t hasDittos : 1;
    
} __attribute__ ((packed));

#endif
