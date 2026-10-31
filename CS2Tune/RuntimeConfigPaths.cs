namespace CS2Tune;

internal static class RuntimeConfigPaths
{
    public static string IniPath =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "CS2_ESP",
            "runtime.ini");
}
