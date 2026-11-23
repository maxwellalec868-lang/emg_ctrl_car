% Final Project (double channel control)
% - double-channel code must be flashed to arduino via ide
% - must also set baude to 230400

% setup two emg channels
% run section 0b to view live signals to ensure good fit
% run section 1a to calibrate and save (or load previous data)
% run section 1b if you ever want to update previously saved data
% select control in section 2, then run
% adjust sliders during binary and smoothed proportional as needed


%% Section 0A) Clear/Initialize
% - rerun this if you ever get a "no serial ports found error"
clearvars; close all; clc;
n_channels = 2;
fs = 1000;
disp("Cleared.")

%% Section 0B) View Live Data
% - run this section only if you want to verify a new emg hookup
% - all this does is display live signals so you can verify proper hookup
% - if/once you run this, may need to run section above before calibration

% Connect to SpikerShield--------------------------------------------------
ports = serialportlist("available");
assert(~isempty(ports), 'No serial ports found.');
emg = serialport(ports(1), 230400);
pause(2);
flush(emg);
disp('SpikerShields connected.');

% Initialize Live Plot-----------------------------------------------------
fig = figure('Name', 'Live EMG');
hold on; grid on;
colors = lines(n_channels);
traces = gobjects(1, n_channels);
for ch = 1:n_channels
    traces(ch) = animatedline('Color', colors(ch,:), 'MaximumNumPoints', 5000);
end

xlabel('Time (s)');
ylabel('EMG (V)');
title('Live SpikerShield EMG');
legend('Channel 1 (A0)', 'Channel 2 (A1)');
xlim([0 5]);
ylim([-3 3]);

% Read and Plot EMG--------------------------------------------------------
sample_count = 0;
serial_buffer = '';

while ishandle(fig)
    pause(0.02);
    emg_samples = zeros(0, n_channels);
    count = emg.NumBytesAvailable;
    if count > 0
        bytes = read(emg, count, "uint8");
        serial_buffer = [serial_buffer, char(bytes(:).')];
        rows = strsplit(serial_buffer, char(10));
        serial_buffer = rows{end};
        for k = 1:numel(rows)-1
            values = sscanf(rows{k}, '%d');
            if numel(values) == n_channels
                voltage = values(:)' / 1023 * 5 - 2.5;
                emg_samples(end+1,:) = voltage;
            end
        end
    end

    if isempty(emg_samples)
        drawnow limitrate;
        continue
    end
    n_samples = size(emg_samples, 1);
    t = (sample_count + (1:n_samples)) / fs;
    for ch = 1:n_channels
        addpoints(traces(ch), t, emg_samples(:,ch));
    end
    sample_count = sample_count + n_samples;
    if t(end) > 5
        xlim([t(end)-5, t(end)]);
    end
    drawnow limitrate;
end



%% Section 1A) Calibration
% - Perform new calibration or load previously saved data
% - Select number of contractions per trial
% - Review recordings and calculated values
% - Redo either channel, both channels, or accept and save

choice = input('1 = New calibration, 2 = Load saved calibration: ');
assert(ismember(choice, [1 2]), 'Choose 1 or 2.');
if choice == 1

    % Record Calibration Data
    fs = 1000;
    n_channels = 2;

    % Select number of contractions
    n_contractions = input('How many contractions per trial? ');
    validateattributes(n_contractions, {'numeric'}, {'scalar', 'integer', 'positive'});

    % Connect to SpikerShields---------------------------------------------
    ports = serialportlist("available");
    assert(~isempty(ports), 'No serial ports found.');
    pause(1);
    emg = serialport(ports(1), 230400);
    pause(2);
    flush(emg);
    disp('SpikerShields connected.');

    % Initialize Calibration Plot
    cal_fig = figure('Name', 'Live Calibration EMG');
    ax = axes(cal_fig);
    hold(ax, 'on'); grid(ax, 'on');
    traces = gobjects(1, 2);
    traces(1) = animatedline(ax, 'Color', 'b', 'MaximumNumPoints', 5000);
    traces(2) = animatedline(ax, 'Color', 'r', 'MaximumNumPoints', 5000);
    xlabel(ax, 'Time (s)');
    ylabel(ax, 'EMG (V)');
    legend(ax, 'Channel 1 (A0)', 'Channel 2 (A1)');
    xlim(ax, [0 5]);
    ylim(ax, [-3 3]);

    % Live plot information
    live.fig = cal_fig;
    live.ax = ax;
    live.traces = traces;
    live.sample_count = 0;
    live.fs = fs;

    % Record Resting EMG
    input('Relax both muscles. Press Enter to start: ', 's');
    [rest_data, live] = captureDualEMG(emg, 5, live, 'REST - Relax both muscles');
    assert(~isempty(rest_data), 'No resting EMG recorded. Restart calibration.');

    % Record Contractions and Review Results
    fist_data = cell(1, 2);
    channels_to_record = 1:2;
    accepted = false;
    while ~accepted

        % Record selected channels-----------------------------------------
        for ch = channels_to_record
            fprintf('\nCHANNEL %d: %d contractions\n', ch, n_contractions);
            input('Press Enter when ready: ', 's');
            figure(live.fig);

            % Replace previous recording for this channel
            fist_data{ch} = zeros(0, 2);
            for k = 1:n_contractions

                % Contract
                instruction = sprintf('Channel %d - %d/%d: CONTRACT', ch, k, n_contractions);
                fprintf('%s\n', instruction);
                [samples, live] = captureDualEMG(emg, 2, live, instruction);
                fist_data{ch} = [fist_data{ch}; samples];

                % Relax
                instruction = sprintf('Channel %d - %d/%d: RELAX', ch, k, n_contractions);
                fprintf('%s\n', instruction);
                [samples, live] = captureDualEMG(emg, 2, live, instruction);
                fist_data{ch} = [fist_data{ch}; samples];
            end
        end

        % Calculate Calibration Parameters
        calibration.fs = fs;
        calibration.n_channels = n_channels;
        calibration.n_requested = n_contractions;
        calibration.window = round(0.25 * fs);
        calibration.plateau_fraction = 0.5;
        calibration.raw_rest = rest_data;
        calibration.raw_fist = fist_data;

        % Initialize results
        calibration.threshold = nan(1, 2);
        calibration.rest_rms = nan(1, 2);
        calibration.mean_contr_rms = nan(1, 2);
        calibration.contraction_rms = cell(1, 2);
        calibration.snr = cell(1, 2);
        calibration.mean_snr = nan(1, 2);
        calibration.std_snr = nan(1, 2);
        calibration.n_detected = zeros(1, 2);

        for ch = 1:2
            rest = rest_data(:, ch);
            fist = fist_data{ch}(:, ch);

            % Moving RMS----------------------------------------------------
            rms_rest_moving = sqrt(movmean(rest.^2, calibration.window));
            rms_fist_moving = sqrt(movmean(fist.^2, calibration.window));

            % Rest-based threshold-----------------------------------------
            threshold = mean(rms_rest_moving) + 3*std(rms_rest_moving);
            rest_rms = sqrt(mean(rest.^2));
            calibration.threshold(ch) = threshold;
            calibration.rest_rms(ch) = rest_rms;

            % Detect contractions------------------------------------------
            active = rms_fist_moving > threshold;
            edges = diff([false; active; false]);
            starts = find(edges == 1);
            stops = find(edges == -1) - 1;

            % Handle undetected contractions
            if isempty(starts)
                warning('No contractions detected on Channel %d.', ch);
                calibration.contraction_rms{ch} = [];
                calibration.snr{ch} = [];
                calibration.n_detected(ch) = 0;
                continue
            end

            % Merge segments separated by 150 ms or less--------------------
            merged_starts = starts(1);
            merged_stops = stops(1);
            min_gap = round(0.15 * fs);
            for k = 2:numel(starts)
                if starts(k) - merged_stops(end) <= min_gap
                    merged_stops(end) = stops(k);
                else
                    merged_starts(end+1) = starts(k);
                    merged_stops(end+1) = stops(k);
                end
            end

            % Middle 50% contraction RMS-----------------------------------
            contraction_rms = zeros(numel(merged_starts), 1);
            for k = 1:numel(merged_starts)
                segment = fist(merged_starts(k):merged_stops(k));
                trim = floor(numel(segment) * (1-calibration.plateau_fraction)/2);

                % Avoid empty plateaus for short segments
                if 2*trim >= numel(segment)
                    trim = 0;
                end
                plateau = segment(1+trim:end-trim);
                contraction_rms(k) = sqrt(mean(plateau.^2));
            end

            % Calculate SNR------------------------------------------------
            if rest_rms > 0
                snr = 20*log10(contraction_rms / rest_rms);
            else
                snr = nan(size(contraction_rms));
                warning('Channel %d has zero resting RMS.', ch);
            end

            % Store channel-specific results-------------------------------
            calibration.contraction_rms{ch} = contraction_rms;
            calibration.mean_contr_rms(ch) = mean(contraction_rms);
            calibration.snr{ch} = snr;
            calibration.mean_snr(ch) = mean(snr);
            calibration.std_snr(ch) = std(snr);
            calibration.n_detected(ch) = numel(merged_starts);
            if numel(merged_starts) ~= n_contractions
                warning(['Channel %d: detected %d contractions, ' 'expected %d.'], ch, numel(merged_starts), n_contractions);
            end

        end

        % Display Recordings for Review
        review_fig = figure('Name', 'Calibration Review');
        for ch = 1:2
            signal = fist_data{ch}(:, ch);
            t = (0:length(signal)-1) / fs;

            % Raw EMG------------------------------------------------------
            subplot(2, 2, 2*ch-1);
            plot(t, signal);
            grid on;
            xlabel('Time (s)');
            ylabel('EMG (V)');
            title(sprintf('Channel %d - Raw EMG', ch));

            % Moving RMS---------------------------------------------------
            moving_rms = sqrt(movmean(signal.^2, calibration.window));
            subplot(2, 2, 2*ch);
            plot(t, moving_rms);
            hold on;
            yline(calibration.threshold(ch), 'k--', 'Threshold');
            grid on;
            xlabel('Time (s)');
            ylabel('RMS (V)');
            title(sprintf('Channel %d - RMS | Detected %d/%d | SNR %.1f dB', ch, calibration.n_detected(ch), n_contractions, calibration.mean_snr(ch)));
        end
        drawnow;

        % Display Calculated Values
        fprintf('\n========== CALIBRATION REVIEW ==========\n');
        for ch = 1:2
            fprintf('\nChannel %d\n', ch);
            fprintf('----------------------------------------\n');
            fprintf('Contractions requested: %d\n', n_contractions);
            fprintf('Contractions detected:  %d\n', calibration.n_detected(ch));
            fprintf('Rest RMS:               %.4f V\n', calibration.rest_rms(ch));
            fprintf('Contraction RMS:        %.4f V\n', calibration.mean_contr_rms(ch));
            fprintf('Threshold:              %.4f V\n', calibration.threshold(ch));
            fprintf('Mean SNR:               %.2f +/- %.2f dB\n', calibration.mean_snr(ch), calibration.std_snr(ch));
        end

        fprintf('\n========================================\n');

        % Prompt to Redo or Accept
        fprintf('\n--- REVIEW CALIBRATION ---\n');
        fprintf('1 - Redo Channel 1\n');
        fprintf('2 - Redo Channel 2\n');
        fprintf('3 - Redo Both Channels\n');
        fprintf('4 - Accept and Save\n');
        choice_review = input('Choose an option (1-4): ');

        switch choice_review
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

    % Disconnect
    clear emg;
    disp('Calibration recordings accepted.');

    % Save Calibration
    folder = fullfile(pwd, 'saved_data');

    if ~isfolder(folder)
        mkdir(folder);
    end

    % Prompt student to name trial
    while true
        answer = inputdlg('Enter a name for your calibration trial:', 'Save Calibration', [1 50], {'Calibration_01'});
        if isempty(answer)
            error('Calibration save cancelled.');
        end
        trial_name = strtrim(answer{1});

        % Replace unsupported filename characters
        trial_name = regexprep(trial_name, '[^a-zA-Z0-9_-]', '_');
        if isempty(trial_name)
            disp('Please enter a valid trial name.');
            continue
        end
        filename = fullfile(folder, [trial_name '.mat']);
        if isfile(filename)
            disp('A calibration with this name already exists.');
            disp('Please choose another name.');
            continue
        end
        break
    end

    % Save calibration data
    save(filename, 'calibration');
    fprintf('\nCalibration saved as: %s\n', filename);
else

    % Load Previous Calibration
    [file, path] = uigetfile('*.mat', 'Select saved calibration');
    assert(~isequal(file, 0), 'No file selected.');
    loaded = load(fullfile(path, file));
    assert(isfield(loaded, 'calibration'), 'Selected file does not contain a calibration structure.');
    calibration = loaded.calibration;
    assert(calibration.n_channels == 2 && calibration.fs == 1000, 'Saved calibration must be for two channels at 1000 Hz.');
    disp('Calibration loaded.');
end

% Display Final Calibration Summary
fprintf('\n========== FINAL CALIBRATION RESULTS ==========\n');
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


%% Section 1B) Revise Saved Calibration
% - Reload previous calibration after testing Control 01
% - Redo Channel 1, Channel 2, or both
% - Preserve recordings from channels not repeated
% - Review results and save revised calibration

revise = input('1 = Revise saved calibration, 2 = Continue to control: ');
assert(ismember(revise, [1 2]), 'Choose 1 or 2.');
if revise == 1

    % Load Previous Calibration
    folder = fullfile(pwd, 'saved_data');
    [file, path] = uigetfile(fullfile(folder, '*.mat'), 'Select calibration to revise');
    assert(~isequal(file, 0), 'No file selected.');
    loaded = load(fullfile(path, file));
    assert(isfield(loaded, 'calibration'), 'File does not contain calibration data.');
    calibration = loaded.calibration;

    % Ensure file contains the original recordings
    assert(calibration.n_channels == 2 && calibration.fs == 1000, 'Two-channel, 1000 Hz calibration required.');
    assert(isfield(calibration, 'raw_rest') && isfield(calibration, 'raw_fist') && iscell(calibration.raw_fist) && numel(calibration.raw_fist) == 2, 'Saved file must contain raw calibration recordings.');
    fs = calibration.fs;
    if isfield(calibration, 'n_requested')
        n_contractions = calibration.n_requested;
    else
        n_contractions = input('How many contractions per trial? ');
        validateattributes(n_contractions, {'numeric'}, {'scalar', 'integer', 'positive'});
        calibration.n_requested = n_contractions;
    end
    fprintf('\nLoaded: %s\n', file);
    revision_board = [];
    accepted = false;

    % Review and Reperform Loop
    while ~accepted
        % Display saved/current recordings-------------------------------
        review_fig = figure('Name', 'Calibration Review');
        for ch = 1:2
            signal = calibration.raw_fist{ch}(:, ch);
            t = (0:numel(signal)-1) / fs;
            % Raw EMG
            subplot(2, 2, 2*ch-1);
            plot(t, signal);
            grid on;
            xlabel('Time (s)');
            ylabel('EMG (V)');
            title(sprintf('Channel %d - Raw EMG', ch));
            % Moving RMS
            moving_rms = sqrt(movmean(signal.^2, calibration.window));
            subplot(2, 2, 2*ch);
            plot(t, moving_rms);
            hold on;
            yline(calibration.threshold(ch), 'k--', 'Threshold');
            grid on;
            xlabel('Time (s)');
            ylabel('RMS (V)');
            title(sprintf('Channel %d - Moving RMS', ch));
        end
        drawnow;

        % Display Calibration Values
        fprintf('\n========== CALIBRATION REVIEW ==========\n');
        for ch = 1:2
            fprintf('\nChannel %d\n', ch);
            fprintf('----------------------------------------\n');
            fprintf('Contractions requested: %d\n', n_contractions);
            fprintf('Contractions detected:  %d\n', calibration.n_detected(ch));
            fprintf('Rest RMS:               %.4f V\n', calibration.rest_rms(ch));
            fprintf('Contraction RMS:        %.4f V\n', calibration.mean_contr_rms(ch));
            fprintf('Threshold:              %.4f V\n', calibration.threshold(ch));
            fprintf('Mean SNR:               %.2f +/- %.2f dB\n', calibration.mean_snr(ch), calibration.std_snr(ch));
        end

        % Ask to Redo or Save
        fprintf('\n--- REVIEW CALIBRATION ---\n');
        fprintf('1 - Redo Channel 1\n');
        fprintf('2 - Redo Channel 2\n');
        fprintf('3 - Redo Both Channels\n');
        fprintf('4 - Accept and Save\n');
        choice_review = input('Choose an option (1-4): ');
        if ismember(choice_review, 1:3)

            % Select channels to repeat
            switch choice_review
                case 1
                    channels_to_record = 1;
                case 2
                    channels_to_record = 2;
                case 3
                    channels_to_record = 1:2;
            end
            if isgraphics(review_fig)
                close(review_fig);
            end

            % Connect Hardware (only when recording)
            if isempty(revision_board)
                ports = serialportlist("available");
                assert(~isempty(ports), 'No serial ports found.');
                revision_board = serialport(ports(1), 230400);
                pause(2);
                flush(revision_board);
                % Initialize live recording plot
                live.fig = figure('Name', 'Live Calibration EMG');
                live.ax = axes(live.fig);
                hold(live.ax, 'on');
                grid(live.ax, 'on');
                live.traces = gobjects(1, 2);
                live.traces(1) = animatedline(live.ax, 'Color', 'b', 'MaximumNumPoints', 5000);
                live.traces(2) = animatedline(live.ax, 'Color', 'r', 'MaximumNumPoints', 5000);
                xlabel(live.ax, 'Time (s)');
                ylabel(live.ax, 'EMG (V)');
                legend(live.ax, 'Channel 1', 'Channel 2');
                xlim(live.ax, [0 5]);
                ylim(live.ax, [-3 3]);
                live.sample_count = 0;
                live.fs = fs;
            end

            % Repeat Selected Trials
            for ch = channels_to_record
                fprintf('\nRedoing Channel %d\n', ch);
                input('Press Enter when ready: ', 's');
                figure(live.fig);
                new_fist = zeros(0, 2);
                for k = 1:n_contractions
                    % Contract
                    instruction = sprintf('Channel %d - %d/%d: CONTRACT', ch, k, n_contractions);
                    fprintf('%s\n', instruction);
                    [samples, live] = captureDualEMG(revision_board, 2, live, instruction);
                    new_fist = [new_fist; samples];
                    % Relax
                    instruction = sprintf( 'Channel %d - %d/%d: RELAX', ch, k, n_contractions);
                    fprintf('%s\n', instruction);
                    [samples, live] = captureDualEMG(revision_board, 2, live, instruction);
                    new_fist = [new_fist; samples];
                end

                % Replace only the selected channel's trial
                calibration.raw_fist{ch} = new_fist;

                % Recalculate this channel's calibration
                calibration = recalculateCalibrationChannel(calibration, ch);
            end
        elseif choice_review == 4

            % Save Revised Calibration
            if any(~isfinite(calibration.mean_contr_rms))
                disp('Invalid calibration values. Repeat affected trials.');
                if isgraphics(review_fig)
                    close(review_fig);
                end
                continue
            end
            [~, original_name] = fileparts(file);
            while true
                answer = inputdlg('Enter a name for the revised calibration:', 'Save Revised Calibration', [1 50], {[original_name '_revised']});
                if isempty(answer)
                    disp('Save canceled. Returning to review.');
                    break
                end
                trial_name = regexprep(strtrim(answer{1}), '[^a-zA-Z0-9_-]', '_');
                if isempty(trial_name)
                    disp('Please enter a valid name.');
                    continue
                end
                filename = fullfile(folder, [trial_name '.mat']);
                if isfile(filename)
                    disp('Filename already exists. Choose another.');
                    continue
                end
                save(filename, 'calibration');
                fprintf('\nRevised calibration saved: %s\n', filename);
                accepted = true;
                break
            end
            if ~accepted && isgraphics(review_fig)
                close(review_fig);
            end
        else
            disp('Invalid selection. Choose 1-4.');
            if isgraphics(review_fig)
                close(review_fig);
            end
        end
    end

    % Disconnect
    if ~isempty(revision_board)
        clear revision_board;
    end
    if exist('live', 'var') && isfield(live, 'fig')
        if isgraphics(live.fig)
            close(live.fig);
        end
    end
    disp('Calibration revision complete.');
end



%% Section 2) Live Control
% - Normalize both EMG channels using saved calibration
% - Select control algorithm
% - Display control outputs without MuJoCo
% - Close figure to stop

% Verify calibration and controller
assert(exist('calibration', 'var') == 1, 'Run or load calibration first.');
assert(calibration.n_channels == 2, 'Two-channel calibration required.');
assert(all(isfinite(calibration.mean_contr_rms)) && all(calibration.mean_contr_rms > calibration.rest_rms), 'Invalid contraction RMS. Repeat calibration.');
fs = calibration.fs;
window = calibration.window;

% Binary Controller Sensitivity
% 1.0 = Original calibrated threshold
% >1.0 = Less sensitive (requires stronger contraction)
% <1.0 = More sensitive (requires weaker contraction)
% binary_sensitivity = [1.0, 1.0]; % [Channel 1, Channel 2]

% Connect to SpikerShields
ports = serialportlist("available");
assert(~isempty(ports), 'No serial ports found.');
emg = serialport(ports(1), 230400);
pause(2);
flush(emg);
disp('SpikerShields connected. Starting Live Control.');

% Initialize Control Plot
control_fig = figure('Name', 'Live Control');
ax = axes(control_fig);
hold(ax, 'on'); grid(ax, 'on');
colors = lines(2);
control_traces = gobjects(1, 2);
for ch = 1:2
    control_traces(ch) = animatedline(ax, 'Color', colors(ch,:), 'LineWidth', 1.5, 'MaximumNumPoints', 500);
end

xlabel(ax, 'Time (s)');
ylabel(ax, 'Normalized Control Value');
legend(ax, 'Channel 1 (A0)', 'Channel 2 (A1)');
xlim(ax, [0 10]);
ylim(ax, [-0.1 1.1]);

% Adjustable Control 03B Parameters
% Sensitivity: higher = less sensitive
% Speed: higher = faster response, less smoothing
% Baseline: higher = greater resting-output correction

sensitivity_start = [1.00 1.00];
speed_start = [0.10 0.10];
baseline_start = [0.00 0.00];

% Make room beneath the plot for controls
ax.Position = [0.10 0.45 0.85 0.48];
dial_names = {'Sensitivity', 'Speed', 'Baseline'};
dial_limits = [0.50 2.50; 0.01 1.00; 0.00 0.90];
dial_initial = [sensitivity_start(1) speed_start(1) baseline_start(1);
                sensitivity_start(2) speed_start(2) baseline_start(2)];

dials = gobjects(2,3);
dial_readouts = gobjects(2,3);

for ch = 1:2
    x = 0.02 + (ch-1)*0.50;
    uicontrol(control_fig, 'Style', 'text', 'String', sprintf('Channel %d Adjustments', ch), 'Units', 'normalized', 'Position', [x 0.37 0.46 0.05]);

    for p = 1:3
        y = 0.28 - (p-1)*0.09;
        uicontrol(control_fig, 'Style', 'text', 'String', dial_names{p}, 'Units', 'normalized', 'Position', [x y 0.16 0.04]);
        dials(ch,p) = uicontrol(control_fig, 'Style', 'slider', 'Min', dial_limits(p,1), 'Max', dial_limits(p,2), 'Value', dial_initial(ch,p), 'Units', 'normalized', 'Position', [x+0.16 y 0.22 0.04]);
        dial_readouts(ch,p) = uicontrol(control_fig, 'Style', 'text', 'String', sprintf('%.2f', dial_initial(ch,p)), 'Units', 'normalized', 'Position', [x+0.39 y 0.07 0.04]);
    end
end

% % Live Binary Sensitivity Controls
% ax.Position = [0.12 0.34 0.82 0.60];
% binary_sliders = gobjects(1, 2);
% for ch = 1:2
%     y = 0.17 - (ch-1)*0.09;
%     uicontrol(control_fig, 'Style', 'text', 'String', sprintf('Channel %d Sensitivity', ch), 'Units', 'normalized', 'Position', [0.02 y 0.30 0.04]);
%     binary_sliders(ch) = uicontrol(control_fig, 'Style', 'slider', 'Min', 0.5, 'Max', 2.5, 'Value', binary_sensitivity(ch), 'Units', 'normalized', 'Position', [0.33 y 0.60 0.04]);
% end

% Initialize EMG Buffers
% Each channel receives its own buffer for the selected controller

emg_buffers = cell(1, 2);
for ch = 1:2
    emg_buffers{ch}.data = nan(fs*60, 1);
    emg_buffers{ch}.ptr = 1;
    emg_buffers{ch}.ptr_prev = 1;
end
serial_buffer = '';
control_values = [0 0];

% Reset smoothing history at the start of each control session
clear control03b

% Read, Normalize, and Display
while isgraphics(control_fig)
    pause(0.02);
    % Read serial data
    count = emg.NumBytesAvailable;
    if count == 0
        drawnow limitrate;
        continue
    end

    bytes = read(emg, count, "uint8");
    serial_buffer = [serial_buffer, char(bytes(:).')];
    rows = strsplit(serial_buffer, char(10));
    serial_buffer = rows{end};

    % Extract two-channel EMG samples
    batch = zeros(numel(rows)-1, 2);
    n = 0;
    for k = 1:numel(rows)-1
        values = sscanf(rows{k}, '%d');
        if numel(values) == 2 && all(values >= 0) && all(values <= 1023)
            n = n + 1;
            batch(n,:) = values(:)' / 1023 * 5 - 2.5;
        end
    end
    batch = batch(1:n,:);
    if isempty(batch)
        drawnow limitrate;
        continue
    end

    % Calculate Control Values for Each Channel
    for ch = 1:2
        buffer = emg_buffers{ch};
        % Add new EMG samples
        indices = buffer.ptr + (0:n-1);
        % Expand buffer when necessary
        if indices(end) > numel(buffer.data)
            buffer.data(end+1:indices(end)+fs*60) = NaN;
        end
        buffer.data(indices) = batch(:,ch);
        buffer.ptr = indices(end) + 1;
        % Calculate normalized control after first RMS window
        if buffer.ptr-1 >= window

            % Select channel-specific calibration parameters
            threshold = calibration.threshold(ch);
            rms_rest = calibration.rest_rms(ch);
            rms_fist = calibration.mean_contr_rms(ch);

            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            %%%%%%%%%%%%%%%%% YOUR CODE GOES HERE %%%%%%%%%%%%%%%%%%%%%

            % Select controller (comment/uncomment)

            % control_value = control01(buffer, window, threshold, rms_rest, rms_fist);
            % control_value = control02(buffer, window, threshold, rms_rest, rms_fist);

            % Smoothed Proportional Controller
            control_value = control03b(buffer, window, threshold, rms_rest, rms_fist, ch, dials(ch,1).Value, dials(ch,2).Value, dials(ch,3).Value);

            % Binary Controller - Adjustable Threshold
            % adjusted_threshold = threshold * dials(ch,1).Value;
            % control_value = control04(buffer, window, adjusted_threshold, rms_rest, rms_fist);

            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

            assert(isscalar(control_value) && isreal(control_value) && isfinite(control_value), 'Invalid control output from Channel %d.', ch);
            control_values(ch) = control_value;
        end
        buffer.ptr_prev = buffer.ptr;
        emg_buffers{ch} = buffer;
    end

        % Update Control Plot
        t = (emg_buffers{1}.ptr-1) / fs;
        for ch = 1:2
            addpoints(control_traces(ch), t, control_values(ch));
        end
        title(ax, sprintf('Channel 1: %.3f     |     Channel 2: %.3f', control_values(1), control_values(2)));
        if t > 10
            xlim(ax, [t-10, t]);
        end

        % Update dial value displays
        for ch = 1:2
            for p = 1:3
                dial_readouts(ch,p).String = sprintf('%.2f', dials(ch,p).Value);
            end
        end
        drawnow limitrate;
end

%% Disconnect
clear emg;
disp('Control 01 stopped. SpikerShields disconnected.');

%% Local Functions

function [samples, live] = captureDualEMG(board, seconds, live, instruction)

    % Update instruction above live plot
    title(live.ax, instruction);
    drawnow;
    flush(board);
    samples = zeros(ceil(seconds * 1200), 2);
    n = 0;
    buffer = '';
    timer = tic;

    while toc(timer) < seconds
        if ~isgraphics(live.fig)
            error('Calibration stopped: plot window closed.');
        end
        count = board.NumBytesAvailable;
        if count > 0
            bytes = read(board, count, "uint8");
            buffer = [buffer, char(bytes(:).')];
            rows = strsplit(buffer, char(10));
            buffer = rows{end};
            batch = zeros(numel(rows)-1, 2);
            nb = 0;
            for k = 1:numel(rows)-1
                values = sscanf(rows{k}, '%d');
                if numel(values) == 2 && all(values >= 0) && all(values <= 1023)
                    nb = nb + 1;
                    batch(nb,:) = values(:)' / 1023 * 5 - 2.5;
                end
            end
            batch = batch(1:nb,:);
            if nb > 0
                % Store recorded EMG
                if n + nb > size(samples, 1)
                    samples = [samples; zeros(1000 + nb, 2)];
                end
                samples(n+(1:nb),:) = batch;
                n = n + nb;

                % Update live plot
                t = (live.sample_count + (1:nb))' / live.fs;
                addpoints(live.traces(1), t, batch(:,1));
                addpoints(live.traces(2), t, batch(:,2));
                live.sample_count = live.sample_count + nb;
                if t(end) > 5
                    xlim(live.ax, [t(end)-5, t(end)]);
                end
                drawnow limitrate;
            end
        end
        pause(0.01);
    end
    samples = samples(1:n,:);
    if n < seconds * live.fs * 0.5
        warning('Only %d samples received in %.1f seconds.', n, seconds);
    end
end


function calibration = recalculateCalibrationChannel(calibration, ch)

    fs = calibration.fs;
    window = calibration.window;

    rest = calibration.raw_rest(:, ch);
    fist = calibration.raw_fist{ch}(:, ch);

    % Moving RMS
    rms_rest_moving = sqrt(movmean(rest.^2, window));
    rms_fist_moving = sqrt(movmean(fist.^2, window));

    % Rest-based threshold
    threshold = mean(rms_rest_moving) + 3*std(rms_rest_moving);
    rest_rms = sqrt(mean(rest.^2));

    calibration.threshold(ch) = threshold;
    calibration.rest_rms(ch) = rest_rms;

    % Detect contractions
    active = rms_fist_moving > threshold;
    edges = diff([false; active; false]);

    starts = find(edges == 1);
    stops = find(edges == -1) - 1;

    % Handle missing contractions
    if isempty(starts)
        calibration.contraction_rms{ch} = [];
        calibration.mean_contr_rms(ch) = NaN;
        calibration.snr{ch} = [];
        calibration.mean_snr(ch) = NaN;
        calibration.std_snr(ch) = NaN;
        calibration.n_detected(ch) = 0;

        warning('No contractions detected on Channel %d.', ch);
        return
    end

    % Merge nearby segments (150 ms)
    merged_starts = starts(1);
    merged_stops = stops(1);
    min_gap = round(0.15*fs);

    for k = 2:numel(starts)
        if starts(k) - merged_stops(end) <= min_gap
            merged_stops(end) = stops(k);
        else
            merged_starts(end+1) = starts(k);
            merged_stops(end+1) = stops(k);
        end
    end

    % Middle 50% contraction RMS
    contraction_rms = zeros(numel(merged_starts), 1);

    for k = 1:numel(merged_starts)
        segment = fist(merged_starts(k):merged_stops(k));

        trim = floor(numel(segment) * ...
            (1-calibration.plateau_fraction)/2);

        if 2*trim >= numel(segment)
            trim = 0;
        end

        plateau = segment(1+trim:end-trim);
        contraction_rms(k) = sqrt(mean(plateau.^2));
    end

    % SNR
    if rest_rms > 0
        snr = 20*log10(contraction_rms / rest_rms);
    else
        snr = nan(size(contraction_rms));
    end

    % Store results
    calibration.contraction_rms{ch} = contraction_rms;
    calibration.mean_contr_rms(ch) = mean(contraction_rms);
    calibration.snr{ch} = snr;
    calibration.mean_snr(ch) = mean(snr);
    calibration.std_snr(ch) = std(snr);
    calibration.n_detected(ch) = numel(merged_starts);

    if numel(merged_starts) ~= calibration.n_requested
        warning('Channel %d: detected %d contractions, expected %d.', ...
            ch, numel(merged_starts), calibration.n_requested);
    end
end


% %% Section 0C) Disconnect
% clear emg;
% disp('SpikerShields disconnected.');