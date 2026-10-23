# Compiler CI resource bounds

Host and device Make builds use two concurrent compile jobs rather than the
runner CPU count. Template-heavy formatting tests can consume substantially more
memory per compiler process than the older implementation. Stale runs of the same
workflow/PR are cancelled when a new revision arrives.

This bounds peak demand and avoids spending hosted runner time on superseded
revisions. Builds may take longer; compiler coverage and executed checks are not
reduced. A cancelled run is not a passing compiler result.
