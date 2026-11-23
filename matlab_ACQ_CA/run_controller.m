function value = run_controller(name, buffer, ch, calibration, sliders)
%RUN_CONTROLLER  Call one of Kirby's controllers with the same inputs, whichever it is.
%   value = run_controller(name, buffer, ch, calibration, sliders)
%   run_controller("reset")            % clear every controller's memory (start of a drive session)
%
%   name        : "control01" | "control03b" | "control04" (files in kirby_matlab_code/, unmodified)
%   buffer      : Kirby's buffer struct for this channel: .data (column of volts), .ptr (next index)
%   ch          : 1 (A0) or 2 (A1)
%   calibration : struct from emg_calibrate
%   sliders     : struct with .sensitivity, .speed, .baseline (1x2 each, Kirby's control sliders)
%   value       : 0..1 (control04: 0 or 1)
%
%   To add a controller: put its file in kirby_matlab_code/ (or matlab_ACQ_CA/), add a "case" below,
%   and add its name to CONTROLLERS at the top of run_emg_car.m.
%
%   Not offered: control02 and control03 keep one `persistent` state shared by both channels, so with
%   two muscles each channel corrupts the other's. control03b is the per-channel version of control03.

KNOWN = ["control01", "control03b", "control04"];

if name == "reset"
    for k = 1:numel(KNOWN)
        clear(char(KNOWN(k)));   % clearing a function resets its persistent variables
    end
    value = [];
    return
end

window = calibration.window;
threshold = calibration.threshold(ch);
rms_rest = calibration.rest_rms(ch);
rms_fist = calibration.mean_contr_rms(ch);

switch name
    case "control01"
        % Proportional: 0 below threshold, else (rms - rest)/(fist - rest), clamped 0..1
        value = control01(buffer, window, threshold, rms_rest, rms_fist);

    case "control03b"
        % Smoothed proportional with Kirby's per-channel sliders
        value = control03b(buffer, window, threshold, rms_rest, rms_fist, ch, ...
            sliders.sensitivity(ch), sliders.speed(ch), sliders.baseline(ch));

    case "control04"
        % Binary, with the Sensitivity slider scaling the threshold (Kirby's commented-out Section 2 code)
        value = control04(buffer, window, threshold * sliders.sensitivity(ch), rms_rest, rms_fist);

    otherwise
        error('run_controller:unknown', 'Unknown controller "%s". Add a case in run_controller.m.', name);
end

assert(isscalar(value) && isreal(value) && isfinite(value), 'Invalid control output from Channel %d.', ch);
end
