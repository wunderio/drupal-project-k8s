#!/bin/bash
t="/tmp/ssh_${USER}_${SSH_CLIENT%% *}_$(date +%FT%TS%4N)"
if [[ $SSH_ORIGINAL_COMMAND ]]; then
    eval "$SSH_ORIGINAL_COMMAND"
    exit
fi
script -qfe -t$t.tim $t.out
