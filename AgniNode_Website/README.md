# AgniNode website and dashboard

Static site (HTML, CSS, JavaScript) on Firebase Hosting, with Firebase Auth and Firestore. No build step.

public/index.html product page (SEO, JSON-LD, Open Graph)
public/login.html Firebase email/password login
public/dashboard.html protected dashboard (redirects to login when signed out)
public/js/config.js PUT YOUR FIREBASE CONFIG HERE (demo mode until you do)
firestore.rules role-based security (viewer, operator, admin, device)

Setup: 1) edit `js/config.js` 2) enable Email/Password in Firebase Auth 3) create
`users/{uid}` documents with a `role` field 4) replace `YOUR-DOMAIN.com` in `index.html`,
`robots.txt`, and `sitemap.xml` 5) add `public/og-image.png` (1200x630) 6) deploy with
Firebase CLI.

Firestore data:

- `devices/{id}` and `devices/{id}/status/live`: current gateway/STM32 state and telemetry.
- `devices/{id}/readings/{readingId}`: time-series telemetry history.
- `devices/{id}/alerts/{alertId}`: persisted STM32 alert events shown in the dashboard.
- `devices/{id}/commands/{commandId}`: operator/admin commands; the dashboard currently exposes buzzer on/off only.
- `config/thresholds` and `users/{uid}`: dashboard/demo settings and role records.

The ESP32 gateway authenticates as a Firebase user with role `device`. That user's
`users/{uid}.deviceId` must exactly match the gateway's local `DEVICE_ID`; Firestore rules bind
device reads and writes to that ID. It updates the root device document and live-status document,
and appends history records. The website listens to the root device documents and reads history
from `readings`.

The dashboard's live electrical analysis shows only STM32-reported measurements. The firmware's
nominal residual-current trip threshold is 300 mA; two CTs and this threshold do not constitute a
certified RCCB. The Vbus monitor is not connected, so firmware inhibits automatic mains close.
The scope, fault simulator, and predictive displays are demo-only and hidden for configured live
Firebase mode. The dashboard does not stream raw waveform samples or change firmware safety
thresholds.
