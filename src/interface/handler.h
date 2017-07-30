#ifndef BITCOIN_INTERFACE_HANDLER_H
#define BITCOIN_INTERFACE_HANDLER_H

namespace interface {

//! Generic interface for managing an event handler or callback function
//! registered with another interface. Has a single disconnect method to cancel
//! the registration and prevent any future notifications.
class Handler
{
public:
    virtual ~Handler() {}

    //! Disconnect the handler.
    virtual void disconnect() = 0;
};

} // namespace interface

#endif // BITCOIN_INTERFACE_HANDLER_H
