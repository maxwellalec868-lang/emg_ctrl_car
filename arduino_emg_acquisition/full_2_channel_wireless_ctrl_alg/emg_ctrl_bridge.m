function emg_ctrl_bridge(emgPort, radioPort, dryRun)
%EMG_CTRL_BRIDGE  EMG from the Uno -> control algorithm -> drive command to the Nano ESP32 radio, with a live plot.
%
%   emg_ctrl_bridge                              list the serial ports (to find which is which)
%   emg_ctrl_bridge("COM5", "COM7")              DRY RUN: computes commands but only sends E-STOP
%   emg_ctrl_bridge("COM5", "COM7", false)       LIVE: sends the algorithm's commands to the car
%
%   emgPort   : the Uno running full_2_channel_wireless_ctrl_alg.ino (250000 baud, "%04d %04d" lines)
%   radioPort : the Nano ESP32 running nrf_tx_drive.ino (forwards each byte to the STM32 over the nRF)
%   dryRun    : true (default) = only E-STOP is sent, the wheels never move
%
%   Every CMD_PERIOD_S this sends ONE byte to the Nano, the same bitmask pc_drive.py sends:
%     0x01 forward, 0x02 slow/stop, 0x04 left, 0x08 right, 0x10 EMERGENCY STOP
%   STM32 side: demo_full_PC_car_ctrl() in stm_car_proj/Demos/full_drive_demo.c (unchanged).
%
%   Put your algorithm in control_algorithm() at the bottom of this file.
%
%   Stop: close the plot window (or Ctrl+C). Either way E-STOP is sent 5 times and both ports close.
%   FAILSAFE: if MATLAB stops sending for 200 ms (crash, USB unplugged, or a long stall), the STM32
%   stops the car on its own. Keep heavy work out of the loop so it doesn't stall.
%
%   Close the Arduino Serial Monitors first: only one program can hold each port.

if nargin < 2
    fprintf("Available serial ports:\n");
    disp(serialportlist("available").');
    fprintf("Usage: emg_ctrl_bridge(""COM_uno"", ""COM_nano"")          %% dry run (E-STOP only)\n");
    fprintf("       emg_ctrl_bridge(""COM_uno"", ""COM_nano"", false)   %% live\n");
    fprintf("Tip: unplug one board and list again to see which port disappears.\n");
    return
end
if nargin < 3
    dryRun = true;
end

%% ---------------- Settings (must match the sketches) ----------------
EMG_BAUD      = 250000;   % full_2_channel_wireless_ctrl_alg.ino
RADIO_BAUD    = 115200;   % nrf_tx_drive.ino
FS            = 1000;     % Uno sampling rate [Hz]
CMD_PERIOD_S  = 0.05;     % one command per STM32 control tick. Must stay well under the 200 ms failsafe
ALG_WINDOW_S  = 0.1;      % how much recent EMG control_algorithm() gets
PLOT_SECONDS  = 5;        % live plot history
PLOT_PERIOD_S = 0.1;      % plot refresh (keep >= 0.1 s so drawing never stalls the command loop)

% Must match the KEY_xxx defines in stm_car_proj/Demos/full_drive_demo.c
KEY = struct('FWD', 1, 'SLOW', 2, 'LEFT', 4, 'RIGHT', 8, 'ESTOP', 16);

%% ---------------- Open ports ----------------
emg = serialport(emgPort, EMG_BAUD);
radio = serialport(radioPort, RADIO_BAUD);
cleanup = onCleanup(@() shutdown(radio, KEY.ESTOP, CMD_PERIOD_S));   % runs on return, error or Ctrl+C

write(radio, uint8(KEY.ESTOP), "uint8");
fprintf("Waiting for the Uno to restart (opening its port resets it)...\n");
pause(2);
flush(emg);

%% ---------------- Live plot ----------------
nPlot = PLOT_SECONDS * FS;
nAlg = round(ALG_WINDOW_S * FS);
history = nan(nPlot, 2);                      % rolling buffer, newest sample in the last row
t = (-nPlot+1:0) / FS;

fig = figure('Name', 'EMG car control', 'NumberTitle', 'off');
ax1 = subplot(2, 1, 1);
h1 = plot(ax1, t, history(:, 1));
ylim(ax1, [0 1023]); ylabel(ax1, 'A0 (bottom shield)'); grid(ax1, 'on');
ax2 = subplot(2, 1, 2);
h2 = plot(ax2, t, history(:, 2));
ylim(ax2, [0 1023]); ylabel(ax2, 'A1 (top shield)'); xlabel(ax2, 'time (s)'); grid(ax2, 'on');

if dryRun
    modeText = 'DRY RUN (E-STOP only)';
else
    modeText = 'LIVE';
end
fprintf("Running %s. Close the plot window to stop.\n", modeText);

%% ---------------- Main loop ----------------
leftover = '';
cmd = uint8(KEY.ESTOP);
samplesPerSec = 0;
nSinceRate = 0;
tCmd = tic; tPlot = tic; tRate = tic;

while isgraphics(fig)
    % (1) Everything the Uno has sent since last time
    nBytes = emg.NumBytesAvailable;
    if nBytes > 0
        [samples, leftover, status] = parse_emg_chunk([leftover, read(emg, nBytes, "char")]);
        for i = 1:numel(status)
            fprintf("[uno] %s\n", status{i});
        end
        k = size(samples, 1);
        if k >= nPlot
            history = samples(end-nPlot+1:end, :);
        elseif k > 0
            history = [history(k+1:end, :); samples];
        end
        nSinceRate = nSinceRate + k;
    end

    % (2) One decision per CMD_PERIOD_S -> one byte to the Nano
    if toc(tCmd) >= CMD_PERIOD_S
        tCmd = tic;
        window = history(end-nAlg+1:end, :);
        if any(isnan(window(:)))
            cmd = uint8(KEY.ESTOP);           % not enough data yet
        else
            cmd = uint8(control_algorithm(window, KEY));
        end
        if dryRun
            out = uint8(KEY.ESTOP);
        else
            out = bitand(cmd, uint8(31));     % only the 5 defined bits
        end
        write(radio, out, "uint8");
    end

    % (3) Samples/s, once a second
    if toc(tRate) >= 1
        samplesPerSec = nSinceRate / toc(tRate);
        nSinceRate = 0;
        tRate = tic;
    end

    % (4) Live plot, rate limited
    if toc(tPlot) >= PLOT_PERIOD_S
        tPlot = tic;
        set(h1, 'YData', history(:, 1));
        set(h2, 'YData', history(:, 2));
        title(ax1, sprintf('%s   |   %.0f samples/s   |   cmd: %s', ...
            modeText, samplesPerSec, cmd_to_text(cmd, KEY)));
        drawnow limitrate;
    end

    pause(0.002);                             % yield; also lets MATLAB notice the window closing
end
end


%% =====================================================================================
function cmd = control_algorithm(window, KEY)
% CONTROL_ALGORITHM  PLACEHOLDER: replace the body with your own EMG control algorithm.
%
%   window : nAlg x 2 double, raw ADC counts 0..1023 (col 1 = A0 bottom shield, col 2 = A1 top shield),
%            the most recent ALG_WINDOW_S of data, oldest first, sampled at 1000 Hz.
%   KEY    : the command bits (KEY.FWD, KEY.SLOW, KEY.LEFT, KEY.RIGHT, KEY.ESTOP)
%   cmd    : bitmask, e.g. KEY.FWD + KEY.LEFT. 0 = nothing pressed (the car cruises, wheels re-center).
%
%   Called every 50 ms. Keep it fast (a few ms). Use `persistent` variables for filter state, etc.
%
%   Placeholder: envelope = mean |x - mean(x)| per channel, then a threshold.
%     A0 active -> forward, A1 active -> steer right, both -> slow down.
THRESHOLD = 40;   % ADC counts; tune per person by watching the plot

envelope = mean(abs(window - mean(window, 1)), 1);
a0 = envelope(1) > THRESHOLD;
a1 = envelope(2) > THRESHOLD;

if a0 && a1
    cmd = KEY.SLOW;
elseif a0
    cmd = KEY.FWD;
elseif a1
    cmd = KEY.RIGHT;
else
    cmd = 0;
end
end


function txt = cmd_to_text(cmd, KEY)
% "FWD+LEFT", "E-STOP" or "none", for the plot title
names = {'FWD', 'SLOW', 'LEFT', 'RIGHT', 'E-STOP'};
bits = [KEY.FWD, KEY.SLOW, KEY.LEFT, KEY.RIGHT, KEY.ESTOP];
on = bitand(double(cmd), bits) > 0;
if any(on)
    txt = strjoin(names(on), '+');
else
    txt = 'none';
end
end


function shutdown(radio, estop, period)
% Runs when emg_ctrl_bridge returns, errors or is stopped with Ctrl+C
try
    for i = 1:5
        write(radio, uint8(estop), "uint8");
        pause(period);
    end
    fprintf("Sent E-STOP. Ports closed.\n");
catch err
    warning("emg_ctrl_bridge:shutdown", ...
        "Could not send E-STOP (%s). The STM32 failsafe stops the car after 200 ms.", err.message);
end
end
