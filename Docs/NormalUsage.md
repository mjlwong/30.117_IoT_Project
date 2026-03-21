# Normal Usage

## Operation

The drybox sensor will periodically send the measured humidity and temperature from the SHT31 sensor. The user may also view the humidity and temperature against time on the app. 

When the humidity exceeds a threshold (default is 40%), a timer (defualt 30min) will start. When the timer is up and the humidity is still above the threshold, the device will warn the user on the app to replace the silica gel in the drybox. They can then pres a button on the app to indicate that the silica gel has been replaced, resetting the alert. 

The humidity threshold and timer can be changed via the configuration settings in this project. [Click here](Configuring.md) for more information. 