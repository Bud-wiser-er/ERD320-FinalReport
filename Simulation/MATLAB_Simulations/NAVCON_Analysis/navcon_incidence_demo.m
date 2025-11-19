function navcon_car_vs_moving_block_fixed()
% NAVCON demo: car drives at heading phi; horizontal block TRANSLATES a set distance then stops.
% FIXED VERSION with improved car movement visualization

clc; close all;

%% ---------------- PARAMETERS ----------------
% Geometry & logic
B                    = 40;        % mm, L↔R sensor spacing
forward_window_mm    = B + 10;    % one-outer-only inference window
car_len              = 28;        % mm, for drawing
car_wid              = B + 20;    % mm, for drawing

% Car motion (distance along heading per frame)
angles_deg           = [15 30 45 60 75];  % test headings (0° = +y)
ds_car_mm            = 1.0;       % mm/frame (REDUCED for better visibility)
s_start              = -30;       % mm (start further back)
s_end_extra          = 100;       % mm (run past block)

% Block (maze line), initially horizontal
block_center0        = [0, 60];   % (x0, y0) initial center
block_len_mm         = 240;       % mm
block_thick_mm       = 10;        % mm

% Block TRANSLATION
block_dir_deg        = 0;         % 0=+x, 90=+y, 180=-x, 270=-y (world frame)
block_speed_mm_frame = 0.8;       % mm per frame (REDUCED for better visibility)
block_travel_limit_mm= 80;        % mm; block stops after this distance

% Animation control
frame_delay          = 0.05;      % seconds between frames
%% -------------------------------------------

% Convenience vectors for sensors in car frame
xL = -B/2; xC = 0; xR = +B/2;   % lateral (right=+)
ofs = [xL,0; xC,0; xR,0];

for phi = angles_deg
    % Car heading (world): forward u, right r
    u = [sind(phi), cosd(phi)];      % +y at 0°
    r = [cosd(phi), -sind(phi)];

    % Place car so that at s = L0 the centre sensor is near the (initial) block centre
    L0 = 90;                          % mm along the path
    pC0 = block_center0 - L0*u;       % centre sensor world position at s=0
    p0  = pC0;                         % use centre-sensor reference

    % Block motion setup
    vb_dir = [cosd(block_dir_deg), sind(block_dir_deg)]; % unit direction
    block_travel = 0;                                     % mm traveled so far
    block_center = block_center0;

    % Figure setup
    fig = figure('Color','w','Name',sprintf('Car vs MOVING block (phi=%.1f°)',phi),...
        'Position',[120 80 1200 800]);
    ax = axes('Parent',fig); hold(ax,'on'); grid(ax,'on');
    title(ax, sprintf('Heading \\phi=%.1f° | Block moves %g mm @ %g mm/frame along %g°', ...
        phi, block_travel_limit_mm, block_speed_mm_frame, block_dir_deg), 'FontWeight','bold');
    xlabel(ax,'x (mm)'); ylabel(ax,'y (mm)');
    xlim(ax, [-200, 200]); ylim(ax, [-20, 220]);
    axis(ax, 'equal'); % Keep aspect ratio square

    % Draw (dynamic) block patch
    [xl,xr,yb,yt] = block_bounds(block_center, block_len_mm, block_thick_mm);
    hBlock = patch('XData',[xl xr xr xl],'YData',[yb yb yt yt], ...
                   'FaceColor',[0.3 0.3 0.3],'FaceAlpha',0.7,'EdgeColor',[0 0 0],'LineWidth',2);

    % Car initial draw
    s = s_start; s_end = L0 + s_end_extra;
    [carPoly, hL, hC, hR] = draw_car(ax, p0, r, u, ofs, s, car_len, car_wid);

    % Add trajectory line for visualization
    s_traj = s_start:5:s_end;
    traj_points = zeros(length(s_traj), 2);
    for i = 1:length(s_traj)
        traj_points(i,:) = p0 + s_traj(i)*u;
    end
    hTraj = plot(ax, traj_points(:,1), traj_points(:,2), 'r--', 'LineWidth', 1);
    hTraj.Color(4) = 0.5; % Set alpha transparency

    % Status box (made larger and more visible)
    status = annotation('textbox',[.02 .55 .45 .40],'String','', ...
        'FontName','Consolas','FontSize',10,'BackgroundColor',[0.95 0.95 1], ...
        'EdgeColor',[0.3 0.3 0.7],'LineWidth',1.5);

    % Hit bookkeeping
    hitL=false; hitC=false; hitR=false;
    odoL=NaN;   odoC=NaN;   odoR=NaN;

    % Animation loop with better frame control
    tic; % Start timing
    frame_count = 0;
    while ishandle(fig) && s <= s_end
        frame_count = frame_count + 1;
        
        % 1) Move CAR
        s = s + ds_car_mm;

        % 2) Move BLOCK until travel limit reached
        if block_travel < block_travel_limit_mm
            step = min(block_speed_mm_frame, block_travel_limit_mm - block_travel);
            block_center = block_center + step*vb_dir;
            block_travel = block_travel + step;

            % update patch geometry
            [xl,xr,yb,yt] = block_bounds(block_center, block_len_mm, block_thick_mm);
            set(hBlock,'XData',[xl xr xr xl],'YData',[yb yb yt yt]);
        end

        % 3) Update car drawing
        [carPoly, hL, hC, hR] = update_car(ax, carPoly, hL, hC, hR, p0, r, u, ofs, s);

        % 4) Compute current sensor positions (world)
        pL = p0 + s*u + xL*r;  pC = p0 + s*u + xC*r;  pR = p0 + s*u + xR*r;

        % 5) First-hit detection (dynamic block)
        if ~hitL && in_rect(pL, xl,xr,yb,yt), hitL=true; odoL=s; highlight(ax,hL); end
        if ~hitC && in_rect(pC, xl,xr,yb,yt), hitC=true; odoC=s; highlight(ax,hC); end
        if ~hitR && in_rect(pR, xl,xr,yb,yt), hitR=true; odoR=s; highlight(ax,hR); end

        % 6) Decisions (Rule A & B)
        ruleA = ruleA_str(odoL, odoR, B);
        ruleB = ruleB_str(odoL, odoR, forward_window_mm);
        onlyL = isfinite(odoL) && ~isfinite(odoR);
        onlyR = isfinite(odoR) && ~isfinite(odoL);
        ruleB_flag = xor(onlyL, onlyR) && (min([odoL,odoR],[],'omitnan') <= forward_window_mm);

        action = action_str(odoL, odoR, odoC, B, ruleB_flag);

        % 7) Enhanced Status Display
        elapsed_time = toc;
        status.String = sprintf([ ...
            'FRAME: %d | TIME: %.1fs | CAR POSITION: s=%.1f mm\n' ...
            '════════════════════════════════════════════════\n' ...
            'phi = %.1f°   B = %.1f   window = %.1f mm\n' ...
            'Block travel: %.1f / %.1f mm  (dir %g°)\n' ...
            '────────────────────────────────────────────────\n' ...
            'SENSOR HITS:  L=%d  C=%d  R=%d\n' ...
            'Hit distances: L=%s | C=%s | R=%s\n' ...
            'Δs (outers) = %s\n' ...
            '────────────────────────────────────────────────\n' ...
            'Rule A (Δs>B): %s\n' ...
            'Rule B (one outer ≤ window): %s\n' ...
            '────────────────────────────────────────────────\n' ...
            'SUGGESTED ACTION:\n%s'], ...
            frame_count, elapsed_time, s, ...
            phi, B, forward_window_mm, ...
            block_travel, block_travel_limit_mm, block_dir_deg, ...
            hitL, hitC, hitR, ...
            numstr(odoL), numstr(odoC), numstr(odoR), ...
            deltaS_str(odoL,odoR), ruleA, yesno(ruleB_flag), action);

        % Control animation speed
        pause(frame_delay);
        drawnow;
        
        % Add frame counter display on plot
        if frame_count == 1 || mod(frame_count, 20) == 0
            title(ax, sprintf('Frame %d | Heading \\phi=%.1f° | Car s=%.1f mm | Block travel=%.1f mm', ...
                frame_count, phi, s, block_travel), 'FontWeight','bold');
        end
    end

    % Console summary
    fprintf('\n=== SIMULATION COMPLETE ===\n');
    fprintf('phi = %.1f° | block moved %.1f/%.1f mm @ %g°\n', ...
        phi, block_travel, block_travel_limit_mm, block_dir_deg);
    fprintf('Total frames: %d | Total time: %.2f seconds\n', frame_count, toc);
    final_report_console(odoL, odoR, B, forward_window_mm, onlyL, onlyR);

    if ishandle(fig)
        % Add completion message
        completion_text = annotation('textbox',[.25 .02 .50 .08], ...
            'String','SIMULATION COMPLETE - Press any key for next angle', ...
            'HorizontalAlignment','center','FontSize',12,'FontWeight','bold', ...
            'BackgroundColor',[1 1 0.8],'EdgeColor',[0.8 0.6 0]);
        pause; 
        if ishandle(fig), close(fig); end
    end
end
end

%% ----------------- Helper Functions (Enhanced) -----------------
function [xl,xr,yb,yt] = block_bounds(c, L, T)
xl = c(1) - L/2; xr = c(1) + L/2;
yb = c(2) - T/2; yt = c(2) + T/2;
end

function tf = in_rect(p, xl,xr,yb,yt)
tf = (p(1) >= xl) && (p(1) <= xr) && (p(2) >= yb) && (p(2) <= yt);
end

function [carPoly, hL, hC, hR] = draw_car(ax, p0, r, u, ofs, s, L, W)
% Enhanced car drawing with better visibility
P  = p0 + s*u;
BL = P - L*u - (W/2)*r;  BR = P - L*u + (W/2)*r;
FR = P + (W/2)*r;        FL = P - (W/2)*r;

% Car body with better colors
carPoly = patch('XData',[BL(1) BR(1) FR(1) FL(1)], ...
                'YData',[BL(2) BR(2) FR(2) FL(2)], ...
                'FaceColor',[0.8 0.9 1],'EdgeColor',[0.2 0.2 0.8],'LineWidth',2,'Parent',ax);

% Sensors with better visibility
pL = P + ofs(1,1)*r;  pC = P + ofs(2,1)*r;  pR = P + ofs(3,1)*r;
hL = plot(ax, pL(1), pL(2), 'ko','MarkerFaceColor','red','MarkerSize',10,'LineWidth',2);
hC = plot(ax, pC(1), pC(2), 'ko','MarkerFaceColor','green','MarkerSize',10,'LineWidth',2);
hR = plot(ax, pR(1), pR(2), 'ko','MarkerFaceColor','blue','MarkerSize',10,'LineWidth',2);

% Add direction arrow
arrow_end = P + (L/2)*u;
quiver(ax, P(1), P(2), arrow_end(1)-P(1), arrow_end(2)-P(2), 0, ...
       'Color', [1 0.5 0], 'LineWidth', 3, 'MaxHeadSize', 0.3);
end

function [carPoly, hL, hC, hR] = update_car(ax, carPoly, hL, hC, hR, p0, r, u, ofs, s)
% Enhanced car update function
L = 28; W = 60; % Use fixed values for consistency
P  = p0 + s*u;
BL = P - L*u - (W/2)*r;  BR = P - L*u + (W/2)*r;
FR = P + (W/2)*r;        FL = P - (W/2)*r;

% Update car body
set(carPoly,'XData',[BL(1) BR(1) FR(1) FL(1)], 'YData',[BL(2) BR(2) FR(2) FL(2)]);

% Update sensors
pL = P + ofs(1,1)*r; set(hL,'XData',pL(1),'YData',pL(2));
pC = P + ofs(2,1)*r; set(hC,'XData',pC(1),'YData',pC(2));
pR = P + ofs(3,1)*r; set(hR,'XData',pR(1),'YData',pR(2));
end

function highlight(ax,h)
% Enhanced highlighting
set(h,'MarkerEdgeColor','yellow','MarkerFaceColor','yellow','MarkerSize',15); 
% Add a pulsing effect
plot(ax,h.XData,h.YData,'yo','MarkerSize',20,'LineWidth',3);
end

% Keep all the existing helper functions unchanged
function s = numstr(v), if isfinite(v), s=sprintf('%.1f',v); else, s='–'; end, end
function s = deltaS_str(sL,sR), if isfinite(sL)&&isfinite(sR), s=sprintf('%.1f mm',abs(sR-sL)); else, s='n/a'; end, end
function s = ruleA_str(sL,sR,B)
if isfinite(sL)&&isfinite(sR)
  ds=abs(sR-sL); s=sprintf('%s (Δs=%.1f vs B=%.1f)', tern(ds>B,'TRUE','FALSE'), ds, B);
else, s='n/a (need both outers)'; end
end
function s = ruleB_str(sL,sR,win)
onlyL = isfinite(sL)&&~isfinite(sR)&&sL<=win;
onlyR = isfinite(sR)&&~isfinite(sL)&&sR<=win;
s = tern(xor(onlyL,onlyR), sprintf('TRUE (one outer ≤ %.0f mm)',win), 'FALSE');
end
function s = yesno(flag), if flag, s='TRUE'; else, s='FALSE'; end, end
function s = action_str(odoL,odoR,odoC,B,ruleB_flag)
if isfinite(odoL)&&isfinite(odoR)
  ds = abs(odoR-odoL);
  if ds>B, if odoL<odoR, s='θ>45°: reverse; rotate RIGHT ≤5°; retry';
           else,          s='θ>45°: reverse; rotate LEFT ≤5°; retry'; end
  else     if odoL<odoR, s='θ≤45°: micro-steer RIGHT to ≤5°; cross';
           else,          s='θ≤45°: micro-steer LEFT to ≤5°; cross'; end
  end
else
  if ruleB_flag
      if isfinite(odoL), s='RuleB: only L hit → θ>45° (reverse; rotate RIGHT ≤5°; retry)';
      else,              s='RuleB: only R hit → θ>45° (reverse; rotate LEFT ≤5°; retry)'; end
  else
      if isfinite(odoC), s='Centre-only: near-perpendicular; stop/verify then cross';
      else,              s='Drive forward (no decision yet)'; end
  end
end
end
function final_report_console(odoL,odoR,B,win,onlyL,onlyR)
if isfinite(odoL)&&isfinite(odoR)
  ds=abs(odoR-odoL); th=atand(ds/B);
  if abs(ds-B)<1e-9, fprintf('Boundary: Δs=%.1f → θ=45°.\n',ds);
  elseif ds>B,       fprintf('Both outers: Δs=%.1f → θ≈%.1f° (>45°).\n',ds,th);
  else,              fprintf('Both outers: Δs=%.1f → θ≈%.1f° (≤45°).\n',ds,th);
  end
elseif xor(onlyL,onlyR)
  side='LEFT'; if onlyR, side='RIGHT'; end
  fprintf('Only %s outer within window %.1f mm → θ>45° (Rule B).\n', side, win);
else
  fprintf('No decisive pattern yet; continue sampling.\n');
end
end
function t = tern(c,a,b), if c, t=a; else, t=b; end, end