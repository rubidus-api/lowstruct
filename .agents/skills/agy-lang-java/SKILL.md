---
name: agy-lang-java
description: Use when changing, building, or verifying a non-Android Java project here — server applications, desktop applications, or libraries — including Gradle/Maven questions, JDK and toolchain selection, and how to verify a change without launching a full production server.
---

# Java work

## Build

Determine whether the project uses Gradle, Maven, or custom scripts, and prefer
its wrapper when one exists. Some projects here pin a specific local JDK under a
`toolchains/` directory — use that exact path rather than whatever `java` resolves
to, and check `JAVA_HOME` before blaming the code for a build failure.

## Verification

For a server application, verify the affected layer (unit or integration test for
that handler/service) rather than launching a long-running production-style server
to look at it. For a desktop application, compilation may not cover UI behavior —
use available tests, and when none exist, report what was left unverified instead
of implying it works.

## Design

Do not introduce a framework or a large dependency for something the existing
architecture already expresses.
