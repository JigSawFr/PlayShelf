# Home Assistant and the Boréale agent

PlayShelf can tell [Home Assistant](https://www.home-assistant.io/) what it is doing: the downloads left, the one in progress, the SD card's free space. It does it through the **Boréale agent**, a small program that runs in the background on the console and keeps one connection to your MQTT broker for every Boréale homebrew (PlayGuard and PlayShelf today). Each homebrew adds its own information and orders to it; the agent publishes the console's own by itself.

You need an MQTT broker (the Mosquitto add-on of Home Assistant will do), the [Boréale integration](https://github.com/JigSawFr/boreale-ha) in Home Assistant, and Atmosphère on the console (the agent is a sysmodule).

## What shows up in Home Assistant

- **From the agent alone**, with no homebrew open: the console's hardware, power, network and storage, whether it is online and when it was last seen, and the notifications of the Boréale homebrews.
- **With PlayShelf**: *PlayShelf downloads left*, *PlayShelf download progress*, *PlayShelf last installed*, *PlayShelf SD card free space*, *PlayShelf downloading* and *PlayShelf download problem*, and the button *Pause PlayShelf downloads*. They appear while PlayShelf is open and go unavailable when it closes. The button pauses the queue; to resume, use the console.
- **With PlayGuard**: the parental controls, the play time, the games and the orders from Home Assistant, as its own documentation says.

## Set it up from PlayShelf

1. **Preferences › Home Assistant link**: turn on *Connect to the broker* and give the broker's address and port, *Secure connection (TLS)* if it uses one, and the user and the password it expects. These settings are shared: PlayGuard and the agent read the same file (`sd:/config/boreale/link.conf`), so you set them once.
2. **Preferences › Boréale agent**: choose **Install**. PlayShelf carries a copy of the agent, puts it on the SD card, starts it and checks that it answers; if the new one does not answer, the previous one is put back. **Start with the console** keeps it running after a restart.
3. In Home Assistant, add the Boréale integration: it finds the console on the broker by itself.

The same screen later offers **Update** when PlayShelf carries a newer agent, **Stop now** or **Start now**, and **Remove**. Stopping or updating makes the agent leave the broker cleanly, so Home Assistant sees the console go offline instead of waiting for a timeout.

## Publish PlayShelf's state

*Preferences › Publish PlayShelf's state (Boréale agent)* decides whether PlayShelf talks to the agent at all. Off, it stays silent: no downloads, no free space, no button, and its entities go unavailable. The agent and the other homebrews are not affected.

## Good to know

- What the agent and PlayShelf publish goes only to *your* broker.
- The broker's password is kept in `link.conf` on the SD card as plain text, like the other homebrews' settings. Use a user that can only reach this console's topics, and TLS when the broker allows it.
- The agent is installed by PlayShelf or by PlayGuard, whichever you use first; both manage the same one.
