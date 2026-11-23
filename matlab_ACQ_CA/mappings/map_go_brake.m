function [cmd, state] = map_go_brake(values, settings, state, KEY)
%MAP_GO_BRAKE  A0 = go (FWD), A1 = brake (SLOW). Steering stays centred.
%   [cmd, state] = map_go_brake(values, settings, state, KEY)
%
%   Every mapping in this folder has this same signature, so run_emg_car can swap them:
%     values   : [A0 A1] controller outputs, 0..1
%     settings : .level = activation level; a channel is "active" above it (slider in the drive window)
%     state    : struct the mapping may keep between calls (starts as struct()); returned updated
%     KEY      : command bits (KEY.FWD, KEY.SLOW, KEY.LEFT, KEY.RIGHT, KEY.ESTOP)
%     cmd      : uint8 bitmask sent to the car this tick (called every 50 ms)
%
%   Car behaviour (STM32 demo_full_PC_car_ctrl): FWD held = speed up, nothing = keep speed (cruise),
%   SLOW held = slow down to 0. Both active = SLOW (braking wins, as it does on the STM32).

a0 = values(1) > settings.level;
a1 = values(2) > settings.level;

if a1
    cmd = KEY.SLOW;
elseif a0
    cmd = KEY.FWD;
else
    cmd = 0;
end
cmd = uint8(cmd);
end
