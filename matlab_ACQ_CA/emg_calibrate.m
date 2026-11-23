function calibration = emg_calibrate(uno, mode)
%EMG_CALIBRATE  Calibrate a user: new, load saved, or revise saved. Returns the calibration struct.
%   calibration = emg_calibrate(uno)            % asks: 1 = new, 2 = load, 3 = revise
%   calibration = emg_calibrate(uno, "new")     % or "load" / "revise"
%
%   Kirby's Sections 1A (calibration) and 1B (revise saved calibration) from
%   final_project-2channel-original.m, as one function. Same recording protocol, same math
%   (compute_calibration_channel.m), same review plots, printouts and redo menu.
%
%   New calibration protocol:
%     1. REST: relax both muscles for 5 s
%     2. Channel 1, then Channel 2: N x (2 s CONTRACT + 2 s RELAX)
%     3. Review plots and values -> redo Channel 1, Channel 2, both, or accept and save
%   Files are saved in matlab_ACQ_CA/saved_data/<name>.mat (variable "calibration"), the same format
%   as Kirby's, so his saved files load here and these load in his code.
%
%   uno : open serialport to the Uno (not used for "load"; pass [] then)

if nargin < 2 || isempty(mode)
    fprintf('\nCalibration:\n  1 - New calibration\n  2 - Load saved calibration\n  3 - Revise saved calibration\n');
    choice = input('Choose (1-3): ');
    assert(isscalar(choice) && ismember(choice, 1:3), 'Choose 1, 2 or 3.');
    modes = ["new", "load", "revise"];
    mode = modes(choice);
end

folder = fullfile(fileparts(mfilename('fullpath')), 'saved_data');

switch string(mode)
    case "new"
        calibration = new_calibration(uno, folder);
    case "load"
        calibration = load_calibration(folder);
    case "revise"
        calibration = revise_calibration(uno, folder);
    otherwise
        error('Unknown mode "%s". Use "new", "load" or "revise".', mode);
end

print_final_summary(calibration);
end


%% ---------------- Section 1A: new calibration ----------------
function calibration = new_calibration(uno, folder)
fs = 1000;

% Select number of contractions
n_contractions = input('How many contractions per trial? ');
validateattributes(n_contractions, {'numeric'}, {'scalar', 'integer', 'positive'});

live = live_emg_plot('Live Calibration EMG');
closeLive = onCleanup(@() close_if_open(live.fig));

% Record Resting EMG
input('Relax both muscles. Press Enter to start: ', 's');
[rest_data, live] = capture_emg(uno, 5, live, 'REST - Relax both muscles');
assert(~isempty(rest_data), 'No resting EMG recorded. Restart calibration.');

% Calibration settings (Kirby's values)
calibration.fs = fs;
calibration.n_channels = 2;
calibration.n_requested = n_contractions;
calibration.window = round(0.25 * fs);
calibration.plateau_fraction = 0.5;
calibration.raw_rest = rest_data;
calibration.raw_fist = cell(1, 2);
calibration.threshold = nan(1, 2);
calibration.rest_rms = nan(1, 2);
calibration.mean_contr_rms = nan(1, 2);
calibration.contraction_rms = cell(1, 2);
calibration.snr = cell(1, 2);
calibration.mean_snr = nan(1, 2);
calibration.std_snr = nan(1, 2);
calibration.n_detected = zeros(1, 2);

% Record Contractions and Review Results
channels_to_record = 1:2;
accepted = false;
while ~accepted
    for ch = channels_to_record
        [calibration.raw_fist{ch}, live] = record_contractions(uno, live, ch, n_contractions);
    end
    for ch = 1:2
        calibration = compute_calibration_channel(calibration, ch);
    end

    review_fig = show_review(calibration);
    print_review(calibration);
    switch review_menu()
        case 1
            channels_to_record = 1;
        case 2
            channels_to_record = 2;
        case 3
            channels_to_record = 1:2;
        case 4
            accepted = true;
        otherwise
            disp('Invalid selection. Please choose 1-4.');
            channels_to_record = [];
    end

    % Close review figure only when repeating
    if ~accepted && isgraphics(review_fig)
        close(review_fig);
    end
end
disp('Calibration recordings accepted.');

[calibration, saved] = save_calibration(calibration, folder, 'Calibration_01');
if ~saved
    warning('Calibration not saved. It is still used for this session.');
end
end


%% ---------------- Load saved calibration ----------------
function calibration = load_calibration(folder)
[file, path] = uigetfile(fullfile(folder, '*.mat'), 'Select saved calibration');
assert(~isequal(file, 0), 'No file selected.');
loaded = load(fullfile(path, file));
assert(isfield(loaded, 'calibration'), 'Selected file does not contain a calibration structure.');
calibration = loaded.calibration;
assert(calibration.n_channels == 2 && calibration.fs == 1000, ...
    'Saved calibration must be for two channels at 1000 Hz.');
[~, calibration.name] = fileparts(file);
fprintf('Calibration loaded: %s\n', file);
end


%% ---------------- Section 1B: revise saved calibration ----------------
function calibration = revise_calibration(uno, folder)
[file, path] = uigetfile(fullfile(folder, '*.mat'), 'Select calibration to revise');
assert(~isequal(file, 0), 'No file selected.');
loaded = load(fullfile(path, file));
assert(isfield(loaded, 'calibration'), 'File does not contain calibration data.');
calibration = loaded.calibration;

% Ensure file contains the original recordings
assert(calibration.n_channels == 2 && calibration.fs == 1000, 'Two-channel, 1000 Hz calibration required.');
assert(isfield(calibration, 'raw_rest') && isfield(calibration, 'raw_fist') && iscell(calibration.raw_fist) ...
    && numel(calibration.raw_fist) == 2, 'Saved file must contain raw calibration recordings.');
if ~isfield(calibration, 'n_requested')
    n = input('How many contractions per trial? ');
    validateattributes(n, {'numeric'}, {'scalar', 'integer', 'positive'});
    calibration.n_requested = n;
end
fprintf('\nLoaded: %s\n', file);
[~, original_name] = fileparts(file);

live = [];
closeLive = onCleanup(@() close_if_open(live));
accepted = false;
while ~accepted
    review_fig = show_review(calibration);
    print_review(calibration);
    choice_review = review_menu();

    if ismember(choice_review, 1:3)
        channels = {1, 2, 1:2};
        if isgraphics(review_fig)
            close(review_fig);
        end
        if isempty(live)
            live = live_emg_plot('Live Calibration EMG');
            closeLive = onCleanup(@() close_if_open(live.fig));   % replaces the empty one
        end
        for ch = channels{choice_review}
            fprintf('\nRedoing Channel %d\n', ch);
            [calibration.raw_fist{ch}, live] = record_contractions(uno, live, ch, calibration.n_requested);
            calibration = compute_calibration_channel(calibration, ch);
        end

    elseif choice_review == 4
        if any(~isfinite(calibration.mean_contr_rms))
            disp('Invalid calibration values. Repeat affected trials.');
            close_if_open(review_fig);
            continue
        end
        [calibration, accepted] = save_calibration(calibration, folder, [original_name '_revised']);
        if ~accepted
            disp('Save canceled. Returning to review.');
            close_if_open(review_fig);
        end

    else
        disp('Invalid selection. Choose 1-4.');
        close_if_open(review_fig);
    end
end
disp('Calibration revision complete.');
end


%% ---------------- Shared pieces (from Kirby's 1A/1B) ----------------
function [data, live] = record_contractions(uno, live, ch, n_contractions)
% N x (2 s CONTRACT + 2 s RELAX) for one channel; records both columns like Kirby's code
fprintf('\nCHANNEL %d: %d contractions\n', ch, n_contractions);
input('Press Enter when ready: ', 's');
figure(live.fig);
data = zeros(0, 2);
for k = 1:n_contractions
    instruction = sprintf('Channel %d - %d/%d: CONTRACT', ch, k, n_contractions);
    fprintf('%s\n', instruction);
    [samples, live] = capture_emg(uno, 2, live, instruction);
    data = [data; samples]; %#ok<AGROW>

    instruction = sprintf('Channel %d - %d/%d: RELAX', ch, k, n_contractions);
    fprintf('%s\n', instruction);
    [samples, live] = capture_emg(uno, 2, live, instruction);
    data = [data; samples]; %#ok<AGROW>
end
end


function review_fig = show_review(calibration)
% Raw EMG and moving RMS with threshold, per channel
review_fig = figure('Name', 'Calibration Review', 'NumberTitle', 'off');
for ch = 1:2
    signal = calibration.raw_fist{ch}(:, ch);
    t = (0:numel(signal)-1) / calibration.fs;

    subplot(2, 2, 2*ch-1);
    plot(t, signal);
    grid on;
    xlabel('Time (s)');
    ylabel('EMG (V)');
    title(sprintf('Channel %d - Raw EMG', ch));

    moving_rms = sqrt(movmean(signal.^2, calibration.window));
    subplot(2, 2, 2*ch);
    plot(t, moving_rms);
    hold on;
    yline(calibration.threshold(ch), 'k--', 'Threshold');
    grid on;
    xlabel('Time (s)');
    ylabel('RMS (V)');
    title(sprintf('Channel %d - RMS | Detected %d/%d | SNR %.1f dB', ch, ...
        calibration.n_detected(ch), calibration.n_requested, calibration.mean_snr(ch)));
end
drawnow;
end


function print_review(calibration)
fprintf('\n========== CALIBRATION REVIEW ==========\n');
for ch = 1:2
    fprintf('\nChannel %d\n', ch);
    fprintf('----------------------------------------\n');
    fprintf('Contractions requested: %d\n', calibration.n_requested);
    fprintf('Contractions detected:  %d\n', calibration.n_detected(ch));
    fprintf('Rest RMS:               %.4f V\n', calibration.rest_rms(ch));
    fprintf('Contraction RMS:        %.4f V\n', calibration.mean_contr_rms(ch));
    fprintf('Threshold:              %.4f V\n', calibration.threshold(ch));
    fprintf('Mean SNR:               %.2f +/- %.2f dB\n', calibration.mean_snr(ch), calibration.std_snr(ch));
end
fprintf('\n========================================\n');
end


function choice = review_menu()
fprintf('\n--- REVIEW CALIBRATION ---\n');
fprintf('1 - Redo Channel 1\n');
fprintf('2 - Redo Channel 2\n');
fprintf('3 - Redo Both Channels\n');
fprintf('4 - Accept and Save\n');
choice = input('Choose an option (1-4): ');
if ~isscalar(choice)
    choice = 0;
end
end


function [calibration, saved] = save_calibration(calibration, folder, default_name)
% Asks for a name (dialog), refuses to overwrite, saves. saved = false if the dialog was cancelled.
if ~isfolder(folder)
    mkdir(folder);
end
saved = false;
while true
    answer = inputdlg('Enter a name for your calibration trial:', 'Save Calibration', [1 50], {default_name});
    if isempty(answer)
        return
    end
    trial_name = regexprep(strtrim(answer{1}), '[^a-zA-Z0-9_-]', '_');
    if isempty(trial_name)
        disp('Please enter a valid trial name.');
        continue
    end
    filename = fullfile(folder, [trial_name '.mat']);
    if isfile(filename)
        disp('A calibration with this name already exists. Please choose another name.');
        continue
    end
    break
end
calibration.name = trial_name;
save(filename, 'calibration');
fprintf('\nCalibration saved as: %s\n', filename);
saved = true;
end


function print_final_summary(calibration)
fprintf('\n========== FINAL CALIBRATION RESULTS ==========\n');
if isfield(calibration, 'name')
    fprintf('Calibration: %s\n', calibration.name);
end
for ch = 1:2
    fprintf('\nChannel %d:\n', ch);
    if isfield(calibration, 'n_requested')
        fprintf('Contractions requested: %d\n', calibration.n_requested);
    end
    fprintf('Contractions detected:  %d\n', calibration.n_detected(ch));
    fprintf('Rest RMS:               %.4f V\n', calibration.rest_rms(ch));
    fprintf('Contraction RMS:        %.4f V\n', calibration.mean_contr_rms(ch));
    fprintf('Threshold:              %.4f V\n', calibration.threshold(ch));
    fprintf('Mean SNR:               %.2f +/- %.2f dB\n', calibration.mean_snr(ch), calibration.std_snr(ch));
end
fprintf('\n===============================================\n');
end


function close_if_open(h)
if ~isempty(h) && isgraphics(h)
    close(h);
end
end
