using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Threading;

namespace DomeControl.Protocol.Tests
{
    /// <summary>
    /// The protocol layer against the real firmware ELF running under simavr: the same bytes the
    /// ASCOM driver would put on a COM port, through <see cref="DomeProtocol"/> unchanged.
    ///
    /// This is an end-to-end test of the protocol, not of the driver. It proves nothing about COM
    /// registration, the ASCOM Profile or the setup dialog, all of which need Windows (AGENTS.md §6).
    /// It runs on the same ELF the L3 checks use; when either is missing it is skipped, never
    /// reported as passed.
    /// </summary>
    internal static class E2EChecks
    {
        private static SimulatorProcess simulator;
        private static DomeProtocol dome;

        private static string RepositoryRoot()
        {
            var directory = new DirectoryInfo(AppContext.BaseDirectory);
            while (directory != null)
            {
                if (File.Exists(Path.Combine(directory.FullName, "firmware", "test", "sim", "harness.c")))
                {
                    return directory.FullName;
                }
                directory = directory.Parent;
            }
            throw new InvalidOperationException("cannot find the repository root from " + AppContext.BaseDirectory);
        }

        private static void EnsureStarted()
        {
            if (dome != null)
            {
                return;
            }
            string root = RepositoryRoot();
            string harness = Path.Combine(root, "firmware", "cmake-build-sim", "harness");
            string firmware = Path.Combine(root, "firmware", "cmake-build-avr", "DomeControl");
            foreach (string required in new[] { harness, firmware })
            {
                if (!File.Exists(required))
                {
                    throw new SkippedException("not built: " + Path.GetFileName(required) +
                                               " (tools/check.sh builds both)");
                }
            }
            simulator = SimulatorProcess.Start(harness, firmware);
            simulator.Settle();
            dome = new DomeProtocol(new TcpTransport(simulator.Port));
        }

        /// <summary>
        /// Wait until the firmware reports the motor stopped, and say how long that took.
        ///
        /// <c>ST#</c> only drops the ramp target: OCR1B steps down by <c>motorSpeedStepDown</c> per
        /// main-loop iteration, so the motor needs about a second of simulated time to actually stop.
        /// Asking <c>IM#</c> straight after <c>ST#</c> races that ramp and passes only when the host
        /// happens to be slow enough — this is what an ASCOM client does when it polls <c>Slewing</c>.
        /// </summary>
        private static bool WaitUntilStopped(DomeProtocol dome, TimeSpan timeout)
        {
            DateTime deadline = DateTime.UtcNow + timeout;
            while (DateTime.UtcNow < deadline)
            {
                if (!dome.IsMoving())
                {
                    return true;
                }
                Thread.Sleep(20);
            }
            return !dome.IsMoving();
        }

        public static IEnumerable<(string Name, Action Body)> All()
        {
            yield return ("the real firmware acknowledges a motion command", () =>
            {
                EnsureStarted();
                dome.GoForward();
                Assert.Equal(true, dome.IsMoving(), "IM# while moving");
                dome.MotorStop();
                Assert.Equal(true, WaitUntilStopped(dome, TimeSpan.FromSeconds(20)),
                    "IM# reports 0 after the ramp has finished");
            });

            yield return ("the encoder value comes back as a number", () =>
            {
                EnsureStarted();
                short value = dome.GetEncoderValue();
                Assert.That(value >= -100 && value <= 100,
                    string.Format(CultureInfo.InvariantCulture,
                                  "GEV# is within the +-100 travel range, got {0}", value));
            });

            yield return ("GoTo accepts a signed target", () =>
            {
                EnsureStarted();
                short here = dome.GetEncoderValue();
                dome.GoTo(here);
                Assert.Equal(true, WaitUntilStopped(dome, TimeSpan.FromSeconds(20)),
                    "the dome settles on a target it already holds");
            });

            yield return ("an unknown command is rejected without OK", () =>
            {
                EnsureStarted();
                DomeProtocolException error = Assert.Throws<DomeProtocolException>(
                    () => dome.SendCommandWithSimpleResp("XY"), "unknown command");
                Assert.That(error.Message.Contains("Unrecognized command"),
                    "the firmware's own wording reaches the caller: " + error.Message);
            });

            yield return ("the link recovers after a rejected command", () =>
            {
                EnsureStarted();
                Assert.Throws<DomeProtocolException>(() => dome.SendCommandWithSimpleResp("XY"),
                                                      "rejected");
                short value = dome.GetEncoderValue();
                Assert.That(value >= -100 && value <= 100, "GEV# still answers, got " + value);
            });

            yield return ("a frame of the documented maximum length is answered", () =>
            {
                EnsureStarted();
                // The firmware compares prefixes, so an over-long body is rejected as unknown rather
                // than truncated into a valid command.
                string overlong = new string('G', 62);
                DomeProtocolException error = Assert.Throws<DomeProtocolException>(
                    () => dome.SendCommandWithSimpleResp(overlong), "62-byte body plus terminator");
                Assert.That(error.Message.Contains("Unrecognized command"),
                    "rejected as unknown: " + error.Message);
            });

            yield return ("FindCenter is acknowledged and the dome reports its position", () =>
            {
                EnsureStarted();
                dome.FindCenter();
                Assert.Equal(true, WaitUntilStopped(dome, TimeSpan.FromSeconds(20)),
                    "the dome comes to rest");
                short value = dome.GetEncoderValue();
                Assert.That(value >= -100 && value <= 100, "GEV# after FC#, got " + value);
            });
        }

        /// <summary>Closes the simulator when the run is over; the runner calls this last.</summary>
        public static void Shutdown()
        {
            if (dome != null)
            {
                simulator.Dispose();
                dome = null;
                simulator = null;
            }
        }
    }
}