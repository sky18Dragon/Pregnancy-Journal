# Security Policy

## Supported versions

Security fixes are applied to the latest source on the default branch. Release notes identify published firmware versions.

## Reporting a vulnerability

Please use GitHub's private vulnerability reporting feature for this repository. Include:

- the affected firmware version or commit;
- the hardware and build profile;
- clear reproduction steps;
- the practical impact;
- any suggested mitigation.

You can expect an initial acknowledgement within seven days. Confirmed issues will be coordinated privately until a fix and disclosure plan are ready.

## Sensitive data

Sticky Bunny does not require cloud credentials for its built-in applications. Development credentials and local environment files belong in `.env`, which is excluded from version control.
