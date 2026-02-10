#!/bin/bash
if [ -d "/Applications/Flow.app" ] || pgrep -x "Flow-service" >/dev/null; then
  exit 0
fi
exit 1
