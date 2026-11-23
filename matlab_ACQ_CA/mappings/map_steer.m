function [cmd, state] = map_steer(values, settings, state, KEY)
%MAP_STEER  A0 = steer left, A1 = steer right, both together = go (FWD).
%   [cmd, state] = map_steer(values, settings, state, KEY)   (signature: see map_go_brake.m)
%
%   Car behaviour (STM32 demo_full_PC_car_ctrl): LEFT/RIGHT held = wheels turn further, released =
%   wheels ease back to centre; FWD held = speed up, released = keep speed (cruise).
%   There is no EMG brake in this mapping: slow down or stop with the E-STOP button / spacebar.

a0 = values(1) > settings.level;
a1 = values(2) > settings.level;

if a0 && a1
    cmd = KEY.FWD;
elseif a0
    cmd = KEY.LEFT;
elseif a1
    cmd = KEY.RIGHT;
else
    cmd = 0;
end
cmd = uint8(cmd);
end
