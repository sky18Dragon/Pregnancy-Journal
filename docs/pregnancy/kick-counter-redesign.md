# Fetal-movement counter redesign

## Product decision

This feature is a private logging aid, not a fetal-health classifier. It helps a
user capture movements as they notice them, review today's records and compare
their own seven-day logging pattern. It does not label a day normal or abnormal,
set a universal target, extrapolate a 12-hour count or advise waiting for a timer
to finish before seeking care.

The persistent safety message is: if the baby moves less than usual, stops
moving or the usual pattern changes, contact maternity care immediately. The
wording deliberately does not defer action until the next day or until a
counting session ends.

## Evidence used

- [NHS: Your baby's movements](https://www.nhs.uk/pregnancy/keeping-well/your-babys-movements/)
  says there is no set number of normal movements, recommends learning the
  baby's usual pattern, and advises immediate contact for reduced, stopped or
  changed movement.
- [RCOG: Your baby's movements in pregnancy](https://www.rcog.org.uk/for-the-public/browse-our-patient-information/your-babys-movements-in-pregnancy/)
  likewise prioritizes awareness of the individual's usual pattern and notes
  that evidence does not support routine movement charts for everyone.
- [RCOG Green-top Guideline No. 57](https://www.rcog.org.uk/guidance/browse-all-guidance/green-top-guidelines/reduced-fetal-movements-green-top-guideline-no-57/)
  is the current professional guideline hub for reduced fetal movements.
- [WHO antenatal care recommendations](https://www.who.int/publications/i/item/9789241549912)
  place routine daily count-to-ten charts in a research context while stressing
  awareness and reporting of reduced movement.
- [ACOG: Special Tests for Monitoring Fetal Well-Being](https://www.acog.org/womens-health/faqs/special-tests-for-monitoring-fetal-well-being)
  explains that multiple counting methods exist and that the user's clinician
  should specify the method and when to call.

## Information architecture

### Today

- Today's recorded movement total is the single focal number.
- Session count, morning/afternoon/evening totals and the latest saved session
  provide context without implying a universal schedule.
- “Start counting” is the primary action; “View 7-day trend” opens analysis.
- A high-contrast safety panel remains visible above the actions.

### Seven-day trend

- Seven consecutive calendar days are shown, including zero-record days.
- Bars represent recorded movements, not inferred fetal activity.
- Supporting values show recorded days, sessions, average per recorded day,
  most-recorded period and today's time-of-day split.
- The page is labeled “records · not a diagnosis.”

### Active counter

- One large central target records one distinct movement.
- A continuous series may be recorded as one movement, while the exact method
  remains something the user should agree with their care team.
- Undo corrects accidental input. A 400 ms guard rejects immediate duplicate
  taps without imposing the former five-second under-counting window.
- The counter never auto-finishes at ten. Finish stores a non-empty session;
  the 60-minute cap remains a data-integrity boundary.

## E-paper and accessibility constraints

The layouts are purpose-built for both 800×480 landscape and 480×800 portrait.
They use only black and white, no animation, one primary focal point per page,
and 56 px bottom actions (above the 48 px minimum touch target). Text and chart
labels are rendered from committed native-size font strikes so firmware output
does not depend on a runtime font engine.
