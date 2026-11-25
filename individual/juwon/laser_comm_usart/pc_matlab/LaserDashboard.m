function fig = LaserDashboard()
% LASERDASHBOARD  레이저 사격 시스템 PC 대시보드 (MATLAB, 코드로 만든 UI)
%
%   실행: 이 폴더에서  >> LaserDashboard
%
%   - 수업 HealthMonitor 앱 구조(포트 선택 -> Connect -> 실시간 그래프 -> write로 MCU에 명령)를 따름
%   - 단, while(1) 대신 timer로 50ms마다 받은 바이트를 꺼내므로 Stop/창 닫기가 정상 동작
%   - 필요 파일: ls_parse_frames.m, ls_crc8.m, ls_build_frame.m (같은 폴더)
%   - 포트 목록의 '(데모: 가짜 MCU)'를 고르면 보드 없이 화면을 시연할 수 있음

%% ---------------- 설정값 ----------------
BAUD        = 115200;
FS          = 100;          % MCU 프레임 주기 100Hz
WIN_SEC     = 5;            % ADC 그래프에 보여줄 시간 [s]
MIRROR_LR   = false;        % 센서 번호가 사수 시점과 좌우 반대면 true
HIT_SHOW_S  = 0.7;          % 피격 칸을 빨갛게 유지할 시간 [s]
STATE_NAMES = {'IDLE', 'PATROL', 'HIT', 'DOWN', 'RECOVER', 'GAME OVER'};

%% ---------------- 상태 변수 (중첩 함수가 공유) ----------------
sp       = [];              % serialport 객체
tmr      = [];              % 수신 timer
rxBuf    = [];              % 수신 바이트 조각
nWin     = FS * WIN_SEC;
adcHist  = nan(nWin, 9);    % ADC 순환 버퍼
tHist    = nan(nWin, 1);
nRx = 0; nBad = 0; nLost = 0;
lastSeq  = [];
lastHit  = [];              % 마지막으로 본 HitCnt
hitLog   = table('Size', [0 5], ...
                 'VariableTypes', {'datetime', 'double', 'double', 'double', 'double'}, ...
                 'VariableNames', {'Time', 'Seq', 'HitNo', 'Zone', 'ReactionMs'});
hitOnTime = [];             % 피격 칸 표시 시작 시각
hitZone   = 0;
t0 = tic;
DEMO_PORT = '(데모: 가짜 MCU)';
demo = struct('on', false, 'seq', 0, 'state', 0, 'tState', 0, 'hit', 0, 'zone', 0, ...
              'rt', 0, 'speed', 3, 'ms', 0, 'next', 2000);

%% ---------------- UI ----------------
fig = uifigure('Name', 'Laser Shooting Dashboard', 'Position', [100 100 1180 700]);
fig.CloseRequestFcn = @onClose;
fig.DeleteFcn = @(~,~) stopTimer();     % 어떤 방식으로 창이 없어져도 timer 정리
gl = uigridlayout(fig, [2 3]);
gl.ColumnWidth = {260, 330, '1x'};
gl.RowHeight   = {'1x', '1x'};

% --- 왼쪽: 연결 / 게임 제어 ---
pCtl = uipanel(gl, 'Title', '연결 / 제어');
pCtl.Layout.Row = [1 2]; pCtl.Layout.Column = 1;
g1 = uigridlayout(pCtl, [11 2]);
g1.RowHeight = repmat({30}, 1, 11);
g1.ColumnWidth = {'1x', '1x'};

place(uilabel(g1, 'Text', 'Port'), 1, 1);
ddPort = place(uidropdown(g1, 'Items', {'(없음)'}), 1, 2);
place(uibutton(g1, 'Text', '포트 새로고침', 'ButtonPushedFcn', @(~,~) refreshPorts()), 2, [1 2]);
btnConn = place(uibutton(g1, 'Text', 'Connect', 'ButtonPushedFcn', @(~,~) onConnect()), 3, 1);
lamp = place(uilamp(g1, 'Color', [0.6 0.6 0.6]), 3, 2);

btnStart = place(uibutton(g1, 'Text', '게임 시작 (S)', 'Enable', 'off', ...
                 'ButtonPushedFcn', @(~,~) sendCmd('S')), 4, 1);
btnEnd   = place(uibutton(g1, 'Text', '게임 종료 (E)', 'Enable', 'off', ...
                 'ButtonPushedFcn', @(~,~) sendCmd('E')), 4, 2);
place(uilabel(g1, 'Text', '순찰 속도'), 5, 1);
ddSpeed = place(uidropdown(g1, 'Items', {'1', '2', '3', '4', '5'}, 'Value', '3', 'Enable', 'off', ...
                'ValueChangedFcn', @(src,~) sendCmd(src.Value)), 5, 2);
place(uibutton(g1, 'Text', '기록 초기화', 'ButtonPushedFcn', @(~,~) resetSession()), 6, 1);
place(uibutton(g1, 'Text', 'CSV 저장', 'ButtonPushedFcn', @(~,~) saveCsv()), 6, 2);

lblState = place(uilabel(g1, 'Text', '상태: -', 'FontWeight', 'bold', 'FontSize', 15), 7, [1 2]);
lblScore = place(uilabel(g1, 'Text', '명중: 0', 'FontSize', 15), 8, [1 2]);
lblRt    = place(uilabel(g1, 'Text', '최근 반응: - ms', 'FontSize', 15), 9, [1 2]);
lblAvg   = place(uilabel(g1, 'Text', '평균 반응: - ms', 'FontSize', 15), 10, [1 2]);
lblStat  = place(uilabel(g1, 'Text', 'RX 0 / CRC오류 0 / 유실 0', 'FontColor', [0.4 0.4 0.4]), 11, [1 2]);

% --- 가운데 위: 3x3 조준경 ---
pAim = uipanel(gl, 'Title', '조준경 (사수 시점)');
pAim.Layout.Row = 1; pAim.Layout.Column = 2;
gAim = uigridlayout(pAim, [3 3], 'RowSpacing', 4, 'ColumnSpacing', 4);
cells = gobjects(1, 9);
for z = 1:9
    cells(z) = uilabel(gAim, 'Text', num2str(z), 'FontSize', 26, 'FontWeight', 'bold', ...
        'HorizontalAlignment', 'center', 'BackgroundColor', [0.92 0.92 0.92], ...
        'FontColor', [0.25 0.25 0.25]);
    [r, c] = zoneToCell(z);
    cells(z).Layout.Row = r; cells(z).Layout.Column = c;
end

% --- 가운데 아래: 탄착 분포 (구역별 명중 횟수) ---
axHeat = uiaxes(gl);
axHeat.Layout.Row = 2; axHeat.Layout.Column = 2;
title(axHeat, '탄착군 분포 (구역별 명중 수)');
heatImg = imagesc(axHeat, zeros(3, 3));
axis(axHeat, 'image'); axHeat.XTick = []; axHeat.YTick = [];
colormap(axHeat, [linspace(1, 0.85, 64)', linspace(1, 0.1, 64)', linspace(1, 0.1, 64)']);
heatTxt = gobjects(3, 3);
for r = 1:3
    for c = 1:3
        heatTxt(r, c) = text(axHeat, c, r, '0', 'HorizontalAlignment', 'center', ...
                             'FontSize', 14, 'Color', [0.1 0.1 0.1]);
    end
end

% --- 오른쪽 위: ADC 9채널 ---
axAdc = uiaxes(gl);
axAdc.Layout.Row = 1; axAdc.Layout.Column = 3;
title(axAdc, 'CdS ADC 원시값 (9채널)'); xlabel(axAdc, 'time [s]'); ylabel(axAdc, 'ADC (0~4095)');
ylim(axAdc, [0 4095]); hold(axAdc, 'on'); grid(axAdc, 'on');
adcLines = gobjects(1, 9);
for ch = 1:9
    adcLines(ch) = plot(axAdc, NaN, NaN, 'DisplayName', sprintf('CH%d', ch));
end
legend(axAdc, 'NumColumns', 3, 'Location', 'northoutside');

% --- 오른쪽 아래: 반응속도 누적 ---
axRt = uiaxes(gl);
axRt.Layout.Row = 2; axRt.Layout.Column = 3;
title(axRt, '반응속도 추이'); xlabel(axRt, '피격 번호'); ylabel(axRt, 'ms'); grid(axRt, 'on');
rtLine = plot(axRt, NaN, NaN, '-o', 'LineWidth', 1.5);

refreshPorts();

%% ================= 중첩 함수 =================
    function h = place(h, r, c)
        h.Layout.Row = r; h.Layout.Column = c;
    end

    function [r, c] = zoneToCell(z)
        r = ceil(z / 3);
        c = mod(z - 1, 3) + 1;
        if MIRROR_LR, c = 4 - c; end
    end

    function refreshPorts()
        p = cellstr(serialportlist("available"));
        ddPort.Items = [p(:)', {DEMO_PORT}];
    end

    function onConnect()
        if isempty(sp)
            port = ddPort.Value;
            if strcmp(port, DEMO_PORT)
                sp = 'demo';
                demo.on = true; demo.ms = 0; demo.state = 0; demo.tState = 0;
            else
                try
                    sp = serialport(port, BAUD);
                    flush(sp);
                catch err
                    sp = [];
                    uialert(fig, err.message, '연결 실패'); return;
                end
            end
            lastSeq = []; lastHit = [];
            rxBuf = [];
            tmr = timer('ExecutionMode', 'fixedSpacing', 'Period', 0.05, ...
                        'BusyMode', 'drop', 'TimerFcn', @(~,~) onTick());
            start(tmr);
            lamp.Color = [0 0.8 0];
            btnConn.Text = 'Disconnect';
            set([btnStart btnEnd ddSpeed], 'Enable', 'on');
        else
            disconnect();
        end
    end

    function stopTimer()
        if ~isempty(tmr) && isvalid(tmr), stop(tmr); delete(tmr); end
        tmr = [];
    end

    function disconnect()
        stopTimer();
        if ~isempty(sp) && ~ischar(sp), try, delete(sp); catch, end, end
        sp = [];
        demo.on = false;
        lamp.Color = [0.6 0.6 0.6];
        btnConn.Text = 'Connect';
        set([btnStart btnEnd ddSpeed], 'Enable', 'off');
    end

    function sendCmd(ch)
        if isempty(sp), return; end
        if demo.on, demoCommand(ch(1)); return; end
        write(sp, uint8(ch(1)), "uint8");     % 수업과 같은 1바이트 명령
    end

    % ---------- 50ms마다: 받은 바이트 처리 ----------
    function onTick()
        if ~isvalid(fig), stopTimer(); return; end
        try
            if demo.on
                rxBuf = [rxBuf, demoBytes()];
            else
                n = sp.NumBytesAvailable;
                if n > 0
                    rxBuf = [rxBuf, double(read(sp, n, "uint8"))];
                end
            end
            [frames, rxBuf, bad] = ls_parse_frames(rxBuf);
            nBad = nBad + bad;
            for f = frames
                handleFrame(f);
            end
            updateView(~isempty(frames));
        catch err
            disconnect();
            uialert(fig, err.message, '통신 오류');
        end
    end

    function handleFrame(f)
        nRx = nRx + 1;
        % 유실 패킷: Seq가 1씩 증가하지 않은 만큼
        if ~isempty(lastSeq)
            nLost = nLost + mod(f.seq - lastSeq - 1, 256);
        end
        lastSeq = f.seq;

        % ADC 순환 버퍼
        adcHist = [adcHist(2:end, :); f.adc];
        tHist   = [tHist(2:end); toc(t0)];

        % 상태
        if f.state + 1 <= numel(STATE_NAMES)
            lblState.Text = ['상태: ' STATE_NAMES{f.state + 1}];
        end

        % 새 피격? (HitCnt가 바뀌었을 때만)
        if isempty(lastHit)
            lastHit = f.hitCnt;          % 연결 직후 첫 프레임은 기준값으로만 사용
        elseif f.hitCnt ~= lastHit
            lastHit = f.hitCnt;
            if f.zone >= 1 && f.zone <= 9
                hitLog(end + 1, :) = {datetime('now'), f.seq, height(hitLog) + 1, f.zone, f.rt};
                hitZone = f.zone;
                hitOnTime = tic;
            end
        end
    end

    function updateView(gotData)
        if gotData
            for k = 1:9
                set(adcLines(k), 'XData', tHist, 'YData', adcHist(:, k));
            end
            tv = tHist(~isnan(tHist));
            if numel(tv) > 1, xlim(axAdc, [tv(1) tv(end)]); end
        end

        % 조준경 칸 색
        for k = 1:9
            cells(k).BackgroundColor = [0.92 0.92 0.92];
        end
        if hitZone > 0 && ~isempty(hitOnTime) && toc(hitOnTime) < HIT_SHOW_S
            cells(hitZone).BackgroundColor = [0.9 0.15 0.15];
        end

        % 점수 / 반응속도 / 탄착 분포
        nHit = height(hitLog);
        lblScore.Text = sprintf('명중: %d', nHit);
        if nHit > 0
            lblRt.Text  = sprintf('최근 반응: %d ms', hitLog.ReactionMs(end));
            lblAvg.Text = sprintf('평균 반응: %.0f ms', mean(hitLog.ReactionMs));
            set(rtLine, 'XData', hitLog.HitNo, 'YData', hitLog.ReactionMs);
        end
        cnt = zeros(3, 3);
        for k = 1:nHit
            [r, c] = zoneToCell(hitLog.Zone(k));
            cnt(r, c) = cnt(r, c) + 1;
        end
        heatImg.CData = cnt;
        clim(axHeat, [0 max(1, max(cnt(:)))]);
        for r = 1:3
            for c = 1:3
                heatTxt(r, c).String = num2str(cnt(r, c));
            end
        end

        lblStat.Text = sprintf('RX %d / CRC오류 %d / 유실 %d', nRx, nBad, nLost);
        drawnow limitrate;
    end

    function resetSession()
        hitLog = hitLog([], :);
        nRx = 0; nBad = 0; nLost = 0; lastSeq = [];
        hitZone = 0;
        set(rtLine, 'XData', NaN, 'YData', NaN);
        updateView(false);
    end

    function saveCsv()
        name = sprintf('shot_log_%s.csv', string(datetime('now', 'Format', 'yyyyMMdd_HHmmss')));
        [file, path] = uiputfile('*.csv', 'CSV 저장', name);
        if isequal(file, 0), return; end
        writetable(hitLog, fullfile(path, file));
    end

    % ---------- 데모 모드: main_comm_test.c의 가짜 게임과 같은 동작 ----------
    function demoCommand(c)
        switch c
            case 'S', demoSet(1);
            case 'E', demoSet(5);
            otherwise
                if c >= '1' && c <= '5', demo.speed = c - '0'; end
        end
    end

    function demoSet(st)
        demo.state = st; demo.tState = demo.ms;
        if st == 1, demo.next = 1000 + 250 * demo.speed + randi(800); end   % 반응속도에 변화 주기
    end

    function bytes = demoBytes()
        % 실제 경과 시간만큼 10ms 프레임 생성
        target = floor(toc(t0) * 1000 / 10) * 10;
        if demo.ms == 0, demo.ms = target - 50; end
        bytes = [];
        while demo.ms < target
            demo.ms = demo.ms + 10;
            dt = demo.ms - demo.tState;
            switch demo.state
                case 1                                   % PATROL
                    if dt >= demo.next
                        demo.zone = randi(9); demo.rt = dt; demo.hit = demo.hit + 1;
                        demoSet(2);
                    end
                case 2, if dt >= 100,  demoSet(3); end   % HIT -> DOWN
                case 3, if dt >= 1500, demoSet(4); end   % DOWN -> RECOVER
                case 4, if dt >= 300,  demoSet(1); end   % RECOVER -> PATROL
            end
            ph  = mod(demo.ms / 10 + (0:8) * 11, 100);
            adc = 1000 + min(ph, 100 - ph) * 8;
            if demo.state == 2 && demo.zone > 0, adc(demo.zone) = 3800; end
            demo.seq = mod(demo.seq + 1, 256);
            bytes = [bytes, ls_build_frame(demo.seq, demo.state, demo.hit, demo.zone, demo.rt, adc)]; %#ok<AGROW>
        end
    end

    function onClose(~, ~)
        disconnect();
        delete(fig);
    end
end
