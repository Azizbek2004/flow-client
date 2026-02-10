#!/bin/bash
if [ -d "/Applications/Flow.app" ] || pgrep -x "Flow-service" >/dev/null; then
  exit 1
fi
exit 0
