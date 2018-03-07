#!/bin/bash
PID=$(pidof "TBSVideoDeal_cgi")
echo TBSVideoDeal_cgi pid = $PID
kill $PID
