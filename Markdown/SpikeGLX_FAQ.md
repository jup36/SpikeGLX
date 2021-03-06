# SpikeGLX FAQ

**Topics:**

* [Why SpikeGLX?](SpikeGLX_FAQ.md#why-spikeglx)
* [How to Uninstall](SpikeGLX_FAQ.md#how-to-uninstall)
* [Side by Side Versions](SpikeGLX_FAQ.md#side-by-side-versions)
* [Running Two Copies of SpikeGLX](SpikeGLX_FAQ.md#running-two-copies-of-spikeglx)
* [Data Integrity](SpikeGLX_FAQ.md#data-integrity)
* [Gauging System Health](SpikeGLX_FAQ.md#gauging-system-health)
* [How to Report Bugs](SpikeGLX_FAQ.md#how-to-report-bugs)
* Nidq Settings
    + [Wrong Sample Rate](SpikeGLX_FAQ.md#wrong-sample-rate)

## <a name="why-spikeglx"></a>Why SpikeGLX?

"What does Bill think is the best feature of SpikeGLX?"

I worked really hard on experiment integration and synchronization.

As originally conceived the Imec 'Neuropixels' probes recorded lots of
neural channels but had very limited auxiliary inputs for accelerometers,
physiological readouts, lick responses, door activations and so on.
Combining the Imec and Nidq streams vastly expands the aux inputs
available in your experiment.

Moreover, all of the data are tightly synchronized. You can see them
together on screen during an experiment, the output files are synced
to within a few samples and the offline File Viewer lets you review
all the recorded data in a time-locked way. This helps you see the
experiment as an integrated whole.

## <a name="how-to-uninstall"></a>How to Uninstall

"How do I completely remove SpikeGLX from my computer?"

When you download a release of SpikeGLX you get a folder with a name like
"Release_v20161101". Everything SpikeGLX needs to run is in that folder.
There are no Registry entries, no DLLs placed into Windows OS folders or
any other cookies or crumbs.

To delete it, drag the release folder to the trash.

## <a name="side-by-side-versions"></a>Side by Side Versions

"Can I have multiple versions of SpikeGLX on one computer?...Will they
interfere with each other?"

Yes, not a problem. Refer to FAQ
[How to Uninstall](SpikeGLX_FAQ.md#how-to-uninstall) to see
that each SpikeGLX setup is self-contained. We organize things like this:

```
SpikeGLX/               // master folder with all versions
    ...
    Release_v20160703/  // a release folder
    Release_v20160806/  // another
    Release_v20161101/  // and so on
    ...
```

## <a name="running-two-copies-of-spikeglx"></a>Running Two Copies of SpikeGLX

"What happens if I try to run two copies at the same time?"

If you are only doing offline things, like using the File Viewer, there is
no problem. You can run as many instances as you want until you run out
of RAM. There are only issues if data acquisition hardware is involved.

If you are using NI-DAQ hardware, you have to visit the Configuration dialog
in SpikeGLX, check `Enable NI-DAQ` and click `Detect`. The first instance
of the app to do that gets ownership of the NI-DAQ hardware and no other
instance will be allowed to use it. This prevents accidental conflicts which
is a good thing, but it also prevents running multiple NI-based probes on
one host computer.

Imec hardware uses a fixed static IP address, and that prevents running
two setups from one host. So generally, to do online runs, you need
separate computers.

## <a name="data-integrity"></a>Data Integrity

"My run quit unexpectedly, are my data likely to be corrupt or garbage?"

#### Graceful Shutdown

SpikeGLX monitors a number of health and performance metrics during a
data taking run. If there are signs of **pending** trouble it will
initiate a graceful shutdown of the run before a catastrophic failure
occurs. It closes open data files and then stops the data acquisition.
Messages in the Console window's log will describe the specific problem
encountered and the fact that the run was stopped. Your data files are
intact because we close them before corruption happens.

#### Crash

A graceful shutdown, described above, is **not a "crash"**. Software
engineers reserve the term "crash" for a completely pathological and
unexpected event that is so bad, the operating system must step in
and terminate the application with extreme prejudice before harm is
done to other programs or to the OS itself.

What are the signs of a crash? When an event like this happens, there
will usually be a Windows OS dialog box on the screen with a cryptic
message about quitting unexpectedly. Often, since Windows 7, the screen
has a peculiar milky appearance after a crash. A really severe crash ends
in the blue screen of death.

Crashes are usually the consequence of software bugs (bad practice) and
these mistakes are fixed over time as they're uncovered. If a crash happens
while running SpikeGLX (unlikely), you're still in pretty good shape
because the data files will be intact up to the moment just before the
crash. The crash itself will be very swift, so only the last few data
points in the files may be in question.

## <a name="gauging-system-health"></a>Gauging System Health

"What can I observe about SpikeGLX to look for performance issues?"

- Open the Windows Task Manager and watch network performance. The
usage graph runs at something like 15.3% to 15.5% and is pretty steady.
Steady is good, while varying throughput means the system is struggling.

- The main Console window status bar, **when writing files**, displays the
current Xilinx card buffer fill percentage, which should remain below
5%, and it shows the disk write rate which should be around 1500+ MB/s.
If these are changing the system is struggling.

SpikeGLX doesn’t write data files with gaps. Rather, if any resources
are choked beyond a monitored threshold the run is stopped gracefully.
There are some specific monitoring messages that may appear in the text
of the console window…

- If data are not being pulled from the Xilinx card fast enough you’ll
start to see messages like this: **"IMEC FIFOQFill% value, …”** Again,
these are triggered if the fill % exceeds 5%.

- The data are placed into a central stream where graphs, file writing
and other services can get it. If that queue is backing up you’ll get
messages like: **"AIQ stream mem running low."**

- If a data service is bogging down fetching from the stream you’ll see:
**“AIQ::catBlocks failed.”** You might also see: **"GraphFetcher mem failure…"**

## <a name="how-to-report-bugs"></a>How to Report Bugs

If something unexpected happens while running SpikeGLX try to gather these
two files for diagnosis:

1. A screen shot that covers as much context as you can get, including any
Windows message box about the incident. You can make a screen shot by
pressing `shift + Print-Screen`. This saves a picture file to the clipboard.
You can then paste the picture into MS Paint and save that as a `jpg` image.

2. The Console window's log. If the program is still operable you can use
command `Tools/Save Log File...`

If the computer is hung so you can't save files, the next best thing is
to write down any error messages you see in dialog boxes and the Console
window.

## Nidq Settings

### <a name="wrong-sample-rate"></a>Wrong Sample Rate

"Suppose I enter the wrong sample rate in the `Samples/s` box on the
`Config dialog/NI Setup tab`. What happens?"

- Potential to exceed maximum rate of the nidq card (if not using Whisper).
- Wrong high-pass filter poles in {Graph window, Shank viewers, Spike trigger option}.
- Wrong time spans in triggers that specify wall time (refractory periods, recording spans,…}.
- Wrong “On Since” clock readouts (status bar, Graph Window toolbar).
- Wrong length set for in-memory history stream (used for peri-event file capture).
- Wrong overall memory footprint; too much memory degrades performance and may terminate run.
- Wrong metadata values recorded for nidq data offsets and spans (affects offline analysis).


_fin_

