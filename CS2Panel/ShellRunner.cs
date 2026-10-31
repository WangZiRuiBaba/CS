using System.Diagnostics;
using System.Text;

namespace CS2Panel;

internal sealed class ShellRunner
{
    public event Action<string>? LineReceived;

    public async Task<int> RunAsync(string fileName, string arguments, string workingDirectory,
        bool asAdmin, CancellationToken cancellationToken)
    {
        var psi = new ProcessStartInfo
        {
            FileName = fileName,
            Arguments = arguments,
            WorkingDirectory = workingDirectory,
        };

        if (asAdmin)
        {
            psi.UseShellExecute = true;
            psi.Verb = "runas";
            psi.WindowStyle = ProcessWindowStyle.Hidden;
        }
        else
        {
            psi.UseShellExecute = false;
            psi.RedirectStandardOutput = true;
            psi.RedirectStandardError = true;
            psi.CreateNoWindow = true;
            psi.StandardOutputEncoding = Encoding.UTF8;
            psi.StandardErrorEncoding = Encoding.UTF8;
        }

        using var process = Process.Start(psi);
        if (process is null)
            throw new InvalidOperationException(asAdmin
                ? "进程未启动（可能取消了 UAC 管理员授权）"
                : $"Failed to start: {fileName}");

        if (!asAdmin)
        {
            _ = PumpAsync(process.StandardOutput, cancellationToken);
            _ = PumpAsync(process.StandardError, cancellationToken);
        }
        else
        {
            LineReceived?.Invoke("(管理员模式：dumper 无控制台输出，请等待 UAC 与进程结束…)");
        }

        await process.WaitForExitAsync(cancellationToken);
        return process.ExitCode;
    }

    private async Task PumpAsync(StreamReader reader, CancellationToken cancellationToken)
    {
        while (!reader.EndOfStream && !cancellationToken.IsCancellationRequested)
        {
            var line = await reader.ReadLineAsync(cancellationToken);
            if (line is null)
                break;
            LineReceived?.Invoke(line);
        }
    }
}
