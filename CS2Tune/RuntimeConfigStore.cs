using System.Globalization;

namespace CS2Tune;

internal sealed class RuntimeAimConfig
{
    public float FovDegrees { get; set; } = 7f;
    public float Smooth { get; set; } = 0.22f;
    public float MaxStepDegrees { get; set; } = 1.25f;
    public float HeadZOffset { get; set; } = -2f;
    public float ScreenFovPixels { get; set; } = 220f;
    public float TargetSwitchHysteresisDeg { get; set; } = 1.35f;
    public float TargetLockBreakFovDeg { get; set; } = 10.5f;
    public float SpraySmooth { get; set; } = 0.42f;
    public float SprayMaxStepDegrees { get; set; } = 2.4f;
    public float SprayChestDropZ { get; set; } = 28f;
    public float RcsScaleTap { get; set; } = 1.2f;
    public float RcsScaleSpray { get; set; } = 2.2f;
    public int SprayShotsThreshold { get; set; } = 2;
}

internal static class RuntimeConfigStore
{
    private const string Section = "aim";

    public static RuntimeAimConfig Load()
    {
        var path = RuntimeConfigPaths.IniPath;
        EnsureExists(path);
        var cfg = new RuntimeAimConfig();
        if (!File.Exists(path))
            return cfg;

        cfg.FovDegrees = ReadFloat(path, "fov_degrees", cfg.FovDegrees);
        cfg.Smooth = ReadFloat(path, "smooth", cfg.Smooth);
        cfg.MaxStepDegrees = ReadFloat(path, "max_step_degrees", cfg.MaxStepDegrees);
        cfg.HeadZOffset = ReadFloat(path, "head_z_offset", cfg.HeadZOffset);
        cfg.ScreenFovPixels = ReadFloat(path, "screen_fov_pixels", cfg.ScreenFovPixels);
        cfg.TargetSwitchHysteresisDeg = ReadFloat(path, "target_switch_hysteresis_deg", cfg.TargetSwitchHysteresisDeg);
        cfg.TargetLockBreakFovDeg = ReadFloat(path, "target_lock_break_fov_deg", cfg.TargetLockBreakFovDeg);
        cfg.SpraySmooth = ReadFloat(path, "spray_smooth", cfg.SpraySmooth);
        cfg.SprayMaxStepDegrees = ReadFloat(path, "spray_max_step_degrees", cfg.SprayMaxStepDegrees);
        cfg.SprayChestDropZ = ReadFloat(path, "spray_chest_drop_z", cfg.SprayChestDropZ);
        cfg.RcsScaleTap = ReadFloat(path, "rcs_scale_tap", cfg.RcsScaleTap);
        cfg.RcsScaleSpray = ReadFloat(path, "rcs_scale_spray", cfg.RcsScaleSpray);
        cfg.SprayShotsThreshold = ReadInt(path, "spray_shots_threshold", cfg.SprayShotsThreshold);
        return cfg;
    }

    public static void Save(RuntimeAimConfig cfg)
    {
        var path = RuntimeConfigPaths.IniPath;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        WriteFloat(path, "fov_degrees", cfg.FovDegrees);
        WriteFloat(path, "smooth", cfg.Smooth);
        WriteFloat(path, "max_step_degrees", cfg.MaxStepDegrees);
        WriteFloat(path, "head_z_offset", cfg.HeadZOffset);
        WriteFloat(path, "screen_fov_pixels", cfg.ScreenFovPixels);
        WriteFloat(path, "target_switch_hysteresis_deg", cfg.TargetSwitchHysteresisDeg);
        WriteFloat(path, "target_lock_break_fov_deg", cfg.TargetLockBreakFovDeg);
        WriteFloat(path, "spray_smooth", cfg.SpraySmooth);
        WriteFloat(path, "spray_max_step_degrees", cfg.SprayMaxStepDegrees);
        WriteFloat(path, "spray_chest_drop_z", cfg.SprayChestDropZ);
        WriteFloat(path, "rcs_scale_tap", cfg.RcsScaleTap);
        WriteFloat(path, "rcs_scale_spray", cfg.RcsScaleSpray);
        WriteInt(path, "spray_shots_threshold", cfg.SprayShotsThreshold);
    }

    private static void EnsureExists(string path)
    {
        if (File.Exists(path))
            return;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        Save(new RuntimeAimConfig());
    }

    private static float ReadFloat(string path, string key, float fallback)
    {
        var raw = ReadRaw(path, key);
        return float.TryParse(raw, NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : fallback;
    }

    private static int ReadInt(string path, string key, int fallback)
    {
        var raw = ReadRaw(path, key);
        return int.TryParse(raw, NumberStyles.Integer, CultureInfo.InvariantCulture, out var v) ? v : fallback;
    }

    private static string ReadRaw(string path, string key)
    {
        foreach (var line in File.ReadAllLines(path))
        {
            var t = line.Trim();
            if (t.StartsWith('[') || !t.Contains('='))
                continue;
            var parts = t.Split('=', 2);
            if (parts.Length != 2)
                continue;
            if (string.Equals(parts[0].Trim(), key, StringComparison.OrdinalIgnoreCase))
                return parts[1].Trim();
        }

        return "";
    }

    private static void WriteFloat(string path, string key, float value) =>
        AppendOrReplace(path, key, value.ToString("0.####", CultureInfo.InvariantCulture));

    private static void WriteInt(string path, string key, int value) =>
        AppendOrReplace(path, key, value.ToString(CultureInfo.InvariantCulture));

    private static void AppendOrReplace(string path, string key, string value)
    {
        var lines = File.Exists(path) ? File.ReadAllLines(path).ToList() : new List<string>();
        if (lines.Count == 0 || !lines[0].StartsWith('['))
        {
            lines.Insert(0, $"[{Section}]");
        }

        var replaced = false;
        for (var i = 0; i < lines.Count; i++)
        {
            var t = lines[i].Trim();
            if (!t.Contains('='))
                continue;
            var parts = t.Split('=', 2);
            if (parts.Length == 2 && string.Equals(parts[0].Trim(), key, StringComparison.OrdinalIgnoreCase))
            {
                lines[i] = $"{key}={value}";
                replaced = true;
                break;
            }
        }

        if (!replaced)
            lines.Add($"{key}={value}");

        File.WriteAllLines(path, lines);
    }
}
