using System.Diagnostics;
using System.Text.Json;

namespace CS2Panel;

internal sealed class MainForm : Form
{
    private readonly ProjectPaths _paths;
    private readonly ShellRunner _runner = new();
    private readonly RichTextBox _log = new() { Dock = DockStyle.Fill, ReadOnly = true, BackColor = Color.FromArgb(30, 30, 30), ForeColor = Color.Gainsboro, Font = new Font("Consolas", 9.5f) };
    private readonly CheckBox _adminDump = new() { Text = "导出偏移时使用管理员权限", AutoSize = true, Checked = true };
    private readonly Label _status = new() { Dock = DockStyle.Top, Height = 28, TextAlign = ContentAlignment.MiddleLeft, Padding = new Padding(8, 0, 0, 0) };
    private CancellationTokenSource? _cts;

    public MainForm(ProjectPaths paths)
    {
        _paths = paths;
        Text = "CS2 透视工具面板";
        MinimumSize = new Size(720, 520);
        Size = new Size(860, 600);
        StartPosition = FormStartPosition.CenterScreen;

        _runner.LineReceived += line => AppendLog(line);

        var top = new FlowLayoutPanel
        {
            Dock = DockStyle.Top,
            Height = 120,
            Padding = new Padding(8),
            WrapContents = true,
            AutoScroll = true,
        };

        top.Controls.Add(MakeButton("检查环境", (_, _) => CheckEnvironment()));
        top.Controls.Add(MakeButton("1. 导出偏移 (cs2-dumper)", (_, _) => RunStep(DumpOffsetsAsync)));
        top.Controls.Add(MakeButton("2. 写入 offsets.hpp", (_, _) => RunStep(SyncOffsetsAsync)));
        top.Controls.Add(MakeButton("3. 编译 ESP", (_, _) => RunStep(BuildEspAsync)));
        top.Controls.Add(MakeButton("4. 启动透视", (_, _) => RunStep(LaunchEspAsync)));
        top.Controls.Add(MakeButton("一键：导出+写入+编译", (_, _) => RunStep(FullUpdateAsync)));
        top.Controls.Add(_adminDump);

        var bottom = new Panel { Dock = DockStyle.Bottom, Height = 36 };
        bottom.Controls.Add(_status);

        Controls.Add(_log);
        Controls.Add(bottom);
        Controls.Add(top);

        AppendLog($"项目目录: {_paths.RepoRoot}");
        AppendLog($"锁头热调请单独运行 CS2Tune（scripts\\build-tune.ps1）");
        CheckEnvironment();
    }

    private Button MakeButton(string text, EventHandler onClick)
    {
        var btn = new Button
        {
            Text = text,
            AutoSize = true,
            Margin = new Padding(4),
            Padding = new Padding(10, 6, 10, 6),
        };
        btn.Click += onClick;
        return btn;
    }

    private void CheckEnvironment()
    {
        AppendLog("--- 环境检查 ---");
        AppendLog(Process.GetProcessesByName("cs2").Length > 0
            ? "[OK] 检测到 cs2.exe 正在运行"
            : "[提示] cs2.exe 未运行（导出偏移前请先开 CS2 主菜单）");
        AppendLog(File.Exists(_paths.DumperExe)
            ? $"[OK] cs2-dumper: {_paths.DumperExe}"
            : $"[缺失] cs2-dumper: {_paths.DumperExe}");
        AppendLog(File.Exists(_paths.BuildScript)
            ? $"[OK] 编译脚本: {_paths.BuildScript}"
            : $"[缺失] {_paths.BuildScript}");
        AppendLog(File.Exists(_paths.EspExe)
            ? $"[OK] ESP 已编译: {_paths.EspExe}"
            : "[提示] ESP 尚未编译，请点「3. 编译 ESP」");
        SetStatus("就绪");
    }

    private async void RunStep(Func<CancellationToken, Task> step)
    {
        if (_cts is not null)
        {
            AppendLog("[!] 已有任务在运行");
            return;
        }

        _cts = new CancellationTokenSource();
        SetStatus("运行中...");
        try
        {
            await step(_cts.Token);
            SetStatus("完成");
        }
        catch (Exception ex)
        {
            AppendLog("[错误] " + ex.Message);
            SetStatus("失败");
        }
        finally
        {
            _cts.Dispose();
            _cts = null;
        }
    }

    private async Task DumpOffsetsAsync(CancellationToken token)
    {
        if (!File.Exists(_paths.DumperExe))
            throw new FileNotFoundException("未找到 cs2-dumper，请确认 tools/cs2-dumper/cs2-dumper.exe 存在");

        Directory.CreateDirectory(_paths.DumpOutput);
        AppendLog("--- 运行 cs2-dumper ---");
        if (_adminDump.Checked)
            AppendLog("将弹出 UAC，请点「是」。完成后检查 tools\\dump-output 是否生成 json。");
        var args = $"-o \"{_paths.DumpOutput}\" -f json";
        var code = await _runner.RunAsync(_paths.DumperExe, args, _paths.RepoRoot, _adminDump.Checked, token);
        if (code != 0)
            throw new InvalidOperationException(
                $"cs2-dumper 退出码 {code}。可取消「管理员」重试，或手动管理员运行 tools\\cs2-dumper\\cs2-dumper.exe");

        if (!File.Exists(_paths.OffsetsJson) || !File.Exists(_paths.ClientDllJson))
            throw new InvalidOperationException("dump 完成但未找到 offsets.json / client_dll.json");

        AppendLog("[OK] 输出目录: " + _paths.DumpOutput);
    }

    private Task SyncOffsetsAsync(CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        if (!File.Exists(_paths.OffsetsJson) || !File.Exists(_paths.ClientDllJson))
            throw new FileNotFoundException("请先执行「导出偏移」");

        AppendLog("--- 生成 offsets.hpp ---");
        var content = OffsetGenerator.Generate(_paths.OffsetsJson, _paths.ClientDllJson);
        Directory.CreateDirectory(Path.GetDirectoryName(_paths.OffsetsHpp)!);
        File.WriteAllText(_paths.OffsetsHpp, content);
        AppendLog("[OK] 已写入: " + _paths.OffsetsHpp);
        if (!JsonDocument.Parse(File.ReadAllText(_paths.OffsetsJson)).RootElement
                .GetProperty("client.dll").TryGetProperty("dwSensitivity", out _))
            AppendLog("[提示] offsets.json 无 dwSensitivity（dumper 模式过期），已跳过；ESP 不依赖此项。");
        return Task.CompletedTask;
    }

    private async Task BuildEspAsync(CancellationToken token)
    {
        if (!File.Exists(_paths.BuildScript))
            throw new FileNotFoundException(_paths.BuildScript);

        AppendLog("--- 编译 CS2_ESP ---");
        var code = await _runner.RunAsync(
            "powershell.exe",
            $"-NoProfile -ExecutionPolicy Bypass -File \"{_paths.BuildScript}\"",
            _paths.RepoRoot,
            false,
            token);

        if (code != 0)
            throw new InvalidOperationException($"编译失败，退出码 {code}");

        if (!File.Exists(_paths.EspExe))
            throw new FileNotFoundException("编译完成但未找到 CS2_ESP.exe");

        AppendLog("[OK] " + _paths.EspExe);
    }

    private Task LaunchEspAsync(CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        if (!File.Exists(_paths.EspExe))
            throw new FileNotFoundException("请先编译 ESP");

        AppendLog("--- 启动 CS2_ESP ---");
        Process.Start(new ProcessStartInfo
        {
            FileName = _paths.EspExe,
            WorkingDirectory = Path.GetDirectoryName(_paths.EspExe)!,
            UseShellExecute = true,
        });
        AppendLog("[OK] 已启动。Insert 透视 | Home 内存锁 | 侧键按住锁 | End 退出。");
        AppendLog("[提示] 锁头参数用 CS2Tune.exe 调整，ESP 运行中会自动生效。");
        return Task.CompletedTask;
    }

    private async Task FullUpdateAsync(CancellationToken token)
    {
        await DumpOffsetsAsync(token);
        await SyncOffsetsAsync(token);
        await BuildEspAsync(token);
        AppendLog("[OK] 一键更新完成，可点「4. 启动透视」");
    }

    private void AppendLog(string line)
    {
        if (InvokeRequired)
        {
            BeginInvoke(() => AppendLog(line));
            return;
        }

        _log.AppendText($"[{DateTime.Now:HH:mm:ss}] {line}{Environment.NewLine}");
        _log.ScrollToCaret();
    }

    private void SetStatus(string text)
    {
        if (InvokeRequired)
        {
            BeginInvoke(() => SetStatus(text));
            return;
        }

        _status.Text = "  " + text;
    }
}
