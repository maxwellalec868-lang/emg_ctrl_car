function run_emg_car(unoPort, nanoPort)
%RUN_EMG_CAR  Start here: calibrate a user, then drive the car with their EMG.
%   run_emg_car                       list the serial ports (to find which board is which)
%   run_emg_car("COM5", "COM7")       Uno port first, then the Nano port
%
%   Boards: Uno + 2 SpikerShields running full_2_channel_wireless_ctrl_alg.ino,
%           Nano ESP32 + nRF running nrf_tx_drive.ino. STM32 running demo_full_PC_car_ctrl().
%   Close the Arduino Serial Monitors first: only one program can hold each port.
%
%   Steps (menus in the Command Window):
%     1. View signals (optional): check the electrode hookup
%     2. Calibration: new, load saved, or revise saved (Kirby's Sections 1A/1B)
%     3. Pick the controller and the mapping
%     4. Drive: dry run (only E-STOP is sent) or LIVE. Close the drive window to stop.
%     5. Then: drive again, change controller/mapping, recalibrate, view signals, or quit
%   The ports stay open the whole time, so the Uno only resets once.
%   Leaving by any route (quit, error, Ctrl+C) sends E-STOP and closes both ports.
%
%   To add or reorder controllers/mappings, edit the two lists below (first = default).

CONTROLLERS = ["control03b", "control01", "control04"];              % see run_controller.m
MAPPINGS    = ["map_go_brake", "map_prop_go_brake", "map_steer"];    % files in mappings/

UNO_BAUD  = 250000;   % full_2_channel_wireless_ctrl_alg.ino
NANO_BAUD = 115200;   % nrf_tx_drive.ino
KEY_ESTOP = 16;

if nargin < 2
    fprintf('Available serial ports:\n');
    disp(serialportlist("available").');
    fprintf('Usage: run_emg_car("COM_uno", "COM_nano")\n');
    fprintf('Tip: unplug one board and list again to see which port disappears.\n');
    return
end

here = fileparts(mfilename('fullpath'));
addpath(here, fullfile(here, 'mappings'), fullfile(here, 'kirby_matlab_code'));

%% ---------------- Connect ----------------
uno = serialport(unoPort, UNO_BAUD);
nano = serialport(nanoPort, NANO_BAUD);
closeAll = onCleanup(@() shutdown(nano, KEY_ESTOP));
write(nano, uint8(KEY_ESTOP), "uint8");
fprintf('Ports open. Waiting 2 s for the Uno to restart (opening its port resets it)...\n');
pause(2);
check_boards(uno, nano, unoPort, nanoPort);

%% ---------------- Calibrate, choose, drive ----------------
if ask_yes_no('View live signals first to check the electrode hookup?', false)
    view_signals(uno);
end
calibration = calibrate_until_valid(uno);
controller = choose('Controller', CONTROLLERS, CONTROLLERS(1));
mapping = choose('Mapping', MAPPINGS, MAPPINGS(1));

while true
    dryRun = ~ask_yes_no(sprintf('Drive LIVE with %s + %s? (no = dry run, only E-STOP is sent)', controller, mapping), false);
    if ~dryRun
        input('LIVE: wheels off the ground / area clear? Press Enter to continue (Ctrl+C to abort): ', 's');
    end
    emg_ctrl_bridge(uno, nano, calibration, controller, mapping, dryRun);

    fprintf('\nNext:\n  1 - Drive again (same settings)\n  2 - Change controller / mapping\n');
    fprintf('  3 - Recalibrate (new / load / revise)\n  4 - View live signals\n  5 - Quit\n');
    next = input('Choose (1-5): ');
    if ~isscalar(next)
        next = 1;
    end
    switch next
        case 2
            controller = choose('Controller', CONTROLLERS, controller);
            mapping = choose('Mapping', MAPPINGS, mapping);
        case 3
            calibration = calibrate_until_valid(uno);
        case 4
            view_signals(uno);
        case 5
            break
    end
end
fprintf('Done.\n');
end


%% =====================================================================================
function check_boards(uno, nano, unoPort, nanoPort)
% Catch swapped ports / test firmware before calibrating
pause(0.5);
n = uno.NumBytesAvailable;
text = '';
if n > 0
    text = read(uno, n, "char");
end
[samples, ~, status] = parse_emg_chunk(text);
if any(contains(status, 'USB flow test'))
    warning('The Uno is running the USB flow test sketch. Flash full_2_channel_wireless_ctrl_alg.ino.');
elseif size(samples, 1) < 100
    warning(['Little or no EMG data from %s. Is it the Uno, running full_2_channel_wireless_ctrl_alg.ino? ' ...
        'Are the two ports swapped?'], unoPort);
end
if nano.NumBytesAvailable > 0
    warning(['%s answered the E-STOP byte. It is probably running nano_usb_echo.ino ' ...
        '(flash nrf_tx_drive.ino), or it is the Uno (ports swapped).'], nanoPort);
    flush(nano);
end
flush(uno);
end


function calibration = calibrate_until_valid(uno)
% Kirby's Section 2 check: contraction RMS must be finite and above rest RMS on both channels
while true
    calibration = emg_calibrate(uno);
    ok = all(isfinite(calibration.mean_contr_rms)) && all(calibration.mean_contr_rms > calibration.rest_rms);
    if ok
        return
    end
    warning('Invalid calibration (a channel has no usable contractions). Please calibrate again.');
end
end


function view_signals(uno)
% Kirby's Section 0B: live signals until the window is closed
live = live_emg_plot('Live EMG');
fprintf('Showing live EMG. Close the window to continue.\n');
capture_emg(uno, Inf, live, 'Live SpikerShield EMG (close window to continue)', true);
end


function pick = choose(label, options, current)
fprintf('\n%s:\n', label);
for k = 1:numel(options)
    mark = '';
    if options(k) == current
        mark = '   <- current';
    end
    fprintf('  %d - %s%s\n', k, options(k), mark);
end
choice = input(sprintf('Choose (1-%d, Enter = keep current): ', numel(options)));
if isscalar(choice) && ismember(choice, 1:numel(options))
    pick = options(choice);
else
    pick = current;
end
fprintf('%s: %s\n', label, pick);
end


function yes = ask_yes_no(question, default)
if default
    hint = '[Y/n]';
else
    hint = '[y/N]';
end
answer = strtrim(lower(input(sprintf('%s %s: ', question, hint), 's')));
if isempty(answer)
    yes = default;
else
    yes = startsWith(answer, 'y');
end
end


function shutdown(nano, estop)
try
    for i = 1:5
        write(nano, uint8(estop), "uint8");
        pause(0.05);
    end
    fprintf('Sent E-STOP. Ports closed.\n');
catch err
    warning('run_emg_car:shutdown', ...
        'Could not send E-STOP (%s). The STM32 failsafe stops the car after 200 ms.', err.message);
end
end
