function [samples, live] = capture_emg(uno, seconds, live, instruction, stopOnClose)
%CAPTURE_EMG  Record both EMG channels for a fixed time while plotting them live.
%   [samples, live] = capture_emg(uno, seconds, live, instruction)
%   [samples, live] = capture_emg(uno, Inf, live, instruction, true)   % view until the window closes
%
%   From Kirby's final_project-2channel-original.m (local function captureDualEMG). The line parsing
%   is now parse_emg_chunk.m, and the count->volt conversion is counts_to_volts.m.
%
%   uno         : open serialport to the Uno (full_2_channel_wireless_ctrl_alg.ino, 250000 baud)
%   seconds     : how long to record (Inf = until the plot window is closed; needs stopOnClose)
%   live        : struct from live_emg_plot (updated and returned so the time axis continues)
%   instruction : text shown above the plot ("CONTRACT", "RELAX", ...)
%   stopOnClose : false (default) = closing the window is an error (stops a calibration);
%                 true = closing the window just ends the capture
%   samples     : N x 2 volts [Channel 1 (A0), Channel 2 (A1)]

if nargin < 5
    stopOnClose = false;
end

% Update instruction above live plot
title(live.ax, instruction);
drawnow;
flush(uno);
samples = zeros(ceil(min(seconds, 60) * 1200), 2);
n = 0;
leftover = '';
timer = tic;

while toc(timer) < seconds
    if ~isgraphics(live.fig)
        if stopOnClose
            break
        end
        error('emg:captureStopped', 'Calibration stopped: plot window closed.');
    end
    count = uno.NumBytesAvailable;
    if count > 0
        [counts, leftover] = parse_emg_chunk([leftover, read(uno, count, "char")]);
        nb = size(counts, 1);
        if nb > 0
            batch = counts_to_volts(counts);

            % Store recorded EMG
            if n + nb > size(samples, 1)
                samples = [samples; zeros(1000 + nb, 2)]; %#ok<AGROW>
            end
            samples(n+(1:nb), :) = batch;
            n = n + nb;

            % Update live plot
            t = (live.sample_count + (1:nb))' / live.fs;
            addpoints(live.traces(1), t, batch(:, 1));
            addpoints(live.traces(2), t, batch(:, 2));
            live.sample_count = live.sample_count + nb;
            if t(end) > 5
                xlim(live.ax, [t(end)-5, t(end)]);
            end
            drawnow limitrate;
        end
    end
    pause(0.01);
end

% Keep samples that arrived during the last loop pass (the next capture flushes them otherwise),
% then trim to exactly `seconds` of data (e.g. 5000 samples for the 5 s rest)
if isfinite(seconds)
    if uno.NumBytesAvailable > 0
        counts = parse_emg_chunk([leftover, read(uno, uno.NumBytesAvailable, "char")]);
        nb = size(counts, 1);
        samples(n+(1:nb), :) = counts_to_volts(counts);
        n = n + nb;
    end
    n = min(n, round(seconds * live.fs));
end
samples = samples(1:n, :);
if isfinite(seconds) && n < seconds * live.fs * 0.5
    warning('Only %d samples received in %.1f seconds.', n, seconds);
end
end
