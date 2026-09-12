function stm32_swv_tcp_reader()
% STM32_SWV_TCP_READER
% Reads MPU6050 (accel/gyro) + HMC5883L (mag) values printed over SWV/ITM
% via printf, through an OpenOCD SWO-to-TCP bridge. Plots each sensor in
% its own figure and writes all captured samples to a CSV file whenever
% the script stops (Ctrl+C, error, or normal exit).
%
% Expects firmware printf lines of the form:
%   MAG X: %d, Y: %d, Z: %d | ACCEL X: %d, Y: %d, Z: %d | GYRO X: %d, Y: %d, Z: %d\r\n
%
% Assumes printf is retargeted to ITM_SendChar on the ITM stimulus port
% given by targetStimPort below (commonly port 0). If your retarget
% implementation uses a different port, or packs multiple characters per
% ITM write, this should still work since the decoder appends raw
% payload bytes in packet order rather than interpreting them as numbers.
%
% NOTE: exact OpenOCD "tpiu config" syntax is version-dependent; verify
% against your OpenOCD version (run "tpiu config" with no args in the
% OpenOCD console) before relying on the command line you use to start it.

%% --- User settings ---
close all
host = '127.0.0.1';
port = 3443;
targetStimPort = 0;     % ITM stimulus port printf/ITM_SendChar writes to
readTimeout = 5;        % seconds, for the initial TCP connection
rollingWindow = 300;    % samples shown on the live plots (CSV keeps everything)
csvFilename = fullfile(pwd, 'imu_mag_log.csv');
autosaveEvery = 100;    % write the CSV every N samples as a safety net

%% --- Data storage (grows for the whole session; saved to CSV at the end) ---
sampleIdx = 0;
magX = []; magY = []; magZ = [];
accX = []; accY = []; accZ = [];
gyroX = []; gyroY = []; gyroZ = [];
timestamps = [];

%% --- Connect ---
try
    t = tcpclient(host, port, "Timeout", readTimeout);
    disp("Connected to OpenOCD SWO TCP server.");
catch ME
    error("Could not connect to %s:%d. Is OpenOCD running with a tpiu TCP server on this port? Details: %s", ...
        host, port, ME.message);
end

% onCleanup fires when this function's workspace is torn down, including
% on Ctrl+C, so the CSV gets written no matter how the script stops.
% This only works reliably because saveCSV is a NESTED function (shares
% this function's workspace) rather than a separate local function.
cleanupObj = onCleanup(@saveCSV); %#ok<NASGU>

%% --- Line parsing setup ---
lineBuffer = '';
pattern = ['MAG X:\s*(-?\d+),\s*Y:\s*(-?\d+),\s*Z:\s*(-?\d+)\s*\|\s*' ...
           'ACCEL X:\s*(-?\d+),\s*Y:\s*(-?\d+),\s*Z:\s*(-?\d+)\s*\|\s*' ...
           'GYRO X:\s*(-?\d+),\s*Y:\s*(-?\d+),\s*Z:\s*(-?\d+)'];

%% --- Figures: one per sensor ---
stopRequested = false;

figAccel = figure('Name', 'Accelerometer');
axAccel = axes(figAccel); hold(axAccel, 'on'); grid(axAccel, 'on');
title(axAccel, 'MPU6050 Accelerometer'); xlabel(axAccel, 'Sample'); ylabel(axAccel, 'Raw value');
hAx = plot(axAccel, nan, nan, '-', 'DisplayName', 'X');
hAy = plot(axAccel, nan, nan, '-', 'DisplayName', 'Y');
hAz = plot(axAccel, nan, nan, '-', 'DisplayName', 'Z');
legend(axAccel);

% Preferred way to stop: this button exits the read loop normally, so
% onCleanup/saveCSV runs cleanly with no interrupt involved. Ctrl+C still
% works as a fallback, but isn't guaranteed to save cleanly every time.
uicontrol(figAccel, 'Style', 'pushbutton', 'String', 'Stop & Save', ...
    'Units', 'normalized', 'Position', [0.02 0.02 0.15 0.06], ...
    'Callback', @stopButtonCallback);

figGyro = figure('Name', 'Gyroscope');
axGyro = axes(figGyro); hold(axGyro, 'on'); grid(axGyro, 'on');
title(axGyro, 'MPU6050 Gyroscope'); xlabel(axGyro, 'Sample'); ylabel(axGyro, 'Raw value');
hGx = plot(axGyro, nan, nan, '-', 'DisplayName', 'X');
hGy = plot(axGyro, nan, nan, '-', 'DisplayName', 'Y');
hGz = plot(axGyro, nan, nan, '-', 'DisplayName', 'Z');
legend(axGyro);

figMag = figure('Name', 'Magnetometer');
axMag = axes(figMag); hold(axMag, 'on'); grid(axMag, 'on');
title(axMag, 'HMC5883L Magnetometer'); xlabel(axMag, 'Sample'); ylabel(axMag, 'Raw value');
hMx = plot(axMag, nan, nan, '-', 'DisplayName', 'X');
hMy = plot(axMag, nan, nan, '-', 'DisplayName', 'Y');
hMz = plot(axMag, nan, nan, '-', 'DisplayName', 'Z');
legend(axMag);

%% --- Live read + decode + parse loop ---
buffer = uint8.empty;
disp("Reading SWO stream. Click 'Stop & Save' to stop cleanly (or Ctrl+C as a fallback).");

while ~stopRequested
    if t.NumBytesAvailable > 0
        newBytes = read(t, t.NumBytesAvailable, "uint8");
        buffer = [buffer, newBytes]; %#ok<AGROW>
    end

    [chars, buffer] = decodeSWIT(buffer, targetStimPort);

    if ~isempty(chars)
        lineBuffer = [lineBuffer, char(chars)]; %#ok<AGROW>

        % Split off complete lines (terminated by \n; strip any \r too)
        while true
            nlPos = find(lineBuffer == newline, 1);
            if isempty(nlPos)
                break;
            end
            rawLine = lineBuffer(1:nlPos-1);
            lineBuffer = lineBuffer(nlPos+1:end);
            rawLine = strrep(rawLine, sprintf('\r'), '');
            processLine(rawLine);
        end
    end

    pause(0.01);
end

%% --- Nested functions (these share this function's workspace) ---
    function processLine(lineStr)
        tokens = regexp(lineStr, pattern, 'tokens', 'once');
        if isempty(tokens)
            return; % not a matching line (e.g. a boot/log message) -- skip it
        end

        vals = str2double(tokens);
        if any(isnan(vals))
            return; % malformed/partial line -- skip it
        end

        sampleIdx = sampleIdx + 1;
        timestamps(end+1) = sampleIdx; %#ok<AGROW>

        magX(end+1) = vals(1); magY(end+1) = vals(2); magZ(end+1) = vals(3); %#ok<AGROW>
        accX(end+1) = vals(4); accY(end+1) = vals(5); accZ(end+1) = vals(6); %#ok<AGROW>
        gyroX(end+1) = vals(7); gyroY(end+1) = vals(8); gyroZ(end+1) = vals(9); %#ok<AGROW>

        updatePlots();

        if mod(sampleIdx, autosaveEvery) == 0
            saveCSV();   % periodic safety-net save; overwrites csvFilename
        end
    end

    function stopButtonCallback(~, ~)
        stopRequested = true;
    end

    function updatePlots()
        n = numel(timestamps);
        idxStart = max(1, n - rollingWindow + 1);
        idx = idxStart:n;

        set(hAx, 'XData', timestamps(idx), 'YData', accX(idx));
        set(hAy, 'XData', timestamps(idx), 'YData', accY(idx));
        set(hAz, 'XData', timestamps(idx), 'YData', accZ(idx));

        set(hGx, 'XData', timestamps(idx), 'YData', gyroX(idx));
        set(hGy, 'XData', timestamps(idx), 'YData', gyroY(idx));
        set(hGz, 'XData', timestamps(idx), 'YData', gyroZ(idx));

        set(hMx, 'XData', timestamps(idx), 'YData', magX(idx));
        set(hMy, 'XData', timestamps(idx), 'YData', magY(idx));
        set(hMz, 'XData', timestamps(idx), 'YData', magZ(idx));

        drawnow limitrate;
    end

    function saveCSV()
        if isempty(timestamps)
            disp('No samples captured -- nothing to save.');
            return;
        end
        T = table(timestamps', magX', magY', magZ', accX', accY', accZ', gyroX', gyroY', gyroZ', ...
            'VariableNames', {'Sample','MagX','MagY','MagZ','AccelX','AccelY','AccelZ','GyroX','GyroY','GyroZ'});
        writetable(T, csvFilename);
        fprintf('Saved %d samples to %s\n', numel(timestamps), csvFilename);
    end

    function [chars, remaining] = decodeSWIT(buf, wantedPort)
        % Same SWIT packet framing as before (see ARM CoreSight ITM
        % architecture), but this returns raw payload bytes as
        % characters in packet order, rather than interpreting them as
        % a single numeric value, since this stream carries ASCII text.
        chars = uint8.empty;
        i = 1;
        n2 = numel(buf);

        while i <= n2
            header = buf(i);
            isSWIT   = bitget(header, 3) == 0;   % bit 2 == 0 -> software source
            sizeCode = bitand(header, 3);         % bits [1:0]
            stimPort = bitshift(header, -3);      % bits [7:3]

            if ~isSWIT || sizeCode == 0 || stimPort ~= wantedPort
                i = i + 1;   % not a recognized packet for our port, skip a byte
                continue;
            end

            switch sizeCode
                case 1, payloadLen = 1;
                case 2, payloadLen = 2;
                case 3, payloadLen = 4;
            end

            if i + payloadLen > n2
                break;   % incomplete packet, wait for more bytes next read
            end

            chars = [chars, buf(i+1 : i+payloadLen)]; %#ok<AGROW>
            i = i + 1 + payloadLen;
        end

        remaining = buf(i:end);
    end

end