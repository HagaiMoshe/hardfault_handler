# hardfault_handler
A cortexM4 hardfault handler for debugging crashes without access to debugger.

The perpose of this handler is to save not only the core registers, but also the stack of the violating context.
This is crucial for debugging both hard faults that occure on remote devices and hard faults that are hard to recreate.

Once a hardfault occurs, the handler saves the core registers and the context's stack to a persistent memory.
After reboot the saved data can be read, then you and store it to log for later, send it to a remote server or do with it whatever else you fancy.

Note: several device specific methods will need to be implemented<br>
