# ESPAlarm
## Module to forward fire-, test-, desaster- and all-clear alarms from siren control to telegram chanel

**Version History**

- ESPAlarm V1.2  11/2018  1st official public Version

**Vision**

By being a member of voluntary fire department since my 14th birthday, I know verry well the problems of siren alarm. For exemple if the wind blows in a bad direction, you can't differ if the siren of your homebase, or the sirens of the villages around your homebase are alerting. Or simple you are out of earshot.
So I was looking to find a solution to get a additional alarm on the mobile phones of me and my comrades.

ESPAlarm, written for ESP8266-Module ESP-12F on [Arduino IDE](https://www.arduino.cc/en/Main/Software) allows to forward any kind of siren alarm, either for civil usage (e.g. desaster alarm) or fire alarm to a [telegram channel](https://telegram.org/).


**Telegram as alarm platform**

Telegram offers you the opportunity to generate [Bots](https://core.telegram.org/bots/faq) and [Chanels](https://telegram.org/faq_channels).
A bot is a kind of telegram-user which can be accessed and controled via [telegram bot-API](https://core.telegram.org/bots). In my usecase, the software of the bot (e.g. on which key words he reacts or what his answers are) is settled in the software of ESPAlarm and controls the "Bot-User" via Bot-API. This bot is, beside me, admin of a telegram chanel and sends the alarm messages straight to it. All comerades of our fire department are allowed to read this private chanel and will be informed if any alarm occures.


**Detection of alarm-types**


