% ======================================================================
% Preamp ↦ Filter (fixed) ↦ Postamp
% Read-off tool: choose preamp gain; compute required postamp gain
% Target: 5.0 Vpp final output; stage swing limit: 0–5 Vpp
% Exports vector PDFs for printing.
% ======================================================================

clear; clc; close all;

%% ---- Fixed parameters ----
Gf            = 22;          % fixed filter gain
Vpp_target    = 5.0;         % want 5.0 Vpp at final output
Aout_target   = Vpp_target/2;% target amplitude = 2.5 V
G1_max        = 2200;        % pre-amp gain cap
G3_max        = 2200;        % post-amp gain cap
Vpp_limit     = 5.0;         % per-stage linear swing limit (Vpp)
A_limit       = Vpp_limit/2; % amplitude limit per stage (V)

%% ---- Sweep axes ----
Vin_pp_uv = logspace(1,3,200);               % X: 10 µVpp … 1000 µVpp
G1_vec    = logspace(0,log10(2200),200);     % Y: 1× … 2200×

% grids: rows ↔ G1 choices, cols ↔ Vin points
[Vin_pp_uv_grid, G1_grid] = meshgrid(Vin_pp_uv, G1_vec);
Vin_amp_grid = (Vin_pp_uv_grid*1e-6)/2;      % amplitude in V

%% ---- Stage amplitudes before the post-amp ----
A1 = Vin_amp_grid .* G1_grid;    % after pre-amp
A2 = A1 .* Gf;                   % after filter (fixed gain)

%% ---- Required post-amp gain to hit 5 Vpp at the end ----
G3_req = Aout_target ./ max(A2, eps);

%% ---- Feasibility checks ----
ok_G1 = G1_grid <= G1_max;
ok_G3 = G3_req  <= G3_max;
ok_A1 = A1      <= A_limit;      % pre-amp not clipping
ok_A2 = A2      <= A_limit;      % filter not clipping
ok_all = ok_G1 & ok_G3 & ok_A1 & ok_A2;

% Mask impossible points for plotting (keep colourbar range sensible)
G3_plot = G3_req;
G3_plot(~ok_all) = NaN;

%% ===================== PLOTS (printer friendly) =====================
fig = figure('Position',[80 80 1200 520],'Color','w');

% ----- Left: Required post-amp gain heatmap -----
ax1 = subplot(1,2,1);
imagesc(Vin_pp_uv, G1_vec, G3_plot); axis xy tight
set(ax1,'XScale','log','YScale','log','Box','on','LineWidth',0.8);
cb1 = colorbar; colormap(ax1, parula);
xlabel('Input level (\muV p-p)'); ylabel('Chosen pre-amp gain G_1 (×)');
ylabel(cb1,'G_3 required (×)');
title('Required post-amp gain G_3 to reach 5 V_{pp}','FontWeight','bold');

% Light grey tint where infeasible (no heavy black)
hold on
bad = ~ok_all;
if any(bad,'all')
    h = imagesc(Vin_pp_uv, G1_vec, double(bad));
    set(h,'AlphaData',0.12);            % very light overlay
    colormap(ax1, parula);              % keep colour scale
end

% Feasibility boundary + helpful gain contours
try
    contour(Vin_pp_uv, G1_vec, ok_all, [1 1], 'k', 'LineWidth', 0.9);
    [~,hh] = contour(Vin_pp_uv, G1_vec, G3_req, ...
        [2 5 10 20 50 100 200 500 1000], 'k:', 'ShowText','on');
    hh.LineWidth = 0.8;
end
grid(ax1,'on');

% ----- Right: Feasibility map (white/very light grey) -----
ax2 = subplot(1,2,2);
imagesc(Vin_pp_uv, G1_vec, ok_all); axis xy tight
set(ax2,'XScale','log','YScale','log','Box','on','LineWidth',0.8);
colormap(ax2, [0.85 0.85 0.85; 1 1 1]); caxis([0 1]);  % 0=infeasible,1=feasible
cb2 = colorbar; set(cb2,'Ticks',[0 1],'TickLabels',{'Infeasible','Feasible'});
xlabel('Input level (\muV p-p)'); ylabel('Chosen pre-amp gain G_1 (×)');
title('Feasibility (no gain/swing limit violations)','FontWeight','bold');
hold on
contour(Vin_pp_uv, G1_vec, ok_all, [1 1], 'k','LineWidth',0.9);
grid(ax2,'on');

sgtitle(sprintf('Filter gain fixed at G_f = %.0f   |   Target output = 5.0 Vpp', Gf), ...
        'FontWeight','bold');

%% ===================== PDF EXPORTS =====================
% 1) Combined (both subplots in one page)
exportgraphics(fig, 'StageGains_Feasibility_Combined.pdf', 'ContentType','vector');

% 2) Individual pages
exportgraphics(ax1, 'Required_PostAmp_Gain.pdf', 'ContentType','vector');
exportgraphics(ax2, 'Feasibility_Map.pdf',       'ContentType','vector');

disp('Saved PDFs:');
disp('  - StageGains_Feasibility_Combined.pdf');
disp('  - Required_PostAmp_Gain.pdf');
disp('  - Feasibility_Map.pdf');

%% ------------ Readout helper ------------
% Call: readout( VIN_uVpp , G1_choice )
function readout(Vin_uVpp, G1_choice)
    % Echo the numbers for a single operating point
    Gf = evalin('base','Gf'); Aout_target = evalin('base','Aout_target');
    A_limit = evalin('base','A_limit'); G1_max = evalin('base','G1_max'); G3_max = evalin('base','G3_max');
    Vin_amp = (Vin_uVpp*1e-6)/2;
    A1 = Vin_amp * G1_choice;
    A2 = A1 * Gf;
    G3_req = Aout_target / max(A2,eps);
    feasible = (G1_choice<=G1_max) && (G3_req<=G3_max) && (A1<=A_limit) && (A2<=A_limit);
    fprintf('\nREADOUT @ %.1f µVpp, G1=%.1f×\n', Vin_uVpp, G1_choice);
    fprintf('  After pre-amp:  %.3f V amplitude (%.3f Vpp)\n', A1, 2*A1);
    fprintf('  After filter:   %.3f V amplitude (%.3f Vpp)\n', A2, 2*A2);
    fprintf('  Needed G3:      %.1f×  (limit %.0f×)\n', G3_req, G3_max);
    fprintf('  Feasible:       %s\n', string(feasible));
end
