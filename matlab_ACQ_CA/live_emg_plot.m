function live = live_emg_plot(name)
%LIVE_EMG_PLOT  Live two-channel EMG plot used while recording (calibration) or checking the hookup.
%   live = live_emg_plot(name)
%
%   Same plot Kirby's code builds in Sections 0B, 1A and 1B: blue = Channel 1 (A0), red = Channel 2
%   (A1), volts, last 5 s. Pass the returned struct to capture_emg.

live.fig = figure('Name', name, 'NumberTitle', 'off');
live.ax = axes(live.fig);
hold(live.ax, 'on');
grid(live.ax, 'on');
live.traces = gobjects(1, 2);
live.traces(1) = animatedline(live.ax, 'Color', 'b', 'MaximumNumPoints', 5000);
live.traces(2) = animatedline(live.ax, 'Color', 'r', 'MaximumNumPoints', 5000);
xlabel(live.ax, 'Time (s)');
ylabel(live.ax, 'EMG (V)');
legend(live.ax, 'Channel 1 (A0)', 'Channel 2 (A1)');
xlim(live.ax, [0 5]);
ylim(live.ax, [-3 3]);
live.sample_count = 0;
live.fs = 1000;
drawnow;    % do the first (slowest) draw now, not in the middle of a recording
end
