function [cmd, state] = map_prop_go_brake(values, settings, state, KEY)
%MAP_PROP_GO_BRAKE  Proportional go/brake: contraction strength sets how fast the speed changes.
%   [cmd, state] = map_prop_go_brake(values, settings, state, KEY)   (signature: see map_go_brake.m)
%
%   Same channels as map_go_brake (A0 = go, A1 = brake), but the bit is sent on a share of the 50 ms
%   ticks equal to the control value. The STM32 changes speed by one step per tick while the bit is
%   set, so a value of 0.5 accelerates at half rate, and 1.0 at full rate. No STM32 changes needed.
%
%   How: an accumulator per bit adds the value every tick and sends the bit when it reaches 1.
%   Values at or below settings.level count as 0. Braking wins when both channels are active.

if ~isfield(state, 'acc')
    state.acc = [0 0];          % [FWD, SLOW]
end

v = values(:).';
v(v <= settings.level) = 0;

cmd = 0;
if v(2) > 0                     % brake
    state.acc(1) = 0;
    state.acc(2) = state.acc(2) + v(2);
    if state.acc(2) >= 1
        cmd = KEY.SLOW;
        state.acc(2) = state.acc(2) - 1;
    end
elseif v(1) > 0                 % go
    state.acc(2) = 0;
    state.acc(1) = state.acc(1) + v(1);
    if state.acc(1) >= 1
        cmd = KEY.FWD;
        state.acc(1) = state.acc(1) - 1;
    end
else
    state.acc = [0 0];          % relaxed: start fresh next time
end
cmd = uint8(cmd);
end
