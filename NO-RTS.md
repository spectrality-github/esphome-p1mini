# The RTS signal
p1mini uses the RTS signal to request updates from the meter at the desired intervals. Once an update has been received, RTS is set low while the data is processed, which insures that the meter will not send new updates before the p1mini is ready to receive.

Another advantage is that some meters can send updates more frequently when prompted by the RTS signal than they do when RTS is left always high.

## RTS always high
If the RTS signal is always high, the meter will send updates at some interval which is specific to that meter (1 second, 5 seconds and 10 seconds have been observerd). The meter will send the update regardless if the p1mini is ready to receive or not (but a typical p1mini can receive and process more than two updates per second, so usually not a problem).

Generally, the RTS signal is a good thing, but there are some cases when it is better to set it high all the time: If the meter works unreliably with the RTS signal or if your p1mini was build without attaching the RTS signal to a GPIO pin.

### Meters that do not work well with RTS
If you have any of these and are having problems, you should try disabling RTS.

* S34U18 (Sanxing SX631) / Vattenfall 

### RTS not attached to a GPIO
If you built your ESP based on the original esphome-p1reader or if you bought a Slimmeleser etc which does not connect the RTS to a GPIO, it is better to disable the RTS functionality in the p1mini code.

## Disabling the RTS signal
By changing this, the RTS signal (if connected) will be set constantly high, and the p1mini will work slightly differently internally.

In the yaml file, find this line:

```
id(p1_period),
```

and replace it with this:

```
nullptr,
```

This also means that the number template `p1_period` is not used, so it could be removed or set to internal.
