# ESPAlarm
## Module to forward fire-, test-, desaster- and all-clear alarms from siren control to telegram chanel

**Version History**

- ESPAlarm V1.2  11/2018  1st official public Version

**Vision**

By being a member of voluntary fire department since my 14th birthday, I know verry well the problems of siren alarm. For exemple if the wind blows in a bad direction, you can't differ if the siren of your homebase, or the sirens of the villages around your homebase are alerting. Or for an other example you're simple out of earshot.
So I was looking to find a solution to get a additional alarm on the mobile phones of me and my comrades.

ESPAlarm, written for ESP8266-Module ESP-12F on [Arduino IDE](https://www.arduino.cc/en/Main/Software) allows to forward any kind of siren alarm, either for civil usage (e.g. desaster alarm) or fire alarm to a [telegram channel](https://telegram.org/).


**Telegram as alarm platform**

Telegram offers you the opportunity to generate [Bots](https://core.telegram.org/bots/faq) and [Chanels](https://telegram.org/faq_channels).
A bot is a kind of telegram-user which can be accessed and controled via [telegram bot-API](https://core.telegram.org/bots). In my usecase, the software of the bot (e.g. on which key words it reacts or what its answers are) is settled in the software of ESPAlarm and controls the "Bot-User" via Bot-API. This bot is, beside me, admin of a telegram chanel and sends the alarm messages straight to it. All comerades of our fire department are allowed to read this private chanel and will be informed if any alarm occures.


**Detection of alarm-types**

There are four different types of alarm:
- Firealarm (1 min continuous tone with two interrupts)
- Testalarm (same timing as firealarm, every 1st Saturday each month, except public holyday => testalarm will be on following saturday)
- Desasterslarm (1 min wail tone)
- All-Clear (1 min continuous tone)

![Siren timing](/images/siren_timing.jpg)

ESPAlarm is connected via a potential-free auxilary contractor, directly to the siren motor control. The detection of the alarm type will be performed by a kind of time based "state machine" and several software flags. In case of alarm, there are three sequentially time loops. The first one checks, if the alarm signal form motor control lasts longer than 0,5s (Debouncing of auxilary contractor, elimination of interferences on signal line). The second one checks if the signal is still high after 3s. If so, and no desaster-alarm-flag is set, the alarm type must be a firealarm and the corresponding message will be send. If not, the alarm must be a desaster alarm, the alarm message will be sent and a desaster-alarm-flag will be set. The third time loop only acts if this flag is set and the signal is still high after 3s. In this case, the alarm can either be a firealarm or a all-clear alarm. ESPAlarm checks after 15s again the level of the signal. If level is still high, a all-clear alarm with the related message will be triggered, if not the alarm type must be a firealarm.

![Telegram Chanel](/images/channel.jpg)

Every fist saturday each month, between 13:00 and 13:30, will be a testalarm. But there is a tricky exeption... If this first saturday is a public holiday, the testalarm will be shifted one week. To check and compare the time and date of an actual occurring alarm, ESPAlarm synchronisizes to acutal time by usage of an [NTP-Server](https://www.pool.ntp.org/zone/@). The timing of an testalarm is the same as a firealarm.

So if a firealarm occures, ESPAlarm first checks if today is saturday and a public holiday. Checking the weekday is very easy. The Time-Library of Arduino IDE provides the weekday as integer. To check if today is a public holiday, the actual date (excluding year) will be compared to a look up table, containing all "weekday-variable" public holidays. If its a public holiday, a public-holiday-flag will be set.

After that, ESPAlarm checks if a alarm occures on a saturday between 13:00 and 13:30. If so and day in month is before the 8th and no public-holiday-flag is set, it's a testalarm. Otherwise, a firealarm will be sent.
On the other hand, if the alarm occures on a saturday at the same time, the public-holiday-flag is set and day in month is in between 8th and 14th, the shifted testalarm will be sent.

**The Telegram Bot behind**

As already mentioned the main acting part of ESPAlarm is the API control of a telegram bot. This bot sends all messages trough the telegram channel. 
The API control also enables the opportunity to control and comunicate with ESPAlarm. Therefore the following instruction set is given:

- /start    <tab id=t1>starts the bot
- /reset    <tab id=t1>perform a software reset of ESPAlarm
- /info     <tab id=t1>shows the actual status of ESPAlarm
- /time     <tab id=t1>shows the actual runtime of ESPAlarm since last reset
- /help     <tab id=t1>shows the instruction set
- /alets    <tab id=t1>shows the amount of already sent alarms
- /alarmme  <tab id=t1>enables/diables the alarm via telegram channel
- /delete   <tab id=t1>resets the alarm-counter

For easier usage of the bot, a custom keyboard for the major instructions is also implemented. Critical instructions, like /delete or /reset must be confirmed by pressing "yes/no"-Buttons on a given inline keyboard.

![Telegram Bot](/images/bot.jpg)
