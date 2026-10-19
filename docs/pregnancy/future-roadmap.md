# Delivery roadmap

Updated: 2026-10-10. The current six-app implementation is documented in
[current-functionality.md](current-functionality.md). The stages below are the
confirmed delivery plan; they do not mean the firmware has already changed.
V1/V2 are delivery stages, not firmware version numbers.

## V1: four offline apps

- Weight: manual records, 7/30-calendar-day trends, basic history editing,
  same-day replacement confirmation and the existing 366-record target.
- Kicks: counting sessions, today's summary and seven-day recorded-data trends;
  migrate new persistence and validated old sessions to the shared SD repository.
- Settings: language, time, pregnancy setup, display cleanup and card/data management.
- Baby Week: pregnancy setup, week/progress dashboard and offline Baby/Mom content.

Shared MicroSD storage/recovery, legacy data protection, sleep/wake,
landscape/portrait input and e-paper regression remain V1 delivery gates.
The registry, dashboard shortcuts and business alert scheduling must be reduced
to this scope. Existing Reminder/Checkup records remain preserved.

## V2: reminders and voice workflow

- Reminders, Checkups and business alerts.
- Habit rules and daily instances.
- Notes and reusable offline Chinese text editing.
- Recording, local category suggestions and confirmation.
- Inbox, field review, idempotent record creation and audio cleanup.
- Wi-Fi provisioning, cloud consent, transcription gateway and Coze speech-to-text.
- Voice quality/classification, network/privacy, latency and power acceptance.

Keep all four V1 apps and stable persisted IDs. Reuse the completed S0 prototypes
and S1/S2 storage foundations; deferred work is not marked complete.

## Later independent proposals

AI summary, Wi-Fi content sync, OTA, family messages, cloud backup, mobile
configuration beyond V2 provisioning, reports and a partner app remain separate
proposals, not newly required V1/V2 deliverables.

See [the current specification](../PROJECT-SPEC.md),
[active tasks and historical evidence](../TODO.md) and
[the V2 specification](../V2-PROJECT-SPEC.md). Future implementation preserves the
apps -> domain services -> Sticky Core -> device HAL dependency direction.
