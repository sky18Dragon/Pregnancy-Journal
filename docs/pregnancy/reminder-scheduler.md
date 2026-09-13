# Reminder and scheduler

Apps never program RTC wake sources directly. At sleep preparation the system
coordinator collects four independent candidates:

- daily pregnancy refresh at 03:00;
- current app's requested wake;
- earliest uncompleted reminder;
- earliest uncompleted checkup.

`StickyScheduler` stores typed events and selects the closest future epoch.
The system arms one ESP32 timer wake 15 seconds early while preserving top-button
wake. Separate event IDs prevent one source from replacing another.

On a timer boot, the coordinator scans uncompleted reminders and checkups in a
small due-time window. A matching event starts the existing repeating buzzer;
button or touch interaction stops it. On the next sleep, candidates are
recomputed from persisted data, so completing, deleting or rolling a recurring
reminder changes the next wake without app-level RTC ownership.

Reminder records are independent checksummed NVS slots. Daily and weekly
recurrence are implemented; complex recurrence editing is deferred.
