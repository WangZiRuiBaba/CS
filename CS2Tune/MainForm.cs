namespace CS2Tune;

internal sealed class MainForm : Form
{
    public MainForm()
    {
        Text = "CS2 锁头参数（白话）";
        MinimumSize = new Size(760, 620);
        Size = new Size(820, 680);
        StartPosition = FormStartPosition.CenterScreen;
        Controls.Add(new RuntimeTuneControl { Dock = DockStyle.Fill });
    }
}
