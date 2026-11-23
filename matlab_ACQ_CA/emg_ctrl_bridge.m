function emg_ctrl_bridge(uno, nano, calibration, controllerName, mappingName, dryRun, opts)
%EMG_CTRL_BRIDGE  Live drive loop: Uno EMG -> Kirby's controller -> mapping -> drive byte to the Nano.
%   emg_ctrl_bridge(uno, nano, calibration, controllerName, mappingName, dryRun)
%   emg_ctrl_bridge(..., opts)   optional: opts.plotPeriod = seconds between redraws (default 0.1)
%
%   Normally started from run_emg_car.m, which opens the ports, calibrates and picks the controller and
%   mapping. Runs until the drive window is closed.
%
%   uno            : open serialport, Uno running full_2_channel_wireless_ctrl_alg.ino (250000 baud)
%   nano           : open serialport, Nano ESP32 running nrf_tx_drive.ino (115200)
%   calibration    : struct from emg_calibrate.m
%   controllerName : e.g. "control03b" (see run_controller.m)
%   mappingName    : e.g. "map_go_brake" (a file in mappings/)
%   dryRun         : true = everything runs and is shown, but only E-STOP is sent to the car
%
%   Every 20 ms (CTRL_PERIOD_S): new samples -> volts -> each channel's buffer -> controller -> 0..1
%     (Kirby's loop ran his controllers about every 20 ms, and control03b's smoothing is per call.)
%   Every 50 ms (SEND_PERIOD_S): mapping turns [A0 A1] into the command bits -> one byte to the Nano.
%
%   Safety:
%     - E-STOP button or spacebar: sends E-STOP until RESUME is clicked. LIVE mode starts stopped:
%       click RESUME to drive.
%     - No EMG data for 0.25 s (Uno unplugged or reset): E-STOP is sent automatically.
%     - Closing the window or Ctrl+C: E-STOP x5. MATLAB stopping for 200 ms: the STM32 failsafe stops.
%     - At the end it prints the longest gap between commands. Over ~150 ms means this computer is
%       drawing too slowly: raise PLOT_PERIOD_S below (e.g. 0.25).

%% ---------------- Settings ----------------
FS              = 1000;    % Uno sample rate [Hz]
CTRL_PERIOD_S   = 0.02;    % controller update period (matches Kirby's loop)
SEND_PERIOD_S   = 0.05;    % one command per STM32 control tick (must stay well under its 200 ms failsafe)
PLOT_PERIOD_S   = 0.1;     % plot refresh; don't lower it, or drawing can stall the command loop
if nargin >= 7 && isfield(opts, 'plotPeriod')
    PLOT_PERIOD_S = opts.plotPeriod;
end
EMG_PLOT_S      = 5;       % EMG history shown
EMG_PLOT_STEP   = 4;       % plot every 4th EMG sample (1250 points per line) to keep drawing fast
CTRL_PLOT_S     = 10;      % controller-output history shown
NO_DATA_ESTOP_S = 0.25;    % E-STOP if the Uno goes quiet this long
LEVEL_START     = 0.20;    % mapping activation level (slider)
SENSITIVITY_START = [1.00 1.00];   % Kirby's control03b defaults [Channel 1, Channel 2]
SPEED_START       = [0.10 0.10];
BASELINE_START    = [0.00 0.00];

% Must match the KEY_xxx defines in stm_car_proj/Demos/full_drive_demo.c
KEY = struct('FWD', 1, 'SLOW', 2, 'LEFT', 4, 'RIGHT', 8, 'ESTOP', 16);

%% ---------------- Checks ----------------
assert(calibration.n_channels == 2, 'Two-channel calibration required.');
assert(all(isfinite(calibration.mean_contr_rms)) && all(calibration.mean_contr_rms > calibration.rest_rms), ...
    'Invalid contraction RMS. Repeat calibration.');
mapFn = str2func(mappingName);
window = calibration.window;
calName = '(unsaved)';
if isfield(calibration, 'name')
    calName = calibration.name;
end
if dryRun
    modeText = 'DRY RUN (only E-STOP is sent)';
else
    modeText = 'LIVE';
end
fprintf('\nDriving: %s | controller %s | mapping %s | calibration %s\n', modeText, controllerName, mappingName, calName);
fprintf('Close the drive window to stop.\n');

stopCar = onCleanup(@() send_estop(nano, KEY.ESTOP, SEND_PERIOD_S));

%% ---------------- Buffers ----------------
% Kirby's per-channel buffer struct (.data column, .ptr = next write index), kept at a fixed length
% so memory doesn't grow during long sessions. Controllers only look at the last `window` samples.
BUF_N = max(2*window, FS);
buffers = repmat(struct('data', nan(BUF_N, 1), 'ptr', BUF_N + 1, 'ptr_prev', BUF_N + 1), 1, 2);
run_controller("reset");

nEmg = EMG_PLOT_S * FS;
nCtrl = round(CTRL_PLOT_S / CTRL_PERIOD_S);
emgHist = nan(nEmg, 2);
ctrlHist = nan(nCtrl, 2);

%% ---------------- Window ----------------
emgPlotIdx = EMG_PLOT_STEP:EMG_PLOT_STEP:nEmg;
ui = build_window(controllerName, mappingName, LEVEL_START, SENSITIVITY_START, SPEED_START, BASELINE_START, ...
    (emgPlotIdx - nEmg) / FS, (-nCtrl+1:0) * CTRL_PERIOD_S);
fig = ui.fig;
setappdata(fig, 'estop', ~dryRun);      % LIVE starts stopped: click RESUME to drive
drawnow;                                % first (slowest) draw happens here, before any commands are timed

%% ---------------- Main loop ----------------
leftover = '';
pending = zeros(0, 2);
nTotal = 0;
values = [0 0];
mapState = struct();
cmd = uint8(KEY.ESTOP);
sent = uint8(KEY.ESTOP);
samplesPerSec = 0;
nSinceRate = 0;
noData = false;
nSent = 0;
maxGap = 0;
flush(uno);
% Controller and send times run on a fixed schedule (next = previous + period), so they average
% exactly 20 ms / 50 ms even though each loop pass (pause) takes a few ms on Windows.
tSession = tic;
nextCtrl = CTRL_PERIOD_S;
nextSend = SEND_PERIOD_S;
lastSend = NaN;
tPlot = tic; tRate = tic; tLastData = tic;

while isgraphics(fig)
    % Closing the window can happen at any point in an iteration (it is processed during pause,
    % drawnow or serial reads). If the window is gone, that is a normal stop, not an error.
    try
        % (1) Uno -> volts
        nBytes = uno.NumBytesAvailable;
        if nBytes > 0
            [counts, leftover, status] = parse_emg_chunk([leftover, read(uno, nBytes, "char")]);
            for i = 1:numel(status)
                fprintf('[uno] %s\n', status{i});
            end
            k = size(counts, 1);
            if k > 0
                volts = counts_to_volts(counts);
                pending = [pending; volts]; %#ok<AGROW> emptied every 20 ms
                emgHist = shift_in(emgHist, volts);
                nSinceRate = nSinceRate + k;
                tLastData = tic;
            end
        end

        % (2) Controller, every CTRL_PERIOD_S
        tNow = toc(tSession);
        if tNow >= nextCtrl
            nextCtrl = next_tick(nextCtrl, CTRL_PERIOD_S, tNow);
            if ~isempty(pending)
                for ch = 1:2
                    buffers(ch).data = shift_in(buffers(ch).data, pending(:, ch));
                end
                nTotal = nTotal + size(pending, 1);
                pending = zeros(0, 2);
                if nTotal >= window
                    sliders = read_sliders(ui);
                    for ch = 1:2
                        values(ch) = run_controller(controllerName, buffers(ch), ch, calibration, sliders);
                    end
                end
                ctrlHist = shift_in(ctrlHist, values);
            end
        end

        % (3) Mapping -> one byte to the Nano, every SEND_PERIOD_S
        tNow = toc(tSession);
        if tNow >= nextSend
            nextSend = next_tick(nextSend, SEND_PERIOD_S, tNow);
            if nSent > 0
                maxGap = max(maxGap, tNow - lastSend);
            end
            lastSend = tNow;
            nSent = nSent + 1;
            settings.level = ui.level.Value;
            [cmd, mapState] = mapFn(values, settings, mapState, KEY);
            noData = toc(tLastData) > NO_DATA_ESTOP_S;
            if getappdata(fig, 'estop') || noData
                cmd = uint8(KEY.ESTOP);
            end
            if dryRun
                sent = uint8(KEY.ESTOP);
            else
                sent = bitand(uint8(cmd), uint8(31));
            end
            write(nano, sent, "uint8");
        end

        % (4) Samples/s
        if toc(tRate) >= 1
            samplesPerSec = nSinceRate / toc(tRate);
            nSinceRate = 0;
            tRate = tic;
        end

        % (5) Window, rate limited
        if toc(tPlot) >= PLOT_PERIOD_S
            tPlot = tic;
            update_window(ui, emgHist(emgPlotIdx, :), ctrlHist, values, cmd, sent, samplesPerSec, modeText, ...
                getappdata(fig, 'estop'), noData, KEY);
        end
        pause(0.002);
    catch err
        if isgraphics(fig)
            rethrow(err);
        end
    end
end

fprintf('Session: %.0f s, %d commands sent, longest gap between commands %.0f ms.\n', ...
    toc(tSession), nSent, 1000*maxGap);
if maxGap > 0.15
    warning('emg_ctrl_bridge:slow', ['Commands were up to %.0f ms apart (the STM32 stops the car after 200 ms). ' ...
        'This computer is drawing too slowly: set PLOT_PERIOD_S = 0.25 at the top of emg_ctrl_bridge.m.'], 1000*maxGap);
end
end


%% =====================================================================================
function ui = build_window(controllerName, mappingName, levelStart, sensStart, speedStart, baseStart, tEmg, tCtrl)
ui.fig = figure('Name', sprintf('EMG car: %s + %s', controllerName, mappingName), 'NumberTitle', 'off', ...
    'Position', [100 80 900 760]);

% EMG (volts)
ui.axEmg = axes(ui.fig, 'Position', [0.08 0.73 0.88 0.22]);
hold(ui.axEmg, 'on'); grid(ui.axEmg, 'on');
ui.emgLines = [plot(ui.axEmg, tEmg, nan(size(tEmg)), 'b'), plot(ui.axEmg, tEmg, nan(size(tEmg)), 'r')];
ylim(ui.axEmg, [-3 3]); xlim(ui.axEmg, [tEmg(1) 0]);
ylabel(ui.axEmg, 'EMG (V)');
legend(ui.axEmg, 'Channel 1 (A0)', 'Channel 2 (A1)', 'Location', 'northwest');

% Controller output (0..1) with the mapping's activation level
ui.axCtrl = axes(ui.fig, 'Position', [0.08 0.43 0.88 0.22]);
hold(ui.axCtrl, 'on'); grid(ui.axCtrl, 'on');
ui.ctrlLines = [plot(ui.axCtrl, tCtrl, nan(size(tCtrl)), 'b', 'LineWidth', 1.5), ...
                plot(ui.axCtrl, tCtrl, nan(size(tCtrl)), 'r', 'LineWidth', 1.5)];
ui.levelLine = yline(ui.axCtrl, levelStart, 'k--', 'Activation level');
ylim(ui.axCtrl, [-0.1 1.1]); xlim(ui.axCtrl, [tCtrl(1) 0]);
xlabel(ui.axCtrl, 'Time (s)'); ylabel(ui.axCtrl, sprintf('%s output', controllerName), 'Interpreter', 'none');

% Kirby's per-channel sliders: Sensitivity (higher = less sensitive), Speed (higher = faster
% response, less smoothing), Baseline (higher = more resting-output correction).
% Enabled only where the selected controller uses them.
names = {'Sensitivity', 'Speed', 'Baseline'};
limits = [0.50 2.50; 0.01 1.00; 0.00 0.90];
initial = [sensStart(:) speedStart(:) baseStart(:)];
used = struct('control01', [false false false], 'control03b', [true true true], 'control04', [true false false]);
if isfield(used, controllerName)
    enabled = used.(controllerName);
else
    enabled = [true true true];
end
onOff = {'off', 'on'};
estopKey = @(~, evt) key_estop(ui.fig, evt);
ui.dials = gobjects(2, 3);
ui.readouts = gobjects(2, 3);
for ch = 1:2
    x = 0.02 + (ch-1)*0.50;
    uicontrol(ui.fig, 'Style', 'text', 'String', sprintf('Channel %d (A%d) adjustments', ch, ch-1), ...
        'Units', 'normalized', 'Position', [x 0.33 0.46 0.03]);
    for p = 1:3
        y = 0.28 - (p-1)*0.05;
        uicontrol(ui.fig, 'Style', 'text', 'String', names{p}, 'Units', 'normalized', 'Position', [x y 0.14 0.03]);
        ui.dials(ch, p) = uicontrol(ui.fig, 'Style', 'slider', 'Min', limits(p, 1), 'Max', limits(p, 2), ...
            'Value', initial(ch, p), 'Units', 'normalized', 'Position', [x+0.14 y 0.24 0.03], ...
            'Enable', onOff{enabled(p) + 1}, 'KeyPressFcn', estopKey);
        ui.readouts(ch, p) = uicontrol(ui.fig, 'Style', 'text', 'String', sprintf('%.2f', initial(ch, p)), ...
            'Units', 'normalized', 'Position', [x+0.39 y 0.06 0.03]);
    end
end

% Mapping activation level
uicontrol(ui.fig, 'Style', 'text', 'String', 'Activation level', 'Units', 'normalized', 'Position', [0.02 0.10 0.14 0.03]);
ui.level = uicontrol(ui.fig, 'Style', 'slider', 'Min', 0.05, 'Max', 0.95, 'Value', levelStart, ...
    'Units', 'normalized', 'Position', [0.16 0.10 0.24 0.03], 'KeyPressFcn', estopKey);
ui.levelReadout = uicontrol(ui.fig, 'Style', 'text', 'String', sprintf('%.2f', levelStart), ...
    'Units', 'normalized', 'Position', [0.41 0.10 0.06 0.03]);

% E-STOP / RESUME
ui.estopBtn = uicontrol(ui.fig, 'Style', 'pushbutton', 'String', 'E-STOP (space)', 'FontSize', 14, ...
    'FontWeight', 'bold', 'BackgroundColor', [0.85 0.1 0.1], 'ForegroundColor', 'w', ...
    'Units', 'normalized', 'Position', [0.52 0.03 0.22 0.10], 'Callback', @(~, ~) setappdata(ui.fig, 'estop', true));
ui.resumeBtn = uicontrol(ui.fig, 'Style', 'pushbutton', 'String', 'RESUME', 'FontSize', 12, ...
    'BackgroundColor', [0.2 0.65 0.2], 'ForegroundColor', 'w', 'Units', 'normalized', ...
    'Position', [0.76 0.03 0.20 0.10], 'Callback', @(~, ~) resume(ui.fig, ui.estopBtn));

% Status line
ui.status = uicontrol(ui.fig, 'Style', 'text', 'String', '', 'FontSize', 11, 'FontWeight', 'bold', ...
    'HorizontalAlignment', 'left', 'Units', 'normalized', 'Position', [0.02 0.03 0.48 0.05]);

set(ui.fig, 'KeyPressFcn', estopKey);
end


function update_window(ui, emgHist, ctrlHist, values, cmd, sent, samplesPerSec, modeText, latched, noData, KEY)
if ~isgraphics(ui.fig)
    return
end
set(ui.emgLines(1), 'YData', emgHist(:, 1));
set(ui.emgLines(2), 'YData', emgHist(:, 2));
set(ui.ctrlLines(1), 'YData', ctrlHist(:, 1));
set(ui.ctrlLines(2), 'YData', ctrlHist(:, 2));
if ui.levelLine.Value ~= ui.level.Value
    ui.levelLine.Value = ui.level.Value;
end
set_text(ui.levelReadout, sprintf('%.2f', ui.level.Value));
for ch = 1:2
    for p = 1:3
        set_text(ui.readouts(ch, p), sprintf('%.2f', ui.dials(ch, p).Value));
    end
end
ui.axEmg.Title.String = sprintf('%s   |   %.0f samples/s', modeText, samplesPerSec);
ui.axCtrl.Title.String = sprintf('Channel 1: %.3f   |   Channel 2: %.3f   |   mapping says: %s', ...
    values(1), values(2), cmd_to_text(cmd, KEY));

if noData
    set_text(ui.status, 'NO EMG DATA -> E-STOP', [0.85 0.1 0.1]);
elseif latched
    set_text(ui.status, 'STOPPED (E-STOP). Click RESUME to drive.', [0.85 0.1 0.1]);
else
    set_text(ui.status, sprintf('Sent to car: %s', cmd_to_text(sent, KEY)), [0 0.5 0]);
end
drawnow limitrate;
end


function set_text(h, str, color)
% Only touch the control when something changed (each change costs a redraw)
if ~strcmp(h.String, str)
    h.String = str;
end
if nargin > 2 && ~isequal(h.ForegroundColor, color)
    h.ForegroundColor = color;
end
end


function next = next_tick(next, period, tNow)
% Next scheduled time. If we fell more than a period behind (e.g. a slow redraw), restart the
% schedule from now instead of firing several times in a row to catch up.
next = next + period;
if next <= tNow
    next = tNow + period;
end
end


function sliders = read_sliders(ui)
sliders.sensitivity = [ui.dials(1, 1).Value, ui.dials(2, 1).Value];
sliders.speed       = [ui.dials(1, 2).Value, ui.dials(2, 2).Value];
sliders.baseline    = [ui.dials(1, 3).Value, ui.dials(2, 3).Value];
end


function hist = shift_in(hist, new)
% Append rows to a fixed-length history, dropping the oldest
k = size(new, 1);
if k >= size(hist, 1)
    hist = new(end-size(hist, 1)+1:end, :);
elseif k > 0
    hist = [hist(k+1:end, :); new];
end
end


function key_estop(fig, evt)
if strcmp(evt.Key, 'space')
    setappdata(fig, 'estop', true);
end
end


function resume(fig, estopBtn)
setappdata(fig, 'estop', false);
uicontrol(estopBtn);    % move keyboard focus to E-STOP, so the spacebar stops (not re-resumes)
end


function txt = cmd_to_text(cmd, KEY)
names = {'FWD', 'SLOW', 'LEFT', 'RIGHT', 'E-STOP'};
bits = [KEY.FWD, KEY.SLOW, KEY.LEFT, KEY.RIGHT, KEY.ESTOP];
on = bitand(double(cmd), bits) > 0;
if any(on)
    txt = strjoin(names(on), '+');
else
    txt = 'none';
end
end


function send_estop(nano, estop, period)
% Runs when the drive window closes, on error, or on Ctrl+C
try
    for i = 1:5
        write(nano, uint8(estop), "uint8");
        pause(period);
    end
catch err
    warning('emg_ctrl_bridge:estop', ...
        'Could not send E-STOP (%s). The STM32 failsafe stops the car after 200 ms.', err.message);
end
end
