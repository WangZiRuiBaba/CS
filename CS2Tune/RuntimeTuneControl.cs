namespace CS2Tune;

internal sealed class RuntimeTuneControl : UserControl
{
    private readonly Label _pathLabel = new() { Dock = DockStyle.Top, Height = 36, AutoEllipsis = true, Padding = new Padding(8, 8, 8, 0) };
    private readonly Label _hint = new()
    {
        Dock = DockStyle.Top,
        Height = 52,
        Text =
            "改数字会自动保存。ESP 开着也能立刻生效，不用重启。\r\n" +
            "数值越大一般越「猛」、越快；越小越稳、越像自己瞄。",
        Padding = new Padding(8, 0, 8, 4),
    };
    private readonly FlowLayoutPanel _buttons = new() { Dock = DockStyle.Top, Height = 40, Padding = new Padding(8, 4, 8, 4), AutoSize = true };
    private readonly Panel _scrollHost = new() { Dock = DockStyle.Fill, AutoScroll = true };
    private readonly TableLayoutPanel _grid = new() { ColumnCount = 3, AutoSize = true, Padding = new Padding(8) };

    private readonly Dictionary<string, NumericUpDown> _fields = new(StringComparer.OrdinalIgnoreCase);
    private readonly System.Windows.Forms.Timer _saveTimer = new() { Interval = 350 };
    private bool _loading;

    public RuntimeTuneControl()
    {
        _saveTimer.Tick += (_, _) =>
        {
            _saveTimer.Stop();
            FlushSave();
        };

        _buttons.Controls.Add(MakeButton("重新读文件", (_, _) => LoadFromDisk()));
        _buttons.Controls.Add(MakeButton("恢复默认", (_, _) => ResetDefaults()));

        _grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 200));
        _grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100));
        _grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

        AddSection("平时怎么锁（点一下、短点）");

        AddField(
            "fov_degrees",
            "准星偏多少还能锁",
            "越大：离头更远也帮你吸过去；太小容易锁不上。",
            1m, 30m, 0.5m, 1);
        AddField(
            "smooth",
            "跟过去有多快",
            "越大转得越快；越小越慢、越稳，不那么像锁。",
            0.05m, 1m, 0.01m, 2);
        AddField(
            "max_step_degrees",
            "每瞬间最多转多少",
            "防止一帧甩太猛；太大可能抖，太小跟不住快移目标。",
            0.1m, 8m, 0.05m, 2);
        AddField(
            "head_z_offset",
            "瞄高还是瞄低",
            "负数往下、正数往上，微调爆头高度。",
            -20m, 20m, 0.5m, 1);
        AddField(
            "rcs_scale_tap",
            "点射压枪（消上跳）",
            "抵消开枪枪口往上蹿；0 等于不压，越大压得越狠。",
            0m, 5m, 0.1m, 1);

        AddSection("屏幕里选哪个人");

        AddField(
            "screen_fov_pixels",
            "准星周围多大圈算「近」",
            "以屏幕中心为圆心，像素越大越容易选到边上的人。",
            50m, 800m, 10m, 0);
        AddField(
            "target_switch_hysteresis_deg",
            "两个人差不多近时别乱跳",
            "越大越愿意继续打原来那个，减少左右来回换。",
            0m, 10m, 0.05m, 2);
        AddField(
            "target_lock_break_fov_deg",
            "人跑远就换下一个",
            "当前目标偏出这么多角度就松手，去锁更近的。",
            3m, 30m, 0.5m, 1);

        AddSection("连发 / 扫射（按住连点）");

        AddField(
            "spray_shots_threshold",
            "连打几枪算扫射",
            "达到后改用下面「连发」那一套（胸线、压枪等）。",
            1m, 10m, 1m, 0);
        AddField(
            "spray_smooth",
            "连发时跟枪有多快",
            "和上面「跟过去有多快」一样，只作用于扫射。",
            0.05m, 1m, 0.01m, 2);
        AddField(
            "spray_max_step_degrees",
            "连发每瞬间最多转多少",
            "扫射时单帧转角上限，防甩枪。",
            0.1m, 10m, 0.1m, 1);
        AddField(
            "spray_chest_drop_z",
            "扫射时往下瞄胸口",
            "越大枪口越往下，少打头、多打胸；连发后坐力大时常调这个。",
            0m, 80m, 1m, 0);
        AddField(
            "rcs_scale_spray",
            "连发压枪（消上跳）",
            "扫射专用压后坐力，一般比点射压得狠一点。",
            0m, 6m, 0.1m, 1);

        _scrollHost.Controls.Add(_grid);
        Controls.Add(_scrollHost);
        Controls.Add(_buttons);
        Controls.Add(_hint);
        Controls.Add(_pathLabel);

        Load += (_, _) =>
        {
            _pathLabel.Text = "配置文件位置：" + RuntimeConfigPaths.IniPath;
            LoadFromDisk();
        };
    }

    private static Button MakeButton(string text, EventHandler onClick)
    {
        var btn = new Button { Text = text, AutoSize = true, Margin = new Padding(4) };
        btn.Click += onClick;
        return btn;
    }

    private void AddSection(string title)
    {
        var row = _grid.RowCount;
        _grid.RowCount = row + 1;
        _grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        var lbl = new Label
        {
            Text = title,
            AutoSize = true,
            Font = new Font(Font.FontFamily, Font.Size, FontStyle.Bold),
            Margin = new Padding(0, row == 0 ? 0 : 14, 0, 6),
        };
        _grid.SetColumnSpan(lbl, 3);
        _grid.Controls.Add(lbl, 0, row);
    }

    private void AddField(string key, string title, string tip, decimal min, decimal max, decimal increment, int decimals)
    {
        var row = _grid.RowCount;
        _grid.RowCount = row + 1;
        _grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        var lbl = new Label
        {
            Text = title,
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(0, 6, 8, 2),
        };
        var num = new NumericUpDown
        {
            Minimum = min,
            Maximum = max,
            Increment = increment,
            DecimalPlaces = decimals,
            Width = 96,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(0, 2, 8, 2),
        };
        num.ValueChanged += (_, _) => ScheduleSave();

        var help = new Label
        {
            Text = tip,
            AutoSize = true,
            ForeColor = SystemColors.GrayText,
            Font = new Font(Font.FontFamily, Font.Size - 0.5f),
            MaximumSize = new Size(420, 0),
            Margin = new Padding(0, 6, 0, 2),
        };

        _grid.Controls.Add(lbl, 0, row);
        _grid.Controls.Add(num, 1, row);
        _grid.Controls.Add(help, 2, row);
        _fields[key] = num;
    }

    private void LoadFromDisk()
    {
        _loading = true;
        try
        {
            var cfg = RuntimeConfigStore.Load();
            SetField("fov_degrees", cfg.FovDegrees);
            SetField("smooth", cfg.Smooth);
            SetField("max_step_degrees", cfg.MaxStepDegrees);
            SetField("head_z_offset", cfg.HeadZOffset);
            SetField("screen_fov_pixels", cfg.ScreenFovPixels);
            SetField("target_switch_hysteresis_deg", cfg.TargetSwitchHysteresisDeg);
            SetField("target_lock_break_fov_deg", cfg.TargetLockBreakFovDeg);
            SetField("spray_smooth", cfg.SpraySmooth);
            SetField("spray_max_step_degrees", cfg.SprayMaxStepDegrees);
            SetField("spray_chest_drop_z", cfg.SprayChestDropZ);
            SetField("rcs_scale_tap", cfg.RcsScaleTap);
            SetField("rcs_scale_spray", cfg.RcsScaleSpray);
            SetField("spray_shots_threshold", cfg.SprayShotsThreshold);
        }
        finally
        {
            _loading = false;
        }
    }

    private void ResetDefaults()
    {
        _loading = true;
        try
        {
            var cfg = new RuntimeAimConfig();
            SetField("fov_degrees", cfg.FovDegrees);
            SetField("smooth", cfg.Smooth);
            SetField("max_step_degrees", cfg.MaxStepDegrees);
            SetField("head_z_offset", cfg.HeadZOffset);
            SetField("screen_fov_pixels", cfg.ScreenFovPixels);
            SetField("target_switch_hysteresis_deg", cfg.TargetSwitchHysteresisDeg);
            SetField("target_lock_break_fov_deg", cfg.TargetLockBreakFovDeg);
            SetField("spray_smooth", cfg.SpraySmooth);
            SetField("spray_max_step_degrees", cfg.SprayMaxStepDegrees);
            SetField("spray_chest_drop_z", cfg.SprayChestDropZ);
            SetField("rcs_scale_tap", cfg.RcsScaleTap);
            SetField("rcs_scale_spray", cfg.RcsScaleSpray);
            SetField("spray_shots_threshold", cfg.SprayShotsThreshold);
        }
        finally
        {
            _loading = false;
        }

        FlushSave();
    }

    private void SetField(string key, float value) => SetField(key, (decimal)value);

    private void SetField(string key, int value) => SetField(key, (decimal)value);

    private void SetField(string key, decimal value)
    {
        if (!_fields.TryGetValue(key, out var num))
            return;
        var clamped = value;
        if (clamped < num.Minimum)
            clamped = num.Minimum;
        if (clamped > num.Maximum)
            clamped = num.Maximum;
        num.Value = clamped;
    }

    private void ScheduleSave()
    {
        if (_loading)
            return;
        _saveTimer.Stop();
        _saveTimer.Start();
    }

    private void FlushSave()
    {
        if (_loading)
            return;

        var cfg = new RuntimeAimConfig
        {
            FovDegrees = (float)_fields["fov_degrees"].Value,
            Smooth = (float)_fields["smooth"].Value,
            MaxStepDegrees = (float)_fields["max_step_degrees"].Value,
            HeadZOffset = (float)_fields["head_z_offset"].Value,
            ScreenFovPixels = (float)_fields["screen_fov_pixels"].Value,
            TargetSwitchHysteresisDeg = (float)_fields["target_switch_hysteresis_deg"].Value,
            TargetLockBreakFovDeg = (float)_fields["target_lock_break_fov_deg"].Value,
            SpraySmooth = (float)_fields["spray_smooth"].Value,
            SprayMaxStepDegrees = (float)_fields["spray_max_step_degrees"].Value,
            SprayChestDropZ = (float)_fields["spray_chest_drop_z"].Value,
            RcsScaleTap = (float)_fields["rcs_scale_tap"].Value,
            RcsScaleSpray = (float)_fields["rcs_scale_spray"].Value,
            SprayShotsThreshold = (int)_fields["spray_shots_threshold"].Value,
        };
        RuntimeConfigStore.Save(cfg);
    }
}
