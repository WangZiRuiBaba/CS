namespace CS2Panel;

internal sealed class ProjectPaths
{
    public ProjectPaths(string repoRoot)
    {
        RepoRoot = repoRoot;
        DumperExe = Path.Combine(RepoRoot, "tools", "cs2-dumper", "cs2-dumper.exe");
        DumpOutput = Path.Combine(RepoRoot, "tools", "dump-output");
        OffsetsJson = Path.Combine(DumpOutput, "offsets.json");
        ClientDllJson = Path.Combine(DumpOutput, "client_dll.json");
        OffsetsHpp = Path.Combine(RepoRoot, "sdk", "offsets", "offsets.hpp");
        EspExe = Path.Combine(RepoRoot, "build", "Release", "CS2_ESP.exe");
        BuildScript = Path.Combine(RepoRoot, "scripts", "build.ps1");
    }

    public string RepoRoot { get; }
    public string DumperExe { get; }
    public string DumpOutput { get; }
    public string OffsetsJson { get; }
    public string ClientDllJson { get; }
    public string OffsetsHpp { get; }
    public string EspExe { get; }
    public string BuildScript { get; }

    public static ProjectPaths Discover()
    {
        var dir = AppContext.BaseDirectory;
        for (var i = 0; i < 8; i++)
        {
            var candidate = dir;
            if (File.Exists(Path.Combine(candidate, "CMakeLists.txt")))
                return new ProjectPaths(candidate);
            dir = Path.GetDirectoryName(dir) ?? candidate;
        }

        throw new InvalidOperationException("Cannot find CS repo root (CMakeLists.txt).");
    }
}
