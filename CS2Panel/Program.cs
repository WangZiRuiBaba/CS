namespace CS2Panel;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        var paths = ProjectPaths.Discover();

        if (args.Length > 0 && args[0] == "--sync-offsets")
        {
            if (!File.Exists(paths.OffsetsJson) || !File.Exists(paths.ClientDllJson))
            {
                Console.Error.WriteLine("Missing dump-output JSON. Run cs2-dumper first.");
                Environment.Exit(1);
            }

            var content = OffsetGenerator.Generate(paths.OffsetsJson, paths.ClientDllJson);
            Directory.CreateDirectory(Path.GetDirectoryName(paths.OffsetsHpp)!);
            File.WriteAllText(paths.OffsetsHpp, content);
            Console.WriteLine("Wrote " + paths.OffsetsHpp);
            return;
        }

        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm(paths));
    }
}
